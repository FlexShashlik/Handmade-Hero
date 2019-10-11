@echo off

echo - - - - - -
echo - - - - - -

set wildcard=*.cpp *.h *.inl *.c

echo STATICS FOUND:
findstr -s -n -i -l "static" * %wildcard%

echo - - - - - -
echo - - - - - -

echo GLOBALS FOUND:
findstr -s -n -i -l "local_persist" * %wildcard%
findstr -s -n -i -l "global_variable" * %wildcard%

echo - - - - - -
echo - - - - - -
