twostage
========

An extremely simple implementation of two-stage authentication for *NIX systems.

This was really written to cut my teeth after a few years of hiatus from
hacking on unix, or anything else, for that matter.

But it really does work, and it serves a pretty commendable purpose.

Dependencies:

* A recent version of `libcurl`
* A library that provides `clock_gettime` (`-lrt` on a recent linux)

Installation:

1. `make`
2. Copy `twostage` somewhere like `/bin/`.
3. Add `/bin/twostage` to `/etc/shells`
4. `mkdir ~/.twostage` for the account you'll be adding twstage auth
   to.
5. Create a file in `.twostage` named `twostage.cfg`.
6. This file should consist of three lines, one after another:
 a. A TO line containing the email address for you SMS gateway.
 c. A MAIL line containing the path to your `mail` binary.
 d. A SHELL line containing the path to your preferred shell.
   (See twostage.cfg.example)
7. Make `/bin/twostage` your default shell.
8. And logout, log back in. 
