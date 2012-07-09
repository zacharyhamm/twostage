all:
	cc -g -Wall trust.c config.c twostage.c -o twostage -lrt -lcurl
