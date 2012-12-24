
CFLAGS += -O3 -ffast-math -Wall -g -std=c99 -pipe -I.

CFLAGS += `pkg-config --cflags sndfile`
LIBS += `pkg-config --libs sndfile`

LIBS += -lm

OBJECTS = convolute.o main.o readsoundfile.o

ifdef USE_FFTW3
CFLAGS += -DUSE_FFTW3 `pkg-config --cflags fftw3f`
LIBS += `pkg-config --libs fftw3f`
else
OBJECTS += kissfft/kiss_fft.o kissfft/kiss_fftr.o
CFLAGS += -Dkiss_fft_scalar=float
endif

.SUFFIXES: .c .o

all: convolute

convolute: $(OBJECTS)
	$(CC) $(LIBS) $(LDFLAGS) $(OBJECTS) -o convolute

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS)
	rm -f convolute

