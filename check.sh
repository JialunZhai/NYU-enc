#!/bin/bash

rm -f ./grading/nyuenc.tar.xz

tar cvJf nyuenc.tar.xz Makefile nyuenc.cpp

cp nyuenc.tar.xz ./grading/nyuenc.tar.xz

cd ./grading/

./autograder.sh