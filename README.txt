B-Minor Phase 1
Sahel Sabeti - 40113407

Requirements: a C++17 compiler such as g++ or clang++.

Compile:
g++ -std=c++17 main.cpp -o bminor

Run from this folder:
./bminor tests/valid.bminor

Generated files in the current folder:
highlighted.html   colored source, open in a browser
ast.txt            parsed syntax tree
tokens.txt         token list with source positions
errors.txt         lexical and syntax errors, if any

The colored source is also printed to the terminal with ANSI colors.
Open highlighted.html to see color distinctions between function names,
variable names, types, keywords, literals, and comments.

Other input examples:
./bminor tests/simple.bminor
./bminor tests/invalid.bminor

The program returns 0 for a valid input and 2 when there are errors.
Grammar rules and token patterns are in grammar.ebnf.


Testing:
Run the following commands to check valid and invalid inputs:

./bminor tests/valid.bminor
./bminor tests/invalid.bminor

Check tokens.txt, ast.txt, and errors.txt after each run.