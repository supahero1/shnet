#!/bin/bash
result=$(find . -name "in.*" -not -path "./.git/*")
for file in $result
do
  my_dirname=$(dirname $file)/
  my_basename=$(basename $file)
  my_filename=${my_basename:3}
  rm -f "${my_dirname}${my_filename}"
done
