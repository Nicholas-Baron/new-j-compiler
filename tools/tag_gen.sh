#!/bin/sh

# The first sed removes multiline comments
# The second removes single line comments
# The third removes include statements
# The fourth removes string literals

cat src/* \
    | sed -r ':a; s%(.*)/\*.*\*/%\1%; ta; /\/\*/ !b; N; ba' \
    | sed 's/\/\/.*//g' | sed 's/#.*//g' | tr "'" '"' | sed 's/".*"//g' \
    | tr 'A-Z' 'a-z' | tr "\"'(){}[]\+\-\*/%=&;!:.<>,0-9" ' ' > tags.txt
