// Copyright 2009 Jack Christopher Kastorff

#ifndef _DIE_H_
#define _DIE_H_

#include <stdio.h>
#include <stdlib.h>

#define die(msg) \
    { \
    fprintf(stderr, "Died on %s line %u: %s\n", __FILE__, __LINE__, msg); \
    exit(EXIT_FAILURE); \
    }

#define diem(msg, extra) \
    { \
    fprintf(stderr, "Died on %s line %u: %s (%s)\n", __FILE__, __LINE__, msg, extra); \
    exit(EXIT_FAILURE); \
    }

#endif

