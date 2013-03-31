hash\_ring
=========

Simple consistent hashing functionality in C; striving towards something
suitable for kernel-space use.

Derived heavily from StatHat's Golang implementation, with a few variations:
https://github.com/stathat/consistent

The original Golang implementation keeps a ring of string nodes. When a client
asks for appropriate node(s), it is given back those strings. This variation
instead tracks `uint32_t` nodes. This choice lets callers use their own
association between the 32-bit number space and whatever their nodes are (in
our case, probably drive numbers rather than strings).

Additionally, the original implementation's `Get()` family of functions took
strings (keys) and hashed them itself; we leave this to the caller. This
flexibility means callers are free to hash constant-sized structures, for
example. It also means that our `hash_ring_getn()` implementation must trust
the `hash` parameters to be uniformly distributed in the keyspace.

Using
-----

To run tests:

    make check

Otherwise, just use the sources as you see fit. This isn't really packaged
nicely as a library. Sorry.

Examples
--------

Using hash\_ring is pretty straightforward. Here is an example use case where
the user is caching values to a set of disks, which are numbered uniquely in
`uint32_t`. Disks may go down or come up. Stale reads will happen when some
data is on one disk ('A'), a second disk ('B') is added, the data's key now
associates to 'B', an overwrite happens and is written to 'B', and then 'B' is
removed from the live set and the data's key associates back to 'A' again.

```c
struct hash_ring live_drive_set;

hash_ring_init(&live_drive_set);

/* Initially, drives 1-3 are all live */
hash_ring_add(&live_drive_set, 0x01);
hash_ring_add(&live_drive_set, 0x02);
hash_ring_add(&live_drive_set, 0x03);

/*
 * ------------------------------------------------------------------------
 * Incoming cache writes in other contexts look for which drive(s) to write
 */
uint32_t my_drives[2];
int err;
uint32_t my_key_hash = hash(key);

err = hash_ring_getn(&live_drive_set, my_key_hash, 2, my_drives);
if (err != 0) {
    ...;
}
/* Write to drives my_drives[0] and my_drives[1] */

/*
 * -------------------------------------------------------------------
 * Incoming cache reads in other contexts look for which drive to read
 */
uint32_t my_drive;

err = hash_ring_getn(&live_drive_set, my_key_hash, 1, &my_drive);
if (err != 0) { ...; }
/* Read data from drive 'my_drive' */

/*
 * -------------------------------------------------------------------
 * A drive, #2, fails
 */
hash_ring_remove(&live_drive_set, 0x2);

/*
 * -------------------------------------------------------------------
 * A drive, #5, is added to the cache set
 */
hash_ring_add(&live_drive_set, 0x5);
```

Additionally, see the tests — t\_hashring.c.

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
