
# LZW Fuzzing Drivers

This directory contains short driver programs optimized for running the
lzw compression and decompression code through a fuzzer, specifically [AFL - American Fuzzy Lop](https://lcamtuf.coredump.cx/afl/).

## Prerequisites

You need to install AFL, or one of its forks.

The original AFL hasn't seen updates in a long time, but [AFL++](https://aflplus.plus/) should also work, but is untested at this time.

## Running

Simply run `make fuzz` or `make fuzz-<name>-driver` to build the corresponding driver and start AFL.

NOTE: It's recommended to run fuzzing on /tmp (or another RAM disk), since
AFL does a lot of disk writes.
