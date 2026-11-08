@echo off
setlocal
pushd "%~dp0.."
if not exist "bin\ChapterSkipTests" mkdir "bin\ChapterSkipTests"
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /Fo:bin\ChapterSkipTests\ /Fe:bin\ChapterSkipTests\ChapterSkipTests.exe src\mpc-hc\tests\ChapterSkipTests.cpp
if errorlevel 1 goto failed
bin\ChapterSkipTests\ChapterSkipTests.exe
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
