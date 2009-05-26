// Copyright 2009 Jack Christopher Kastorff

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

//#include <complex.h>
#include <fftw3.h>
#include <sndfile.h>

#include "die.h"
#include "convolute.h"
#include "readsoundfile.h"

void convolute(char *inputpath, char *irpath, char *outputpath, double amp) {
    // read the input files
    soundfile * s_in = readsoundfile(inputpath);
    soundfile * s_ir = readsoundfile(irpath);

    // make sure all is well
    if ( s_in->samplerate != s_ir->samplerate )
        die("Sample rates of input and impulse response are different.");

    int fftlen = s_ir->length * 1.5 + 10000;
    // round up to a power of two
    int pow = 1;
    while ( fftlen > 2 ) { pow++; fftlen /= 2; }
    fftlen = 2 << pow;

    // make sure we're not wasting a bunch of memory on large chunk sizes
    // if the overlap will be wasted
    if ( fftlen > s_in->length + s_ir->length + 10 ) {
        fftlen = s_in->length + s_ir->length + 10;
    }

    int stepsize = fftlen - s_ir->length - 10;
    int steps = (s_in->length + stepsize - 1) / stepsize;

#ifdef SPEW
    fprintf(stderr, "fftlen is %d\ndoing %d steps of size %d\n", fftlen, steps, stepsize);
#endif

    fftw_plan p_fw, p_bw;
    fftw_complex *f_in, *f_out, *f_ir;
    double *outspace;

    // get some space for our temporary arrays
    if ( (f_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftlen)) == NULL )
        die("Couldn't malloc space for input fft");
    if ( (f_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftlen)) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (f_ir = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftlen)) == NULL )
        die("Couldn't malloc space for output fft");
    if ( (outspace = malloc(sizeof(double) * fftlen)) == NULL )
        die("Couldn't malloc space for outspace");

    // plan forward and plan backward
    p_fw = fftw_plan_dft_1d(fftlen, f_in, f_out, FFTW_FORWARD,  FFTW_ESTIMATE);
    p_bw = fftw_plan_dft_1d(fftlen, f_in, f_out, FFTW_BACKWARD, FFTW_ESTIMATE);

    // set up the output file
    SNDFILE *s_out;
    SF_INFO outinfo;

    outinfo.samplerate = s_in->samplerate;
    outinfo.channels   = 1;
    outinfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_24 | SF_ENDIAN_FILE;

    if ( (s_out = sf_open(outputpath, SFM_WRITE, &outinfo)) == NULL )
        die("Couldn't open output file for writing");

    // take the fft of the impulse response
    for (int i = 0; i < s_ir->length; i++) {
        f_in[i][0] = s_ir->data[i];
        f_in[i][1] = 0;
    }
    fftw_execute(p_fw);
    memcpy(f_ir, f_out, sizeof(fftw_complex) * fftlen);

    // initialize the outspace (TODO: memset?)
    for (int i = 0; i < fftlen; i++)
        outspace[i] = 0;

    // and go!
    int totalclipped = 0;
    double maxval = 0;
    for (int st = 0; st < steps; st++) {
        fprintf(stderr, "convoluting... %d/%d\r", st, steps);
        int start = st*stepsize;

        // first, copy the input into f_in
        for (int i = 0; i < fftlen; i++) {
            if ( i < stepsize && start+i < s_in->length ) {
                f_in[i][0] = s_in->data[start+i];
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
            sf_write_double(s_out, outspace, s_in->length - stepsize*(steps-1) + s_ir->length);
        }
        
        // and slide the outspace over, padding with zeroes
        // TODO: memmove
        for (int i = 0; i < fftlen; i++) {
            if ( i+stepsize >= fftlen ) {
                outspace[i] = 0;
            } else {
                outspace[i] = outspace[i+stepsize];
            }
        }
    }

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

    // close out the file
    sf_close(s_out);

    // clean up
    fftw_destroy_plan(p_fw);
    fftw_destroy_plan(p_bw);
    fftw_free(f_in);
    fftw_free(f_out);

    free(s_in->data);
    free(s_in);

    free(s_ir->data);
    free(s_ir);

    free(outspace);
}

