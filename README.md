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

    Region size error; lower is better.
    # replicas:     4                       8                       16                      32
    DJB             6.0e-01 (log: -0.7)     6.3e-01 (log: -0.7)     6.5e-01 (log: -0.6)     6.6e-01 (log: -0.6)
    MD5             3.0e-01 (log: -1.7)     2.0e-01 (log: -2.3)     1.5e-01 (log: -2.8)     1.0e-01 (log: -3.3)
    SHA1            2.9e-01 (log: -1.8)     2.4e-01 (log: -2.1)     1.5e-01 (log: -2.7)     1.1e-01 (log: -3.2)
    MH3_32          2.4e-01 (log: -2.0)     1.8e-01 (log: -2.5)     1.3e-01 (log: -3.0)     1.1e-01 (log: -3.2)
    MH3_128         2.0e-01 (log: -2.3)     1.6e-01 (log: -2.6)     1.1e-01 (log: -3.1)     1.1e-01 (log: -3.2)
    isi32           3.6e-01 (log: -1.5)     2.0e-01 (log: -2.3)     1.5e-01 (log: -2.8)     1.1e-01 (log: -3.2)
    isi64           2.7e-01 (log: -1.9)     2.1e-01 (log: -2.2)     1.3e-01 (log: -3.0)     9.6e-02 (log: -3.4)
    
    Distribution error (lower is better):
    DJB             0.33                    0.33                    0.33                    0.33
    MD5             0.13                    0.15                    0.11                    0.07
    SHA1            0.16                    0.11                    0.12                    0.10
    MH3_32          0.27                    0.19                    0.08                    0.13
    MH3_128         0.20                    0.25                    0.12                    0.09
    isi32           0.31                    0.15                    0.22                    0.12
    isi64           0.24                    0.09                    0.11                    0.04

It appears that the DJB hash is fairly abysmal, but that stronger fast hashes
like MurmurHash3 (32- and 128-bit variants) or isi_hash32 and isi_hash64
perform no worse than cryptographically secure hashes such as MD5 and SHA1 for
key distribution with as few as 8 replicas.
