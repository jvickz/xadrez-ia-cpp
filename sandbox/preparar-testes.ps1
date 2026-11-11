param([switch]$PrepararSomente)

$ErrorActionPreference = 'Stop'
$chessProject = Split-Path -Parent $PSScriptRoot
$chessStaging = Join-Path $env:LOCALAPPDATA 'XadrezIA-Sandbox'
$chessBinaries = @('gui-tests.exe', 'xadrez-tests.exe', 'xadrez-grafico-v3.exe')
foreach ($chessBinary in $chessBinaries) {
    if (-not (Test-Path -LiteralPath (Join-Path "$chessProject/build" $chessBinary))) {
        throw "Compile o projeto antes de testar. Arquivo ausente: $chessBinary"
    }
}

# Google Drive's virtual filesystem cannot be mapped by Windows Sandbox.
# Stage identical binaries on the local disk; expose inputs read-only.
foreach ($chessFolder in @('build', 'runner', 'results')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $chessStaging $chessFolder) | Out-Null
}
foreach ($chessBinary in $chessBinaries) {
    $chessSource = Join-Path "$chessProject/build" $chessBinary
    $chessTarget = Join-Path "$chessStaging/build" $chessBinary
    Copy-Item -LiteralPath $chessSource -Destination $chessTarget -Force
    if ((Get-FileHash -LiteralPath $chessSource).Hash -ne (Get-FileHash -LiteralPath $chessTarget).Hash) {
        throw "A cópia local diverge do original: $chessBinary"
    }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'executar-testes.cmd') -Destination "$chessStaging/runner/executar-testes.cmd" -Force
Write-Output "Arquivos preparados e hashes conferidos. Resultados: $chessStaging\results"

if (-not $PrepararSomente) {
    $chessSandbox = Join-Path $env:WINDIR 'System32/WindowsSandbox.exe'
    if (-not (Test-Path -LiteralPath $chessSandbox)) { throw 'Ative a Área Restrita do Windows e reinicie antes de continuar.' }
    $chessConfig = Join-Path $chessProject 'Testar-xadrez.wsb'
    # This launches the interactive test desktop for the user to see and hear.
    Start-Process -FilePath $chessSandbox -ArgumentList ('"' + $chessConfig + '"')
}
