#!/bin/bash
TMPFILED=$(mktemp)
TMPFILEC=$(mktemp)
trap "{ rm $TMPFILED $TMPFILEC; }" EXIT
set -e

rep() {
	C=${1:-0}
	S=${2:-A}
	if [ "$C" -gt 0 ]; then
		printf "%.0s$S" $(seq 1 $C)
	fi
}

function testfile {
	INFILE=$1
	EOPT=$2
	HASHD=$3
	HASHC=${4:-$(sha256sum -b $INFILE | cut -f 1 -d ' ')}
	./lzw-eddy -d $INFILE -o $TMPFILED
	if [ $? != 0 ]; then
		echo "TEST FAIL. (Decompression error)"
		exit 1
	fi
	test "$(sha256sum -b $TMPFILED)" = "$HASHD *$TMPFILED" || (echo "Test failed -- Decompressed hash mismatch." && exit 1)
	./lzw-eddy $EOPT -c $TMPFILED -o $TMPFILEC
	if [ $? != 0 ]; then
		echo "TEST FAIL. (Compression error)"
		exit 1
	fi
	test "$(sha256sum -b $TMPFILEC)" = "$HASHC *$TMPFILEC" || (echo "Test failed. -- Compressed hash mismatch" && exit 1)
}

function testcheck {
	INFILE=$1
	HASHD=$(sha256sum $INFILE | cut -f 1 -d ' ')
	./lzw-eddy -c $INFILE -o $TMPFILEC
	./lzw-eddy -d $TMPFILEC -o $TMPFILED
	HASHC=$(sha256sum $TMPFILED | cut -f 1 -d ' ')
	test "$HASHD" = "$HASHC" || (echo "Test failed. -- Compressed hash mismatch" && exit 1)
}

testfile tests/atsign.lzw "" c3641f8544d7c02f3580b07c0f9887f0c6a27ff5ab1d4a3e29caf197cfc299ae
testfile tests/abra.txt.lzw "" 3119a48c6843ee7dcc08312e97b1d8e3b241b082996afe761f8a045d493b7cef
testfile tests/zeros80000.lzw "" f8c784aa6b57396e7c5e094c34d079d8252473e46e2f60593a921dbebf941fcc
testfile tests/zeros80000.lzw "-m 254" f8c784aa6b57396e7c5e094c34d079d8252473e46e2f60593a921dbebf941fcc 58e6c0321a62f7f112e61dca779edc9be0e34cf0ee356f61df949c7aea839492
testcheck lzw.h
rep 65536 AaA >$TMPFILED
testcheck $TMPFILED
echo "All tests passed."
