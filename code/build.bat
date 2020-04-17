@echo off

set CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4456 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 32-bit
REM cl %CommonCompilerFlags% ..\code\win32_handmade.cpp /link -subsystem:windows,5.1  %CommonLinkerFlags%

REM 64-bit
del *.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% -O2 -I..\..\iaca-win64\ -c ..\code\handmade_optimized.cpp -Fohandmade_optimized.obj -LD
cl %CommonCompilerFlags% -I..\..\iaca-win64\ ..\code\handmade.cpp handmade_optimized.obj -Fmhandmade.map -LD /link -incremental:no -opt:ref -PDB:handmade_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
del lock.tmp
cl %CommonCompilerFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
