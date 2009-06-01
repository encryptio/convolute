// Copyright 2009 Jack Christopher Kastorff

#ifndef __READSOUNDFILE_H__
#define __READSOUNDFILE_H__

typedef struct {
    double *data;
    int length;
    int samplerate;
} soundfile;

soundfile * readsoundfile(char *path);
soundfile * readsoundfilechunk(char *path, int start, int len);
int getsoundfilesamplerate(char *path);
int getsoundfilelength(char *path);

#endif

