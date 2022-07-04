#!/bin/bash
#
# Script that verifies that our reference files decompress and recompress to the exact original bitstream.
# These files can not be redistributed though.
#
DATA=tests
make
test "$(sha256sum -b ${DATA}/levelmp1.map | cut -f 1 -d ' ')" = "8b57ee1e373c926182e47afd3d97477c07f98ad6dde076cdf3c3f703f250d46c" || (echo "Invalid input: hash mismatch for levelmp1.map" && exit 1) || exit 1
test "$(sha256sum -b ${DATA}/extra.puz | cut -f 1 -d ' ')" = "f397e1e1b58d02ca6f469c8af0f5e50620621f267f48cb71af545f77d550607a" || (echo "Invalid input: hash mismatch for extra.puz" && exit 1) || exit 1
echo "Input verified OK."
./lzw-eddy -d ${DATA}/levelmp1.map -o .decomp1
test "$(sha256sum -b .decomp1 | cut -c1-64)" = "7183a6828de608f69563bff78eec25f9c86053d0ab1e36c6f998853717292f5c" || (echo "Decompression error: hash mismatch for levelmp1.decompressed" && exit 1) || exit 1
./lzw-eddy -c .decomp1 -o .comp1 >/dev/null
test "$(sha256sum -b .comp1)" = "8b57ee1e373c926182e47afd3d97477c07f98ad6dde076cdf3c3f703f250d46c *.comp1" || (echo "Compression error: hash mismatch for levelmp1.map" && exit 1) || exit 1
./lzw-eddy -d ${DATA}/extra.puz -o .decomp1
test "$(sha256sum -b .decomp1 | cut -c1-64)" = "d1ce310a2496af792c0c527b86c7e0dc17c14e8eb508d3472a894845e15b1c99" || (echo "Decompression error: hash mismatch for extra.decompressed" && exit 1) || exit 1
./lzw-eddy -c .decomp1 -o .comp1 >/dev/null
test "$(sha256sum -b .comp1)" = "f397e1e1b58d02ca6f469c8af0f5e50620621f267f48cb71af545f77d550607a *.comp1" || (echo "Compression error: hash mismatch for extra.puz" && exit 1) || exit 1
echo "All Good!"
rm .decomp1 .comp1
