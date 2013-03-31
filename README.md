hash\_ring
=========

Simple consistent hashing functionality in C; striving towards something
suitable for kernel-space use.

Derived heavily from StatHat's Golang implementation:
https://github.com/stathat/consistent

Using
-----

To run tests:

    make check

Otherwise, just use the sources as you see fit. This isn't really packaged
nicely as a library. Sorry.

Examples
--------

See the tests -- t\_hashring.c.
