/*
 * Copyright (c) 2009-2010 Jack Christopher Kastorff <encryptio@gmail.com>
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_FFTW3
//#include <complex.h>
#include <fftw3.h>
#else
#include "kissfft/kiss_fftr.h"
#endif

#include <sndfile.h>

#include "die.h"
#include "convolute.h"
#include "readsoundfile.h"

// this number directly corresponds to the memory usage and inversely corresponds to running time and number of passes
#define IMPULSE_CHUNK_MAXLEN 1638400

#define TEMPORARY_SUFFIX ".convolute-temp"

static void addconvolute(char *inputpath, char *irpath, char *addpath, char *outputpath, float amp, int extradelay) {
    // open the input path for reading
    SF_INFO snd_in_info;
    SNDFILE *snd_in;

    memset(&snd_in_info, 0, sizeof(snd_in_info));

    if ( (snd_in = sf_open(inputpath, SFM_READ, &snd_in_info)) == NULL )
        die("Couldn't open a sound file for reading");

    if ( snd_in_info.channels != 1 )
        die("The input sound file has more than one channel");

    int snd_in_len = snd_in_info.frames;

    // extract the chunk of the ir we need
    soundfile *ir = readsoundfilechunk(irpath, extradelay, IMPULSE_CHUNK_MAXLEN);

    if ( snd_in_info.samplerate != ir->samplerate )
        die("Sample rates of input and impulse response are different.");

    int fftlen = ir->length * 1.5 + 10000;
    // round up to a power of two
    int pow = 1;
    while ( fftlen > 2 ) { pow++; fftlen /= 2; }
    fftlen = 2 << pow;

    // make sure we're not wasting a bunch of memory on large chunk sizes
    // if the overlap will be wasted
    if ( fftlen > snd_in_len + ir->length + 10 ) {
        fftlen = snd_in_len + ir->length + 10;
    }

    int stepsize = fftlen - ir->length - 10;
    int steps = (snd_in_len + stepsize - 1) / stepsize;

#ifdef SPEW
    fprintf(stderr, "fftlen is %d\ndoing %d steps of size %d\n", fftlen, steps, stepsize);
#endif

#ifdef USE_FFTW3
    fftwf_plan p_fw, p_bw;
    fftwf_complex *f_out, *f_ir;
#else
    kiss_fftr_cfg cfg_fw, cfg_bw;
    kiss_fft_cpx *f_out, *f_ir;
#endif
    float *revspace, *outspace, *inspace;

    // get some space for our temporary arrays
#ifdef USE_FFTW3
    if ( (f_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (fftlen/2+1))) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (f_ir = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (fftlen/2+1))) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (revspace = fftwf_malloc(sizeof(float) * fftlen)) == NULL )
        die("Couldn't malloc space for output scalars");
#else
    if ( (f_out = KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * (fftlen/2+1))) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (f_ir = KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * (fftlen/2+1))) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (revspace = KISS_FFT_MALLOC(sizeof(float) * fftlen)) == NULL )
        die("Couldn't malloc space for output scalars");
#endif

    if ( (outspace = malloc(sizeof(float) * fftlen)) == NULL )
        die("Couldn't malloc space for outspace");
    if ( (inspace = malloc(sizeof(float) * fftlen)) == NULL )
        die("Couldn't malloc space for inspace");

    // plan forward and plan backward
#ifdef USE_FFTW3
    p_fw = fftwf_plan_dft_r2c_1d(fftlen, inspace,  f_out, FFTW_ESTIMATE);
    p_bw = fftwf_plan_dft_c2r_1d(fftlen, f_out, revspace, FFTW_ESTIMATE);
#else
    cfg_fw = kiss_fftr_alloc(fftlen, 0, NULL, NULL);
    cfg_bw = kiss_fftr_alloc(fftlen, 1, NULL, NULL);
#endif


    // set up the input add file
    SNDFILE *s_add = NULL;
    SF_INFO addinfo;

    memset(&addinfo, 0, sizeof(addinfo));

    if ( addpath ) {
        if ( (s_add = sf_open(addpath, SFM_READ, &addinfo)) == NULL )
            die("Couldn't open additive file for reading");
    }

    // set up the output file
    SNDFILE *s_out;
    SF_INFO outinfo;

    memset(&outinfo, 0, sizeof(outinfo));

    outinfo.samplerate = snd_in_info.samplerate;
    outinfo.channels   = 1;
    outinfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_24 | SF_ENDIAN_FILE;

    if ( (s_out = sf_open(outputpath, SFM_WRITE, &outinfo)) == NULL )
        die("Couldn't open output file for writing");

    // copy the first few bytes of the add to the output, if we're delayed
    if ( addpath ) {
        int tocopy = extradelay;
        while ( tocopy > 0 ) {
            if ( tocopy > fftlen ) {
                sf_read_float(s_add, outspace, fftlen);
                sf_write_float(s_out, outspace, fftlen);
                tocopy -= fftlen;
            } else {
                sf_read_float(s_add, outspace, tocopy);
                sf_write_float(s_out, outspace, tocopy);
                tocopy -= tocopy;
            }
        }
    } else {
        int tocopy = extradelay;
        for (int i = 0; i < fftlen; i++)
            outspace[i] = 0;
        while ( tocopy > 0 ) {
            if ( tocopy > fftlen ) {
                sf_write_float(s_out, outspace, fftlen);
                tocopy -= fftlen;
            } else {
                sf_write_float(s_out, outspace, tocopy);
                tocopy -= tocopy;
            }
        }
    }

    // take the fft of the impulse response
#ifdef USE_FFTW3
    for (int i = 0; i < fftlen; i++)
        inspace[i] = i < ir->length ? ir->data[i] : 0;
    fftwf_execute(p_fw);
    memcpy(f_ir, f_out, sizeof(fftwf_complex) * (fftlen/2+1));
#else
    kiss_fftr(cfg_fw, ir->data, f_ir);
#endif

    // initialize the outspace
    if ( addpath ) {
        sf_read_float(s_add, outspace, fftlen);
    } else {
        for (int i = 0; i < fftlen; i++)
            outspace[i] = 0;
    }

    // initialize the inspace
    for (int i = 0; i < fftlen; i++)
        inspace[i] = 0;

    // and go!
    int totalclipped = 0;
    float maxval = 0;
    for (int st = 0; st < steps; st++) {
        fprintf(stderr, "convoluting... %d/%d\033[K\r", st+1, steps);
        int start = st*stepsize;

        // read some amount from the input file
        int readlength = stepsize;
        if ( start + readlength > snd_in_len )
            readlength = snd_in_len - start;
        sf_read_float(snd_in, inspace, readlength);

        // clean up the trailing part of inspace if it's the last step
        if ( start+fftlen > snd_in_len )
            for (int i = readlength+1; i < fftlen; i++)
                inspace[i] = 0;

        // take the fft
#ifdef USE_FFTW3
        fftwf_execute(p_fw);
#else
        kiss_fftr(cfg_fw, inspace, f_out);
#endif

        // multiply by f_ir
        for (int i = 0; i < fftlen/2+1; i++) {
#ifdef USE_FFTW3
            float re = f_ir[i][0]*f_out[i][0] - f_ir[i][1]*f_out[i][1];
            float im = f_ir[i][1]*f_out[i][0] + f_ir[i][0]*f_out[i][1];
            f_out[i][0] = re;
            f_out[i][1] = im;
#else
            float re = f_ir[i].r*f_out[i].r - f_ir[i].i*f_out[i].i;
            float im = f_ir[i].i*f_out[i].r + f_ir[i].r*f_out[i].i;
            f_out[i].r = re;
            f_out[i].i = im;
#endif
        }

        // take the inverse fft
#ifdef USE_FFTW3
        fftwf_execute(p_bw);
#else
        kiss_fftri(cfg_bw, f_out, revspace);
#endif

        // add the resulting chunk to the outspace (normalizing it as we go)
        for (int i = 0; i < fftlen; i++)
            outspace[i] += f_out[i][0]/fftlen * amp;

        // get some clipping statistics
        for (int i = 0; i < stepsize; i++) {
            if ( fabs(outspace[i]) > maxval )
                maxval = fabs(outspace[i]);
            if ( fabs(outspace[i]) > 1 ) {
                totalclipped++;
                if ( outspace[i] > 0 ) {
                    outspace[i] = 1;
                } else {
                    outspace[i] = -1;
                }
            }
        }

        // write out the part we're done with
        if ( st < steps-1 ) {
            sf_write_float(s_out, outspace, stepsize);
        } else {
            // write out the last step - it's smaller than the rest
            sf_write_float(s_out, outspace, snd_in_len - stepsize*(steps-1) + ir->length);
        }
        
        // and slide the outspace over
        // TODO: memmove
        for (int i = 0; i < fftlen-stepsize; i++) {
            outspace[i] = outspace[i+stepsize];
        }

        // finally, initialize the newly opened up space with more data from the add file
        // append with zeroes if we're already past the end of the add file
        int got = 0;
        if ( addpath )
            got = sf_read_float(s_add, &outspace[fftlen-stepsize], stepsize);
        for (int i = fftlen-stepsize+got; i<fftlen; i++)
            outspace[i] = 0;
    }

    fprintf(stderr, "\r\033[K");

    // finalize the clipping statistics
    for (int i = 0; i < fftlen; i++) {
        if ( fabs(outspace[i]) > maxval )
            maxval = fabs(outspace[i]);
        if ( fabs(outspace[i]) > 1 ) {
            totalclipped++;
            if ( outspace[i] > 0 ) {
                outspace[i] = 1;
            } else {
                outspace[i] = -1;
            }
        }
    }

    // and tell the user about them, if neccessary
    if ( totalclipped ) {
        fprintf(stderr, "WARNING: %d samples got clipped!\n", totalclipped);
        fprintf(stderr, "Recommend a multipler of less than %f instead\n", amp/maxval);
#ifndef SPEW
        fprintf(stderr, "maximum amplitude: %f\n", maxval);
#endif
    }
#ifdef SPEW
    fprintf(stderr, "maximum amplitude: %f\n", maxval);
#endif

    // clean up
    sf_close(s_out);

#ifdef USE_FFTW3
    fftwf_destroy_plan(p_fw);
    fftwf_destroy_plan(p_bw);
    fftwf_free(f_out);
    fftwf_free(f_ir);
    fftwf_free(revspace);
#else
    kiss_fftr_free(cfg_fw);
    kiss_fftr_free(cfg_bw);
    KISS_FFT_FREE(f_ir);
    KISS_FFT_FREE(f_out);
    KISS_FFT_FREE(revspace);
#endif

    free(ir->data);
    free(ir);

    sf_close(snd_in);
    if ( addpath )
        sf_close(s_add);

    free(outspace);
    free(inspace);
}

void killfile(char *path) {
    if ( access(path, F_OK) == 0 ) {
        if ( unlink(path) )
            die("Couldn't unlink existing file");
    }
}

void convolute(char *inputpath, char *irpath, char *outputpath, float amp) {
    char *newpath;

    if ( (newpath = malloc(strlen(outputpath)+strlen(TEMPORARY_SUFFIX))) == NULL )
        die("Couldn't malloc space for newpath");

    strcpy(newpath, outputpath);
    strcpy(&newpath[strlen(outputpath)], TEMPORARY_SUFFIX);

    killfile(outputpath);
    killfile(newpath);

    int irlen = getsoundfilelength(irpath);
    int inlen = getsoundfilelength(inputpath);

    if ( irlen > inlen ) {
#ifdef SPEW
        fprintf(stderr, "swapping ir and in\n");
#endif
        return convolute(irpath, inputpath, outputpath, amp);
    }

    int passes = (int) ceil( (double)irlen / IMPULSE_CHUNK_MAXLEN );

    int i = 0;
    int irat = 0;
    while ( irat < irlen ) {
        if ( passes > 1 )
            fprintf(stderr, "pass %d/%d\033[K\n", i+1, passes);

        addconvolute(inputpath, irpath, i == 0 ? NULL : outputpath, newpath, amp, irat);
        irat += IMPULSE_CHUNK_MAXLEN;
        rename(newpath, outputpath);

        i++;
    }
}

