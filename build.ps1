param(
    [string]$Compiler,
    [switch]$Test,
    [switch]$BuildTests,
    [string]$GuiOutput = 'xadrez-grafico-v3.exe'
)

$ErrorActionPreference = 'Stop'
if (-not $Compiler) {
    foreach ($name in @('g++', 'clang++', 'zig')) {
        $command = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue
        if ($command) {
            $Compiler = $command.Source
            break
        }
    }
    if (-not $Compiler) { throw 'Informe -Compiler com o caminho para g++, clang++ ou zig.' }
}

$compilerArgs = @()
if ([IO.Path]::GetFileNameWithoutExtension($Compiler) -eq 'zig') { $compilerArgs += 'c++' }
$compilerArgs += @('-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic')

Push-Location -LiteralPath $PSScriptRoot
try {
    New-Item -ItemType Directory -Force -Path 'build' | Out-Null
    if ([IO.Path]::GetFileNameWithoutExtension($Compiler) -eq 'zig') {
        $chessResource = 'build/app.res'
        & $Compiler rc /fo $chessResource app.rc
    } else {
        $chessWindres = Get-Command windres -CommandType Application -ErrorAction SilentlyContinue
        if (-not $chessWindres) { throw 'Informe Zig como compilador ou instale windres para incorporar o ícone.' }
        $chessResource = 'build/app-resource.o'
        & $chessWindres.Source app.rc -O coff -o $chessResource
    }
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar os recursos do ícone.' }
    & $Compiler @compilerArgs main.cpp chess.cpp ai.cpp -o build/xadrez.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o jogo.' }
    & $Compiler @compilerArgs gui.cpp chess.cpp ai.cpp $chessResource '-Wl,--subsystem,windows' -lgdi32 -luser32 -lgdiplus -lwinmm -o (Join-Path 'build' $GuiOutput)
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar a interface gráfica.' }
    if ($Test -or $BuildTests) {
        & $Compiler @compilerArgs tests.cpp chess.cpp ai.cpp -o build/xadrez-tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar os testes.' }
        & $Compiler @compilerArgs gui_tests.cpp chess.cpp ai.cpp $chessResource -lgdi32 -luser32 -lgdiplus -lwinmm -o build/gui-tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar os testes da interface.' }
    }
    if ($Test) {
        & ./build/xadrez-tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes.' }
        & ./build/gui-tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes da interface.' }
    }
} finally {
    Pop-Location
}
