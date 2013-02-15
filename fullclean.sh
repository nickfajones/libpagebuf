#!/bin/sh

for a in `cat .gitignore`
do
  rm -rf ./$a
done
