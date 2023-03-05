@echo off

set compiler_flags=/nologo /Wall /W4 /std:c11 /Zi /D_CRT_SECURE_NO_WARNINGS
set linker_flags=/subsystem:console /opt:ref /incremental:no

if not exist build mkdir build

pushd build

cl %compiler_flags% ..\disasm8086.c /link %linker_flags%

popd
