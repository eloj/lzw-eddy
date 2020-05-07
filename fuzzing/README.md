
# Decompression Fuzzing Driver

This directory contains a short driver program optimized for running the
decompression code through a fuzzer, specifically [AFL - American Fuzzy Loop](https://lcamtuf.coredump.cx/afl/).

## Prerequisites

You need to install AFL, specifically you need `afl-gcc`.

## Running

Simply run `make fuzz` to build the driver and and start afl.

NOTE: It's recommended to run fuzzing on /tmp (or another RAM disk), since
AFL does a lot of disk writes.
