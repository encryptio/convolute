// Copyright 2009 Jack Christopher Kastorff

#include "readsoundfile.h"
#include "die.h"

#include <sndfile.h>

soundfile * readsoundfile(char *path) {
    soundfile * ret;
    SF_INFO info;
    SNDFILE *snd;

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        diem("Couldn't open sound file for reading", path);

    if ( info.channels != 1 )
        diem("Sound file has more than one channel", path);

    if ( (ret = malloc(sizeof(*ret))) == NULL )
        die("Couldn't malloc space for soundfile");

    if ( (ret->data = malloc(sizeof(double)*info.frames)) == NULL )
        die("Couldn't malloc space for sound buffer");

    sf_read_double(snd, ret->data, info.frames); // assumption: channel count is 1, verified above

    ret->length = info.frames;
    ret->samplerate = info.samplerate;

    if ( sf_close(snd) )
        diem("Couldn't close sound file", path);

    return ret;
}

soundfile * readsoundfilechunk(char *path, int start, int len) {
    soundfile * ret;
    SF_INFO info;
    SNDFILE *snd;

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        diem("Couldn't open sound file for reading", path);

    if ( info.channels != 1 )
        diem("Sound file has more than one channel", path);

    if ( (ret = malloc(sizeof(*ret))) == NULL )
        die("Couldn't malloc space for soundfile");

    if ( (ret->data = malloc(sizeof(double)*len)) == NULL )
        die("Couldn't malloc space for sound buffer");

    sf_seek(snd, start, SEEK_SET);
    int actuallen = sf_read_double(snd, ret->data, len); // assumption: channel count is 1, verified above

    ret->length = actuallen;
    ret->samplerate = info.samplerate;

    if ( sf_close(snd) )
        diem("Couldn't close sound file", path);

    return ret;
}

int getsoundfilelength(char *path) {
    SF_INFO info;
    SNDFILE *snd;

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        diem("Couldn't open a sound file for reading", path);

    if ( info.channels != 1 )
        diem("A sound file has more than one channel", path);

    int ret = info.frames;

    if ( sf_close(snd) )
        diem("Couldn't close sound file", path);

    return ret;
}

int getsoundfilesamplerate(char *path) {
    SF_INFO info;
    SNDFILE *snd;

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        diem("Couldn't open a sound file for reading", path);

    int ret = info.samplerate;

    if ( sf_close(snd) )
        diem("Couldn't close sound file", path);

    return ret;
}

