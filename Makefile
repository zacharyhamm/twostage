# Comment the line below out for FreeBSD/OpenBSD/NetBSD/Darwin
#DEFINES=-D_BSD_
DEFINES=
LIBS=

all:
	cc -g -Wall trust.c config.c twostage.c -o twostage $(LIBS) $(DEFINES)
