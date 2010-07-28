
CFLAGS += -O3 -ffast-math -Wall -g -std=c99 -pipe -I.

CFLAGS += `pkg-config --cflags sndfile fftw3`
LIBS += `pkg-config --libs sndfile fftw3`

OBJECTS = convolute.o main.o readsoundfile.o

.SUFFIXES: .c .o

all: convolute

convolute: $(OBJECTS)
	$(CC) $(LIBS) $(LDFLAGS) $(OBJECTS) -o convolute

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS)
	rm -f convolute

