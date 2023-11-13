@echo off
SETLOCAL

SET COMPILER_FLAGS=/Zi /TC /I".\third_party\include" /I".\third_party\src"
SET LINKER_FLAGS=/LIBPATH:".\third_party\lib" sdl2main.lib sdl2.lib kernel32.lib shell32.lib /subsystem:console

IF NOT EXIST .\build\ mkdir .\build\

cl %COMPILER_FLAGS% src\main.c third_party\src\gl.c third_party\src\lib.c third_party\tree-sitter-c\src\parser.c /Fe.\build\editor.exe /link %LINKER_FLAGS%

IF %ERRORLEVEL% NEQ 0 (
    echo Compilation failed!
    EXIT /B %ERRORLEVEL%
)

copy .\third_party\lib\SDL2.dll .\build\

echo Compilation successful!

ENDLOCAL
