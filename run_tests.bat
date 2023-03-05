@echo off

set nasm_binary="C:\Program Files\NASM\nasm.exe"

if not exist testbench mkdir testbench

pushd testbench

%nasm_binary% ..\tests\listing_0037_single_register_mov.asm -o listing_0037_single_register_mov
..\build\disasm8086.exe listing_0037_single_register_mov

echo[
echo[

%nasm_binary% ..\tests\listing_0038_many_register_mov.asm -o listing_0038_many_register_mov
..\build\disasm8086.exe listing_0038_many_register_mov

echo[
echo[

%nasm_binary% ..\tests\listing_0039_more_movs.asm -o listing_0039_more_movs
..\build\disasm8086.exe listing_0039_more_movs

echo[
echo[

rem %nasm_binary% ..\tests\listing_0040_challenge_movs.asm -o listing_0040_challenge_movs
rem ..\build\disasm8086.exe listing_0040_challenge_movs

popd
