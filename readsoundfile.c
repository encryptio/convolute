/*
 * Copyright (C) 2009 Jack Christopher Kastorff <encryptio@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

    memset(&info, 0, sizeof(info));

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

