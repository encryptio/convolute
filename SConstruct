import os

env = Environment( ENV = os.environ )
env.Append( CCFLAGS=['-Wall', '-O3', '-ffast-math', '-std=c99'] )
env.Append( CPPPATH=['.'] )

env.ParseConfig('pkg-config --cflags --libs sndfile fftw3')

env.Program("convolute", ["convolute.c", "main.c", "readsoundfile.c"])

