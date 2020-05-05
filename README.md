
# Simple LZW (Lempel-Ziv-Welch) Decompressor

A basic headerless LZW decompressor. Supports variable length codes
between 9 and 12 bits.

This is similar to what's used by GIF, but slightly less flexible.

The algorithm implemented by this code was widely distributed in the
old days in places like Dr.Dobbs and a popular book on compression,
which resulted in it being used in [all sorts of places](https://www.giantbomb.com/profile/eloj/blog/technical-notes-on-the-level-format-of-puzznic-for/114881/).

All code is provided under the [MIT License](LICENSE).

[![Build Status](https://travis-ci.org/eloj/lzw-eddy.svg?branch=master)](https://travis-ci.org/eloj/lzw-eddy)
