#!/bin/sh
# Build the Debian package in a Debian trixie container. The .deb is
# written to ./dist.
set -e

cd "$(dirname "$0")"
mkdir -p dist

docker run --rm -v "$PWD":/src:ro -v "$PWD/dist":/dist debian:trixie sh -ec '
	apt-get update -qq
	DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends debhelper dpkg-dev >/dev/null
	cp -a /src /build
	cd /build
	rm -rf dist
	dpkg-buildpackage -us -uc -b -d
	cp ../*.deb /dist/
'
ls -l dist
