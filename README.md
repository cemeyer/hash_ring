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
        4 =     2.0e-01 (log: -2.3)
        8 =     2.0e-01 (log: -2.3)
        16 =    1.4e-01 (log: -2.8)
        32 =    1.3e-01 (log: -3.0)
        64 =    7.7e-02 (log: -3.7)
        128 =   5.2e-02 (log: -4.3)
        256 =   3.6e-02 (log: -4.8)
        512 =   2.5e-02 (log: -5.3)
    Evaluation hash 'DJB'
        4 =     5.1e-01 (log: -1.0)
        8 =     5.4e-01 (log: -0.9)
        16 =    5.6e-01 (log: -0.8)
        32 =    5.7e-01 (log: -0.8)
        64 =    5.8e-01 (log: -0.8)
        128 =   5.8e-01 (log: -0.8)
        256 =   5.8e-01 (log: -0.8)
        512 =   5.8e-01 (log: -0.8)
    Evaluation hash 'MH3_32'
        4 =     2.3e-01 (log: -2.1)
        8 =     1.8e-01 (log: -2.4)
        16 =    1.4e-01 (log: -2.9)
        32 =    9.2e-02 (log: -3.4)
        64 =    7.5e-02 (log: -3.7)
        128 =   5.0e-02 (log: -4.3)
        256 =   3.6e-02 (log: -4.8)
        512 =   2.6e-02 (log: -5.3)
    Evaluation hash 'MH3_128'
        4 =     2.6e-01 (log: -2.0)
        8 =     1.9e-01 (log: -2.4)
        16 =    1.5e-01 (log: -2.8)
        32 =    1.1e-01 (log: -3.1)
        64 =    7.7e-02 (log: -3.7)
        128 =   5.1e-02 (log: -4.3)
        256 =   3.6e-02 (log: -4.8)
        512 =   2.6e-02 (log: -5.3)
    Evaluation hash 'SHA1'
        4 =     1.5e-01 (log: -2.7)
        8 =     1.8e-01 (log: -2.5)
        16 =    1.3e-01 (log: -2.9)
        32 =    9.5e-02 (log: -3.4)
        64 =    7.1e-02 (log: -3.8)
        128 =   5.0e-02 (log: -4.3)
        256 =   3.7e-02 (log: -4.8)
        512 =   2.7e-02 (log: -5.2)

It appears that the DJB hash is fairly abysmal, but that stronger fast hashes
like MurmurHash3 (32- and 128-bit variants) perform no worse than
cryptographically secure hashes such as MD5 and SHA1 for key distribution with
as few as 8 replicas.
