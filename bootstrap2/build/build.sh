rm -f compiler
gcc -g -rdynamic -o compiler $(find ./.. -name "*.c")