// Copyright 2009 Jack Christopher Kastorff

#include "readsoundfile.h"
#include "die.h"

#include <sndfile.h>

soundfile * readsoundfile(char *path) {
    soundfile * ret;
    SF_INFO info;
    SNDFILE *snd;

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        die("Couldn't open a sound file for reading");

    if ( info.channels != 1 )
        die("A sound file has more than one channel");

    if ( (ret = malloc(sizeof(*ret))) == NULL )
        die("Couldn't malloc space for soundfile");

    if ( (ret->data = malloc(sizeof(double)*info.frames)) == NULL )
        die("Couldn't malloc space for sound buffer");

#ifdef SPEW
    fprintf(stderr, "Reading %d samples from %s...\n", (int)info.frames, path);
#endif
    sf_read_double(snd, ret->data, info.frames); // assumption: channel count is 1, verified above

    ret->length = info.frames;
    ret->samplerate = info.samplerate;

    if ( sf_close(snd) )
        die("Couldn't close played file");

    return ret;
}

