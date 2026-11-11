@echo off
setlocal EnableExtensions
rem This script is intended exclusively for the disposable Windows Sandbox.
if not exist C:\ChessBuild\gui-tests.exe exit /b 2
if not exist C:\ChessResults exit /b 2
set "runId=%RANDOM%-%RANDOM%"
set "runDir=%TEMP%\ChessValidation-%runId%"
mkdir "%runDir%\build"
copy /y C:\ChessBuild\gui-tests.exe "%runDir%\gui-tests.exe" >nul
if errorlevel 1 exit /b 2
copy /y C:\ChessBuild\xadrez-tests.exe "%runDir%\xadrez-tests.exe" >nul
if errorlevel 1 exit /b 2
copy /y C:\ChessBuild\xadrez-grafico-v3.exe "%runDir%\xadrez-grafico-v3.exe" >nul
if errorlevel 1 exit /b 2
cd /d "%runDir%"
xadrez-tests.exe >"C:\ChessResults\%runId%-regras.txt" 2>&1
set "rulesExit=%errorlevel%"
gui-tests.exe >"C:\ChessResults\%runId%-interface.txt" 2>&1
set "guiExit=%errorlevel%"
gui-tests.exe --write-save-test "%runDir%\persistencia.sav" >"C:\ChessResults\%runId%-salvar.txt" 2>&1
set "saveExit=%errorlevel%"
gui-tests.exe --read-save-test "%runDir%\persistencia.sav" >"C:\ChessResults\%runId%-reabrir.txt" 2>&1
set "loadExit=%errorlevel%"
gui-tests.exe --write-autosave-test "%runDir%\automatico.sav" >"C:\ChessResults\%runId%-autosave-fechar.txt" 2>&1
set "autoSaveExit=%errorlevel%"
gui-tests.exe --read-autosave-test "%runDir%\automatico.sav" >"C:\ChessResults\%runId%-autosave-reabrir.txt" 2>&1
set "autoLoadExit=%errorlevel%"
gui-tests.exe --audio-smoke >"C:\ChessResults\%runId%-audio.txt" 2>&1
set "audioExit=%errorlevel%"
>"C:\ChessResults\%runId%-resumo.txt" echo Testes executados dentro do Windows Sandbox.
>>"C:\ChessResults\%runId%-resumo.txt" echo Regras e IA: codigo %rulesExit%
>>"C:\ChessResults\%runId%-resumo.txt" echo Interface e eventos de som: codigo %guiExit%
>>"C:\ChessResults\%runId%-resumo.txt" echo Salvar e reabrir em processos separados: codigos %saveExit% / %loadExit%
>>"C:\ChessResults\%runId%-resumo.txt" echo Autosave com encerramento abrupto e reabertura: codigos %autoSaveExit% / %autoLoadExit%
>>"C:\ChessResults\%runId%-resumo.txt" echo Reproducao dos seis efeitos: codigo %audioExit%
>>"C:\ChessResults\%runId%-resumo.txt" echo Codigo 0 indica sucesso. Consulte os logs de cada etapa.
>>"C:\ChessResults\%runId%-resumo.txt" echo O teste de reproducao confirma que o Windows aceitou o audio; nao mede a saida dos alto-falantes.
start "" "%runDir%\xadrez-grafico-v3.exe"
if not "%rulesExit%"=="0" exit /b 1
if not "%guiExit%"=="0" exit /b 1
if not "%saveExit%"=="0" exit /b 1
if not "%loadExit%"=="0" exit /b 1
if not "%autoSaveExit%"=="0" exit /b 1
if not "%autoLoadExit%"=="0" exit /b 1
if not "%audioExit%"=="0" exit /b 1
exit /b 0
