twostage
========

An extremely simple implementation of two-stage authentication for *NIX systems.

This was really written to cut my teeth after a few years of hiatus from
hacking on unix, or anything else, for that matter.

But it really does work, and it serves a pretty commendable purpose.

Installation
------------

1. `make`
2. Copy `twostage` somewhere like `/bin/`.
3. Add `/bin/twostage` to `/etc/shells`
4. `mkdir ~/.twostage` for the account you'll be adding twstage auth
   to.
5. Create a file in `.twostage` named `twostage.cfg`.
6. This file should consist of three lines, one after another (See `twostage.cfg.example`):
    1. A TO line containing the email address for you SMS gateway.
    2. A MAIL line containing the path to your `mail` binary.
    3. A SHELL line containing the path to your preferred shell.
7. Make `/bin/twostage` your default shell.
8. And logout, log back in. 

If you have any comments drop me a line at zsh AT imipolexg DOT org

(GPG key coming soon)
