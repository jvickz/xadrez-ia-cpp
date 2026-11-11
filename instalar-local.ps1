param()

$ErrorActionPreference = 'Stop'
$chessSourceExe = Join-Path $PSScriptRoot 'build/xadrez-grafico-v3.exe'
$chessSourceIcon = Join-Path $PSScriptRoot 'assets/xadrez.ico'
$chessInstall = Join-Path $env:LOCALAPPDATA 'Programs/XadrezIA'
$chessLocalExe = Join-Path $chessInstall 'XadrezIA-v3.3.exe'
$chessPreviousExe = Join-Path $chessInstall 'XadrezIA-v3.2.exe'
$chessEarlierExe = Join-Path $chessInstall 'XadrezIA-v3.1.exe'
$chessLegacyExe = Join-Path $chessInstall 'XadrezIA.exe'
$chessLocalIcon = Join-Path $chessInstall 'xadrez.ico'
foreach ($chessSource in @($chessSourceExe, $chessSourceIcon)) {
    if (-not (Test-Path -LiteralPath $chessSource -PathType Leaf)) { throw "Arquivo ausente: $chessSource" }
}

$chessDesktopLink = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Xadrez com IA - com sons.lnk'
$chessMenuLink = Join-Path ([Environment]::GetFolderPath('Programs')) 'Xadrez com IA.lnk'
$chessShell = New-Object -ComObject WScript.Shell
# Do not overwrite an unrelated shortcut that happens to use the same name.
foreach ($chessLinkPath in @($chessDesktopLink, $chessMenuLink)) {
    if (Test-Path -LiteralPath $chessLinkPath) {
        $chessOldLink = $chessShell.CreateShortcut($chessLinkPath)
        if ($chessOldLink.TargetPath -notin @($chessSourceExe, $chessLocalExe, $chessPreviousExe, $chessEarlierExe, $chessLegacyExe)) {
            throw "O atalho existente aponta para outro programa: $chessLinkPath"
        }
    }
}

New-Item -ItemType Directory -Force -Path $chessInstall | Out-Null
$chessSourceHash = (Get-FileHash -LiteralPath $chessSourceExe -Algorithm SHA256).Hash
if (-not (Test-Path -LiteralPath $chessLocalExe) -or
    (Get-FileHash -LiteralPath $chessLocalExe -Algorithm SHA256).Hash -ne $chessSourceHash) {
    Copy-Item -LiteralPath $chessSourceExe -Destination $chessLocalExe -Force
}
Copy-Item -LiteralPath $chessSourceIcon -Destination $chessLocalIcon -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets/logo.png') -Destination (Join-Path $chessInstall 'logo.png') -Force
if ((Get-FileHash -LiteralPath $chessLocalExe -Algorithm SHA256).Hash -ne $chessSourceHash) {
    throw 'A verificação da cópia instalada falhou.'
}

foreach ($chessLinkPath in @($chessDesktopLink, $chessMenuLink)) {
    $chessLink = $chessShell.CreateShortcut($chessLinkPath)
    $chessLink.TargetPath = $chessLocalExe
    $chessLink.WorkingDirectory = $chessInstall
    $chessLink.Arguments = ''
    $chessLink.IconLocation = "$chessLocalIcon,0"
    $chessLink.Description = 'Xadrez contra a IA — jogo offline com sons, animações e desfazer.'
    $chessLink.WindowStyle = 1
    $chessLink.Save()
    $chessVerified = $chessShell.CreateShortcut($chessLinkPath)
    if ($chessVerified.TargetPath -ne $chessLocalExe -or $chessVerified.IconLocation -ne "$chessLocalIcon,0") {
        throw "Não foi possível verificar o atalho: $chessLinkPath"
    }
}

@{
    Product = 'Xadrez com IA'
    Version = '3.3'
    InstalledAt = (Get-Date).ToString('o')
    Executable = 'XadrezIA-v3.3.exe'
    SHA256 = $chessSourceHash
    Icon = 'xadrez.ico'
    Offline = $true
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $chessInstall 'installation.json') -Encoding UTF8
@'
Xadrez com IA — versão 3.3 com salvamento automático

Abra XadrezIA-v3.3.exe ou o atalho da área de trabalho/menu Iniciar.
Esta instalação funciona offline e não depende do Google Drive.
Os sons e os desenhos das peças fazem parte do executável.
O botão Som no topo liga ou desliga os efeitos.
A logo está incorporada ao executável e aparece na janela, Alt+Tab e barra de tarefas.
O ícone próprio do atalho também está em xadrez.ico.
Cada jogada sua e da IA é salva automaticamente, assim como Desfazer e Nova partida.
Você pode fechar sem clicar em Salvar partida. O botão e Ctrl+S continuam disponíveis.
Ao reabrir, clique em Continuar. Há um espaço de salvamento, atualizado automaticamente.
A partida fica em %LOCALAPPDATA%\XadrezIA\partida.sav, fora da pasta de instalação.
Abrir o jogo preserva o arquivo salvo. Nova partida substitui a partida anterior.
Se a gravação falhar, um aviso aparece em vermelho; o último arquivo salvo é preservado.
'@ | Set-Content -LiteralPath (Join-Path $chessInstall 'LEIA-ME.txt') -Encoding UTF8

[pscustomobject]@{InstallDirectory=$chessInstall;Executable=$chessLocalExe;DesktopShortcut=$chessDesktopLink;StartMenuShortcut=$chessMenuLink;SHA256=$chessSourceHash}
