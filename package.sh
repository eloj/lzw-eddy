#!/bin/bash
#
# Create a release package for a specific OS.
#
PROJECT=lzw-eddy
OS=$1
ARCH=${2:-`uname -m`}
BP="packages/${OS}"
RP="${BP}/${PROJECT}"
FILES='lzw-eddy lzw.h LICENSE'

if [ -z "${OS}" ]; then
	echo "Usage: $0 <operating-system>"
	exit 1
fi

if [[ "${OS}" == "linux" || "${OS}" == "msys2" ]]; then
	echo "Packaging for ${OS}-${ARCH}"
	OPTIMIZED=1 make -B lzw-eddy
	if [ $? -ne 0 ]; then
		echo "Build failed, packaging aborted."
		exit 1
	fi
	mkdir -p ${RP}
	cp ${FILES} ${RP}
	tar -C ${BP} -czvf packages/${PROJECT}.${OS}-${ARCH}.tar.gz ${PROJECT}
	if [ "${OS}" == "msys2" ]; then
		pushd .
		cd ${RP} && 7z -bd a ../../${PROJECT}.${OS}-${ARCH}.7z *
		popd
	fi
	rm -r ${BP}
else
	echo "Unknown target OS: ${OS}"
	exit 2
fi

echo "Done."
