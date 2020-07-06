#!/bin/bash
#e.g. ./sync.sh

root=${WUKONG_ROOT}/tests/

if [ "$root" = "/" ] ;
then
	echo  "PLEASE set WUKONG_ROOT"
	exit 0
fi

cat mpd.hosts | while read machine
do
	#Don't copy things like Makefile, CMakeFiles, etc in build directory.
	rsync -rtuvl --include=network/build/network* --exclude=network/build/* --exclude=.git   ${root} ${machine}:${root}
done
