@echo off
rem Builds Facet Browser for 64-bit and 32-bit foobar2000 and packages it as
rem dist\foo_facet_browser.fb2k-component (double-click it to install).
rem Needs Visual Studio 2022 or its Build Tools with the "Desktop development with C++" workload.
setlocal
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Build Tools not found. Install "Build Tools for Visual Studio 2022" with the C++ workload.
  exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
  echo MSBuild not found.
  exit /b 1
)
"%MSBUILD%" foo_facet_browser.sln /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 || exit /b 1
"%MSBUILD%" foo_facet_browser.sln /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x86 || exit /b 1

rem A .fb2k-component is a zip: the 32-bit DLL at the root, the 64-bit DLL in x64\
if exist dist\package rmdir /s /q dist\package
mkdir dist\package\x64
copy /y build\Release-Win32\foo_facet_browser.dll dist\package\ >nul
copy /y build\Release-x64\foo_facet_browser.dll dist\package\x64\ >nul
if exist dist\foo_facet_browser.fb2k-component del dist\foo_facet_browser.fb2k-component
rem Windows tar writes standard zip paths (x64/...), unlike Compress-Archive in Windows PowerShell 5
"%SystemRoot%\System32\tar.exe" -a -cf dist\foo_facet_browser.zip -C dist\package foo_facet_browser.dll x64 || exit /b 1
move /y dist\foo_facet_browser.zip dist\foo_facet_browser.fb2k-component >nul
rmdir /s /q dist\package
echo.
echo Built: dist\foo_facet_browser.fb2k-component
