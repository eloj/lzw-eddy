
# LZW Fuzzing Driver

This directory contains short driver programs optimized for running the
lzw compression and decompression code through a fuzzer, specifically [AFL - American Fuzzy Loop](https://lcamtuf.coredump.cx/afl/).

## Prerequisites

You need to install AFL, specifically you need `afl-gcc`.

## Running

Simply run `make fuzz-comp` or `make fuzz-decomp` to build the corresponding driver and start afl.

NOTE: It's recommended to run fuzzing on /tmp (or another RAM disk), since
AFL does a lot of disk writes.
