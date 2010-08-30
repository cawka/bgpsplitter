#!/bin/bash

url=$1
list=`curl -l $url`

SRC=original
DST=filtered

for file in $list; do
	echo
	echo ====================================================================
	wget -O $SRC/$file "$url/$file"

	bgpsplitter -i $SRC/$file -o $DST/$file
done

