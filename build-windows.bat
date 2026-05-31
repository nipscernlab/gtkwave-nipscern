@echo off
setlocal enableextensions enabledelayedexpansion
title GTKWave (nipscern) - Build Windows

REM ============================================================
REM  GTKWave (nipscern) - Build para Windows (MSYS2 / MINGW64)
REM
REM  Gera o gtkwave.exe SEM precisar de WSL, tudo nativo.
REM  Na primeira execucao instala o MSYS2 (via winget) e o
REM  toolchain GTK3/GTK4 + meson automaticamente.
REM
REM  Uso:
REM    build-windows.bat            -> build incremental + install
REM    build-windows.bat clean      -> apaga build/ e refaz do zero
REM    build-windows.bat run        -> build + instala + roda o gtkwave
REM
REM  Override opcional (variaveis de ambiente):
REM    GTKWAVE_MSYS2    = caminho do MSYS2 (ex: D:\msys64)
REM    GTKWAVE_PREFIX   = onde instalar  (default C:/packs/gtkwave-bin)
REM ============================================================

set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

REM Garante que o cmd (e o bash filho) rodem a partir da raiz do projeto.
cd /d "%PROJECT_DIR%"

if not defined GTKWAVE_PREFIX set "GTKWAVE_PREFIX=C:/packs/gtkwave-bin"

REM ---- argumento (clean / run) ----
set "INNERARG="
set "DORUN="
if /i "%~1"=="clean" set "INNERARG=clean"
if /i "%~1"=="run"   set "DORUN=1"

REM ============================================================
REM  1) Localizar (ou instalar) o MSYS2
REM ============================================================
set "MSYS="
if defined GTKWAVE_MSYS2 if exist "%GTKWAVE_MSYS2%\usr\bin\bash.exe" set "MSYS=%GTKWAVE_MSYS2%"
if not defined MSYS if exist "C:\msys64\usr\bin\bash.exe"        set "MSYS=C:\msys64"
if not defined MSYS if exist "C:\packs\msys64\usr\bin\bash.exe"  set "MSYS=C:\packs\msys64"

if not defined MSYS (
  echo.
  echo [GTKWave] MSYS2 nao encontrado. Instalando via winget...
  echo           ^(download grande na primeira vez, ~100+ MB^)
  where winget >nul 2>&1
  if errorlevel 1 (
    echo.
    echo ERRO: 'winget' nao esta disponivel neste sistema.
    echo Instale o MSYS2 manualmente em https://www.msys2.org e rode de novo.
    pause & exit /b 1
  )
  winget install -e --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements
  if exist "C:\msys64\usr\bin\bash.exe" set "MSYS=C:\msys64"
)

if not defined MSYS (
  echo ERRO: nao consegui localizar/instalar o MSYS2.
  pause & exit /b 1
)
echo [GTKWave] MSYS2: %MSYS%

REM ============================================================
REM  2) Ambiente MINGW64 + caminho unix do projeto
REM ============================================================
set "MSYSTEM=MINGW64"
set "CHERE_INVOKING=1"
set "BASH=%MSYS%\usr\bin\bash.exe"

REM ============================================================
REM  3) Build (toolchain + meson) dentro do MINGW64
REM     O bash herda o diretorio atual (CWD = raiz do projeto, via
REM     cd /d acima). CHERE_INVOKING=1 impede o profile de trocar de
REM     pasta, entao 'tools/msys2-build.sh' resolve direitinho.
REM ============================================================
echo [GTKWave] Iniciando build em: %PROJECT_DIR%
"%BASH%" -lc "bash tools/msys2-build.sh %INNERARG%"
if errorlevel 1 (
  echo.
  echo ============================================================
  echo  BUILD FALHOU. Veja as mensagens acima.
  echo ============================================================
  pause & exit /b 1
)

echo.
echo ============================================================
echo  BUILD OK
echo  EXE:      %GTKWAVE_PREFIX%/bin/gtkwave.exe
echo  Launcher: C:\packs\gtkwave-bin\gtkwave.cmd
echo ============================================================

REM ============================================================
REM  4) Rodar (opcional)
REM ============================================================
if defined DORUN (
  echo [GTKWave] Iniciando o gtkwave...
  if exist "C:\packs\gtkwave-bin\gtkwave.cmd" (
    call "C:\packs\gtkwave-bin\gtkwave.cmd" --dark
  ) else (
    echo AVISO: launcher nao encontrado, abrindo o exe direto ^(pode faltar DLL^).
    set "PATH=%MSYS%\mingw64\bin;%PATH%"
    start "" "C:/packs/gtkwave-bin/bin/gtkwave.exe" --dark
  )
)

endlocal
