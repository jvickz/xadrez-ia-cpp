param()

$ErrorActionPreference = 'Stop'
$chessPackageName = 'XadrezIA-3.3.0-windows-x64'
$chessExe = Join-Path $PSScriptRoot 'build/xadrez-grafico-v3.exe'
$chessLicenseDir = Join-Path $PSScriptRoot 'third-party'
foreach ($chessInput in @($chessExe, (Join-Path $PSScriptRoot 'LICENSE'),
    (Join-Path $PSScriptRoot 'release/LEIA-ME.txt'), (Join-Path $chessLicenseDir 'README.txt'),
    (Join-Path $chessLicenseDir 'zig-LICENSE.txt'), (Join-Path $chessLicenseDir 'libcxx-LICENSE.txt'),
    (Join-Path $chessLicenseDir 'libcxxabi-LICENSE.txt'), (Join-Path $chessLicenseDir 'libunwind-LICENSE.txt'),
    (Join-Path $chessLicenseDir 'mingw-COPYING.txt'))) {
    if (-not (Test-Path -LiteralPath $chessInput -PathType Leaf)) { throw "Arquivo ausente: $chessInput" }
}

# Check the PE machine type before naming the package Windows x64.
$chessBytes = [IO.File]::ReadAllBytes($chessExe)
if ($chessBytes.Length -lt 64 -or $chessBytes[0] -ne 77 -or $chessBytes[1] -ne 90) { throw 'Executável inválido.' }
$chessPeOffset = [BitConverter]::ToInt32($chessBytes, 60)
if ($chessPeOffset -lt 0 -or $chessPeOffset + 6 -gt $chessBytes.Length -or
    [BitConverter]::ToUInt32($chessBytes, $chessPeOffset) -ne 0x4550 -or
    [BitConverter]::ToUInt16($chessBytes, $chessPeOffset + 4) -ne 0x8664) { throw 'O executável precisa ser PE x64.' }

# Each invocation gets a new directory; existing release artifacts are never replaced.
$chessOutput = Join-Path $PSScriptRoot ('dist/' + [guid]::NewGuid().ToString('N'))
$chessPackage = Join-Path $chessOutput $chessPackageName
New-Item -ItemType Directory -Path $chessPackage -Force | Out-Null
Copy-Item -LiteralPath $chessExe -Destination (Join-Path $chessPackage 'XadrezIA.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE') -Destination $chessPackage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release/LEIA-ME.txt') -Destination $chessPackage
Copy-Item -LiteralPath $chessLicenseDir -Destination (Join-Path $chessPackage 'licenses') -Recurse
$chessExeHash = (Get-FileHash -LiteralPath $chessExe -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath (Join-Path $chessPackage 'XadrezIA.exe')).Hash -ne $chessExeHash) { throw 'Cópia divergente.' }
$chessZip = Join-Path $chessOutput ($chessPackageName + '.zip')
Compress-Archive -LiteralPath $chessPackage -DestinationPath $chessZip -CompressionLevel Optimal
$chessZipHash = (Get-FileHash -LiteralPath $chessZip -Algorithm SHA256).Hash
$chessSums = Join-Path $chessOutput 'SHA256SUMS.txt'
@("$chessZipHash  $chessPackageName.zip", "$chessExeHash  $chessPackageName/XadrezIA.exe") |
    Set-Content -LiteralPath $chessSums -Encoding ascii
[pscustomobject]@{Directory=$chessOutput;Zip=$chessZip;Checksums=$chessSums;ExecutableSHA256=$chessExeHash;ZipSHA256=$chessZipHash}
