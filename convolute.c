// Copyright 2009 Jack Christopher Kastorff

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

//#include <complex.h>
#include <fftw3.h>
#include <sndfile.h>

#include "die.h"
#include "convolute.h"
#include "readsoundfile.h"

// this number directly corresponds to the memory usage and inversely corresponds to running time and number of passes
#define IMPULSE_CHUNK_MAXLEN 409600

#define TEMPORARY_SUFFIX ".convolute-temp"

static void writezerofile(char *path, int len, int rate) {
    SNDFILE *snd;
    SF_INFO info;

    info.samplerate = rate;
    info.channels   = 1;
    info.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_24 | SF_ENDIAN_FILE;

    if ( (snd = sf_open(path, SFM_WRITE, &info)) == NULL )
        die("Couldn't open output file for writing");

    short *zeroes;

    if ( (zeroes = malloc(sizeof(short)*10240)) == NULL )
        die("Couldn't malloc space for zeroes");

    for (int i = 0; i < 10240; i++)
        zeroes[i] = 0;

    while ( len > 0 ) {
        if ( len > 10240 ) {
            sf_write_short(snd, zeroes, 10240);
            len -= 10240;
        } else {
            sf_write_short(snd, zeroes, len);
            len -= len;
        }
    }

    if ( sf_close(snd) )
        die("Couldn't close output sound file");

    free(zeroes);
}

static void addconvolute(char *inputpath, char *irpath, char *addpath, char *outputpath, double amp, int extradelay) {
    // open the input path for reading
    SF_INFO snd_in_info;
    SNDFILE *snd_in;

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

    fftw_plan p_fw, p_bw;
    fftw_complex *f_in, *f_out, *f_ir;
    double *outspace;
    double *inspace;

    // get some space for our temporary arrays
    if ( (f_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftlen)) == NULL )
        die("Couldn't malloc space for input fft");
    if ( (f_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftlen)) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (f_ir = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftlen)) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (outspace = malloc(sizeof(double) * fftlen)) == NULL )
        die("Couldn't malloc space for outspace");
    if ( (inspace = malloc(sizeof(double) * stepsize)) == NULL )
        die("Couldn't malloc space for inspace");

    // plan forward and plan backward
    p_fw = fftw_plan_dft_1d(fftlen, f_in, f_out, FFTW_FORWARD,  FFTW_ESTIMATE);
    p_bw = fftw_plan_dft_1d(fftlen, f_in, f_out, FFTW_BACKWARD, FFTW_ESTIMATE);

    // set up the input add file
    SNDFILE *s_add;
    SF_INFO addinfo;

    if ( (s_add = sf_open(addpath, SFM_READ, &addinfo)) == NULL )
        die("Couldn't open additive file for reading");

    // set up the output file
    SNDFILE *s_out;
    SF_INFO outinfo;

    outinfo.samplerate = snd_in_info.samplerate;
    outinfo.channels   = 1;
    outinfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_24 | SF_ENDIAN_FILE;

    if ( (s_out = sf_open(outputpath, SFM_WRITE, &outinfo)) == NULL )
        die("Couldn't open output file for writing");

    // copy the first few bytes of the add to the output, if we're delayed
    int tocopy = extradelay;
    while ( tocopy > 0 ) {
        if ( tocopy > fftlen ) {
            sf_read_double(s_add, outspace, fftlen);
            sf_write_double(s_out, outspace, fftlen);
            tocopy -= fftlen;
        } else {
            sf_read_double(s_add, outspace, tocopy);
            sf_write_double(s_out, outspace, tocopy);
            tocopy -= tocopy;
        }
    }

    // take the fft of the impulse response
    for (int i = 0; i < ir->length; i++) {
        f_in[i][0] = ir->data[i];
        f_in[i][1] = 0;
    }
    fftw_execute(p_fw);
    memcpy(f_ir, f_out, sizeof(fftw_complex) * fftlen);

    // initialize the outspace
    sf_read_double(s_add, outspace, fftlen);

    // and go!
    int totalclipped = 0;
    double maxval = 0;
    for (int st = 0; st < steps; st++) {
        fprintf(stderr, "convoluting... %d/%d\033[K\r", st, steps);
        int start = st*stepsize;

        // read some amount from the input file
        int readlength = stepsize;
        if ( start + readlength > snd_in_len )
            readlength = snd_in_len - start;
        sf_read_double(snd_in, inspace, readlength);

        // copy the input into f_in
        for (int i = 0; i < fftlen; i++) {
            if ( i < stepsize && start+i < snd_in_len ) {
                f_in[i][0] = inspace[i];
            } else {
                f_in[i][0] = 0;
            }
            f_in[i][1] = 0;
        }

        // take the fft
        fftw_execute(p_fw);

        // multiply by f_ir and put the result in f_in
        for (int i = 0; i < fftlen; i++) {
            f_in[i][0] = f_ir[i][0]*f_out[i][0] - f_ir[i][1]*f_out[i][1];
            f_in[i][1] = f_ir[i][1]*f_out[i][0] + f_ir[i][0]*f_out[i][1];
        }

        // take the inverse fft
        fftw_execute(p_bw);

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
            sf_write_double(s_out, outspace, stepsize);
        } else {
            // write out the last step - it's smaller than the rest
            sf_write_double(s_out, outspace, snd_in_len - stepsize*(steps-1) + ir->length);
        }
        
        // and slide the outspace over
        // TODO: memmove
        for (int i = 0; i < fftlen-stepsize; i++) {
            outspace[i] = outspace[i+stepsize];
        }

        // finally, initialize the newly opened up space with more data from the add file
        // append with zeroes if we're already past the end of the add file
        int got = sf_read_double(s_add, &outspace[fftlen-stepsize], stepsize);
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

    fftw_destroy_plan(p_fw);
    fftw_destroy_plan(p_bw);
    fftw_free(f_in);
    fftw_free(f_out);

    sf_close(snd_in);

    free(outspace);
    free(inspace);
}

void killfile(char *path) {
    if ( access(path, F_OK) == 0 ) {
        if ( unlink(path) )
            die("Couldn't unlink existing file");
    }
}

void convolute(char *inputpath, char *irpath, char *outputpath, double amp) {
    char *newpath;

    if ( (newpath = malloc(strlen(outputpath)+strlen(TEMPORARY_SUFFIX))) == NULL )
        die("Couldn't malloc space for newpath");

    strcpy(newpath, outputpath);
    strcpy(&newpath[strlen(outputpath)], TEMPORARY_SUFFIX);

    killfile(outputpath);
    killfile(newpath);

    int irlen = getsoundfilelength(irpath);
    int inlen = getsoundfilelength(inputpath);

    int insr = getsoundfilesamplerate(inputpath);

    writezerofile(outputpath, irlen+inlen, insr);

    int passes = (int) ceil( (double)irlen / IMPULSE_CHUNK_MAXLEN );

    int i = 0;
    int irat = 0;
    while ( irat < irlen ) {
        if ( passes > 1 )
            fprintf(stderr, "pass %d/%d\033[K\n", i++, passes);

        addconvolute(inputpath, irpath, outputpath, newpath, amp, irat);
        irat += IMPULSE_CHUNK_MAXLEN;
        rename(newpath, outputpath);
    }
}

