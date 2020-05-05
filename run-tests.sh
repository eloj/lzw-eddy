#!/bin/bash
TMPFILE=$(tempfile)
trap "{ rm $TMPFILE; }" EXIT
set -e
./lzw-eddy -d tests/abra.txt.lzw -o $TMPFILE && test "$(sha256sum $TMPFILE)" = "3119a48c6843ee7dcc08312e97b1d8e3b241b082996afe761f8a045d493b7cef  $TMPFILE" || (echo "Test failed." && exit 1)
echo "Passed."
