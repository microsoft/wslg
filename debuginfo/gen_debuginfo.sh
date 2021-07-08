#!/bin/bash

function split_debuginfo(){
    file=${1//[$'\t\r\n']}
    dir=$(dirname "$file")
    mkdir -p $2/debuginfo/$dir
    org=$2/$file
    dst=$2/debuginfo/$file.debug
    objcopy --only-keep-debug $org $dst
    objcopy --strip-debug $org
    objcopy --add-gnu-debuglink=$dst $org
    echo $dst
}
while read line; do split_debuginfo $line $2; done < $1

