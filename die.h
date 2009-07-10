/*
 * Copyright (c) 2009 Chris Kastorff
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name Chris Kastorff may not be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 */

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

