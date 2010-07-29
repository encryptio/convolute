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

#include "readsoundfile.h"
#include "die.h"

#include <sndfile.h>

#include <string.h>

soundfile * readsoundfile(char *path) {
    soundfile * ret;
    SF_INFO info;
    SNDFILE *snd;

    memset(&info, 0, sizeof(info));

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        diem("Couldn't open sound file for reading", path);

    if ( info.channels != 1 )
        diem("Sound file has more than one channel", path);

    if ( (ret = malloc(sizeof(*ret))) == NULL )
        die("Couldn't malloc space for soundfile");

    if ( (ret->data = malloc(sizeof(float)*info.frames)) == NULL )
        die("Couldn't malloc space for sound buffer");

    sf_read_float(snd, ret->data, info.frames); // assumption: channel count is 1, verified above

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

    memset(&info, 0, sizeof(info));

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        diem("Couldn't open sound file for reading", path);

    if ( info.channels != 1 )
        diem("Sound file has more than one channel", path);

    if ( (ret = malloc(sizeof(*ret))) == NULL )
        die("Couldn't malloc space for soundfile");

    if ( (ret->data = malloc(sizeof(float)*len)) == NULL )
        die("Couldn't malloc space for sound buffer");

    sf_seek(snd, start, SEEK_SET);
    int actuallen = sf_read_float(snd, ret->data, len); // assumption: channel count is 1, verified above

    ret->length = actuallen;
    ret->samplerate = info.samplerate;

    if ( sf_close(snd) )
        diem("Couldn't close sound file", path);

    return ret;
}

int getsoundfilelength(char *path) {
    SF_INFO info;
    SNDFILE *snd;

    memset(&info, 0, sizeof(info));

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

    memset(&info, 0, sizeof(info));

    if ( (snd = sf_open(path, SFM_READ, &info)) == NULL )
        diem("Couldn't open a sound file for reading", path);

    int ret = info.samplerate;

    if ( sf_close(snd) )
        diem("Couldn't close sound file", path);

    return ret;
}

