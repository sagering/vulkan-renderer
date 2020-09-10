echo off
mkdir build
for %%x in (main.vert main.frag compose.vert compose.frag) do tools\glslangValidator.exe -V res\shaders\%%x -o build\%%x.spv"
pause