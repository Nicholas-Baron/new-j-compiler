#!/bin/sh

for file in ../src/*; do
    file_size=$(g++ -E "$file" | wc -l)
    echo "$file_size $file"
done
