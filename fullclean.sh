#!/bin/bash

homedir=$PWD

for dir in . pagebuf test; do
  cd $dir
  if [ ! -e .gitignore ]; then
    cd $homedir
    continue
  fi
  while read -r line; do
    if [[ "$line" =~ ^# ]]; then
      continue
    fi
    if [[ -z "$line" ]]; then
      continue
    fi
    rm -rf ./$line
  done <.gitignore
  cd $homedir
done
