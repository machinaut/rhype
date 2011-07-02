#!/bin/bash
#
# Touch all the buildsystem files to force full rebuild
#

for infile in $(find . | grep ".in$")
do
	touch $infile
done
