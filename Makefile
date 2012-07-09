LIBS=-lrt

all:
	cc -g -Wall trust.c config.c twostage.c -o twostage $(LIBS)
