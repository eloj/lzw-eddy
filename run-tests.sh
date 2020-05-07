#!/bin/bash
TMPFILE=$(tempfile)
trap "{ rm $TMPFILE; }" EXIT
set -e
./lzw-eddy -d tests/atsign.lzw -o $TMPFILE && test "$(sha256sum $TMPFILE)" = "c3641f8544d7c02f3580b07c0f9887f0c6a27ff5ab1d4a3e29caf197cfc299ae  $TMPFILE" || (echo "Test failed." && exit 1)
./lzw-eddy -d tests/abra.txt.lzw -o $TMPFILE && test "$(sha256sum $TMPFILE)" = "3119a48c6843ee7dcc08312e97b1d8e3b241b082996afe761f8a045d493b7cef  $TMPFILE" || (echo "Test failed." && exit 1)
./lzw-eddy -d tests/zeros80000.lzw -o $TMPFILE && test "$(sha256sum $TMPFILE)" = "f8c784aa6b57396e7c5e094c34d079d8252473e46e2f60593a921dbebf941fcc  $TMPFILE" || (echo "Test failed." && exit 1)
echo "Passed."
