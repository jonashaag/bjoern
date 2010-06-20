#!/bin/bash

function clone {
    git clone git://github.com/ry/http-parser
}

function update {
    git pull
}

cd include

if [ ! -d http-parser ]; then
    clone
    cd http-parser
else
    cd http-parser
    update
fi

cd ../..
