@echo off

echo - - - - - -
echo - - - - - -

set wildcard=*.cpp *.h *.inl *.c

echo TODOS FOUND:
findstr -s -n -i -l "TODO" * %wildcard%

echo - - - - - -
echo - - - - - -
