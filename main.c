// Copyright 2009 Jack Christopher Kastorff

#include <convolute.h>
#include <die.h>

int main(int argc, char **argv) {
    if ( argc != 5 )
        die("Bad number of arguments. Usage: deconvolute input impulse output amp");

    convolute(argv[1], argv[2], argv[3], atof(argv[4]));
}

