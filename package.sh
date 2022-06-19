#!/bin/bash
#
# Create a release package for a specific OS.
#
OS=$1
ARCH=${2:-`uname -m`}
BP="packages/${OS}"
RP="${BP}/lzw-eddy"
FILES='lzw-eddy lzw.h LICENSE'

if [ -z "${OS}" ]; then
	echo "Usage: $0 <operating-system>"
	exit 1
fi

if [[ "${OS}" == "linux" || "${OS}" == "windows" ]]; then
	echo "Packaging for ${OS}-${ARCH}"
	OPTIMIZED=1 make -B lzw-eddy
	mkdir -p ${RP}
	cp ${FILES} ${RP}
	tar -C ${BP} -czvf packages/lzw-eddy.${OS}-${ARCH}.tar.gz lzw-eddy
	rm -r ${BP}
else
	echo "Unknown target OS: ${OS}"
	exit 2
fi

echo "Done."
