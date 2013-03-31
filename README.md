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

Key distribution
----------------

A desirable property of a hash function used for a consistent hash
implementation is that the key space is evenly divided. I have attempted to
measure that; here are my results:

    Key distribution error; lower is better.
    Evaluation hash 'MD5'
        4 =     30.00
        8 =     29.39
        16 =    28.79
        32 =    28.64
        64 =    27.93
        128 =   27.65
        256 =   27.21
        512 =   26.69
    Evaluation hash 'DJB'
        4 =     31.55
        8 =     31.60
        16 =    31.63
        32 =    31.64
        64 =    31.64
        128 =   31.65
        256 =   31.64
        512 =   31.65
    Evaluation hash 'MH3_32'
        4 =     30.38
        8 =     29.92
        16 =    29.14
        32 =    28.42
        64 =    28.07
        128 =   27.70
        256 =   27.22
        512 =   26.68
    Evaluation hash 'MH3_128'
        4 =     30.45
        8 =     30.16
        16 =    29.21
        32 =    28.66
        64 =    28.20
        128 =   27.73
        256 =   27.17
        512 =   26.71
    Evaluation hash 'SHA1'
        4 =     30.39
        8 =     29.31
        16 =    29.20
        32 =    28.65
        64 =    28.27
        128 =   27.67
        256 =   27.24
        512 =   26.71

It appears that the DJB hash is fairly abysmal, but that stronger fast hashes
like MurmurHash3 (32- and 128-bit variants) perform no worse than
cryptographically secure hashes such as MD5 and SHA1 for key distribution.
