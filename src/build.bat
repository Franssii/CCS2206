@echo off
setlocal

rem Ustaw ponizej wlasciwa sciezke do pobranego i wypakowanego SFML (wersja dla MSVC)
set SFML_DIR=C:\PROGRAMOWANIE\CPP\SFML

rem Flagi kompilatora (C++17, obsluga wyjatkow, dodanie sciezki include SFML)
set CXXFLAGS=/std:c++17 /EHsc /I"%SFML_DIR%\include"

rem Flagi linkera (dodanie sciezki lib SFML oraz podpiecie modulow uzywanych w kodzie)
set LDFLAGS=/link /LIBPATH:"%SFML_DIR%\lib" sfml-graphics.lib sfml-window.lib sfml-system.lib sfml-audio.lib

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul

echo Kompilowanie zrodel...
cl.exe %CXXFLAGS% *.cpp %LDFLAGS% /Fe:game.exe

if %errorlevel% == 0 (
    echo Kompilacja zakonczona sukcesem! Uruchom plik Game.exe.
) else (
    echo Blad kompilacji. Sprawdz czy sciezka do SFML jest poprawna.
)
endlocal