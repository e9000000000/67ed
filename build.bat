@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "MODE=%~1"
if /I "%MODE%"=="clean" (
    if exist build rmdir /s /q build
    if exist third_party\scintilla\win32\obj rmdir /s /q third_party\scintilla\win32\obj
    if exist third_party\scintilla\bin\libscintilla.lib del /q third_party\scintilla\bin\libscintilla.lib
    echo Cleaned.
    exit /b 0
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Install Visual Studio Build Tools.
    exit /b 1
)

set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo ERROR: Visual C++ Build Tools not found.
    exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo ERROR: vcvars64.bat failed.
    exit /b 1
)

if not exist build mkdir build
if not exist build\obj mkdir build\obj

if not exist build\scintilla.lib (
    echo Building Scintilla...
    if not exist build\scintilla-obj mkdir build\scintilla-obj
    cl /nologo /c /std:c++17 /EHsc /W4 /MP /FS /MT /O2 /GL /utf-8 /DNDEBUG ^
       /DDISABLE_D2D /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
       /Ithird_party\scintilla\include /Ithird_party\scintilla\src ^
       /Fdbuild\scintilla-obj\scintilla.pdb /Fobuild\scintilla-obj\ ^
       third_party\scintilla\src\*.cxx ^
       third_party\scintilla\win32\HanjaDic.cxx ^
       third_party\scintilla\win32\PlatWin.cxx ^
       third_party\scintilla\win32\ListBox.cxx ^
       third_party\scintilla\win32\SurfaceGDI.cxx ^
       third_party\scintilla\win32\SurfaceD2D.cxx ^
       third_party\scintilla\win32\ScintillaWin.cxx
    if errorlevel 1 (
        echo ERROR: Scintilla compilation failed.
        exit /b 1
    )
    lib /nologo /OUT:build\scintilla.lib build\scintilla-obj\*.obj
    if errorlevel 1 (
        echo ERROR: Scintilla library failed.
        exit /b 1
    )
)

if not exist build\lexilla.lib (
    echo Building Lexilla...
    if not exist build\lexilla-obj mkdir build\lexilla-obj
    cl /nologo /c /std:c++17 /EHsc /W3 /MP /FS /MT /O2 /GL /utf-8 /DNDEBUG ^
       /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
       /Ithird_party\scintilla\include /Ithird_party\lexilla\include /Ithird_party\lexilla\lexlib ^
       /Fdbuild\lexilla-obj\lexilla.pdb /Fobuild\lexilla-obj\ ^
       third_party\lexilla\lexlib\*.cxx third_party\lexilla\lexers\*.cxx
    if errorlevel 1 (
        echo ERROR: Lexilla compilation failed.
        exit /b 1
    )
    lib /nologo /OUT:build\lexilla.lib build\lexilla-obj\*.obj
    if errorlevel 1 (
        echo ERROR: Lexilla library failed.
        exit /b 1
    )
)

echo Compiling resources...
rc /nologo /fo build\app.res resources\app.rc
if errorlevel 1 (
    echo ERROR: resource compilation failed.
    exit /b 1
)

set "CFLAGS=/nologo /std:c++20 /EHsc /W4 /permissive- /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Ithird_party\scintilla\include /Ithird_party\scintilla\src /Ithird_party\lexilla\include /Ithird_party\lexilla\lexlib"
set "LDFLAGS=/nologo /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /LTCG /MAP:build\67ed.map"
set "LIBS=build\lexilla.lib build\scintilla.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib advapi32.lib dwmapi.lib uxtheme.lib comctl32.lib imm32.lib shlwapi.lib psapi.lib"

if /I "%MODE%"=="debug" (
    set "CFLAGS=%CFLAGS% /Od /Zi /MT /DDEBUG"
    set "LDFLAGS=%LDFLAGS% /DEBUG"
    set "OUTPUT=build\67ed-debug.exe"
) else (
    set "CFLAGS=%CFLAGS% /O2 /GL /MT /DNDEBUG"
    set "OUTPUT=build\67ed.exe"
)

echo Compiling 67ed...
cl %CFLAGS% /Fobuild\obj\ src\*.cpp /Fe:%OUTPUT% /link %LDFLAGS% build\app.res %LIBS%
if errorlevel 1 (
    echo ERROR: 67ed build failed.
    exit /b 1
)

echo.
echo Built %OUTPUT%
for %%F in ("%OUTPUT%") do echo Size: %%~zF bytes
exit /b 0
