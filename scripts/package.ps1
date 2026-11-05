param([string]$QtRoot = $env:Qt6_ROOT)
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$versionMatch = [regex]::Match([IO.File]::ReadAllText((Join-Path $workspacePath 'CMakeLists.txt')), 'project\(ConsoleLAN VERSION ([0-9]+\.[0-9]+\.[0-9]+) ')
if (-not $versionMatch.Success) { throw 'No se pudo leer la version de CMake.' }
$appVersion = $versionMatch.Groups[1].Value
if (-not $QtRoot) { $QtRoot = 'D:\Qt\6.12.0\msvc2022_64' }
$buildPath = Join-Path $workspacePath 'build-x64'
$distributionPath = Join-Path $workspacePath 'dist'
$bundleName = "ConsoleLAN-$appVersion-win64"
$bundlePath = Join-Path $distributionPath $bundleName
$qtSource = Join-Path $workspacePath 'deps\qtbase-everywhere-src-6.12.0.tar.xz'
if (-not (Test-Path -LiteralPath $qtSource)) { throw 'Ejecute scripts/fetch-deps.ps1 -Packaging antes de empaquetar.' }
if ((& (Join-Path $QtRoot 'bin\qmake.exe') -query QT_VERSION) -ne '6.12.0') { throw 'Este paquete de fuentes corresponde exactamente a Qt 6.12.0; actualice fuentes y licencias para otra versión.' }
# The bundle is regenerated from scratch; validate its resolved parent before deleting it.
if (Test-Path -LiteralPath $bundlePath) {
    $resolvedBundle = (Resolve-Path -LiteralPath $bundlePath).Path
    if ([IO.Path]::GetFullPath($resolvedBundle) -ne [IO.Path]::GetFullPath((Join-Path $distributionPath $bundleName))) { throw 'Destino de limpieza inesperado.' }
    Remove-Item -LiteralPath $resolvedBundle -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $bundlePath | Out-Null
Copy-Item -LiteralPath (Join-Path $buildPath 'ConsoleLAN.exe'),(Join-Path $buildPath 'ConsoleLAN.Access.exe'),(Join-Path $buildPath 'wintun.dll') -Destination $bundlePath
if ((Get-AuthenticodeSignature -LiteralPath (Join-Path $bundlePath 'wintun.dll')).Status -ne 'Valid') { throw 'Firma de Wintun inválida.' }
& (Join-Path $QtRoot 'bin\windeployqt.exe') --release --no-compiler-runtime --no-translations --no-opengl-sw --skip-plugin-types 'iconengines,imageformats,tls,networkinformation' --dir $bundlePath (Join-Path $bundlePath 'ConsoleLAN.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt falló.' }
Copy-Item -LiteralPath (Join-Path $workspacePath 'docs') -Destination $bundlePath -Recurse
Copy-Item -LiteralPath (Join-Path $workspacePath 'LICENSE'),(Join-Path $workspacePath 'README.md'),(Join-Path $workspacePath 'README.en.md') -Destination $bundlePath
New-Item -ItemType Directory -Force -Path (Join-Path $bundlePath 'scripts'),(Join-Path $bundlePath 'licenses\sources') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'firewall.ps1') -Destination (Join-Path $bundlePath 'scripts')
Copy-Item -LiteralPath (Join-Path $workspacePath 'deps\wintun-0.14.1\wintun\LICENSE.txt') -Destination (Join-Path $bundlePath 'licenses\Wintun-LICENSE.txt')
Copy-Item -LiteralPath $qtSource -Destination (Join-Path $bundlePath 'licenses\sources')
Copy-Item -LiteralPath (Join-Path $workspacePath 'docs\DEPENDENCIAS.md') -Destination (Join-Path $bundlePath 'licenses\THIRD-PARTY.md')
Copy-Item -LiteralPath (Join-Path $workspacePath 'deps\DirectXShaderCompiler-LICENSE.txt') -Destination (Join-Path $bundlePath 'licenses')
@("ConsoleLAN $appVersion Windows x64 Release", 'Qt 6.12.0 dynamic / MSVC C++20', (& cmake --version | Select-Object -First 1), ('Ninja ' + (& ninja --version)), ('ConsoleLAN.exe SHA256 ' + (Get-FileHash -LiteralPath (Join-Path $bundlePath 'ConsoleLAN.exe') -Algorithm SHA256).Hash)) | Set-Content -LiteralPath (Join-Path $bundlePath 'build-info.txt') -Encoding utf8
python (Join-Path $PSScriptRoot 'qt-licenses.py') $qtSource (Join-Path $bundlePath 'licenses\qtbase')
if ($LASTEXITCODE -ne 0) { throw 'No se pudieron extraer las licencias Qt.' }
$innoCompiler = Join-Path $workspacePath 'deps\inno\ISCC.exe'
& $innoCompiler (Join-Path $workspacePath 'packaging\ConsoleLAN.iss')
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup falló.' }
$zipFile = Join-Path $distributionPath "ConsoleLAN-$appVersion-win64-portable.zip"
Compress-Archive -LiteralPath $bundlePath -DestinationPath $zipFile -Force
$sourceZip = Join-Path $distributionPath "ConsoleLAN-$appVersion-source.zip"
$sourceItems = @('src','i18n','tests','scripts','packaging','docs','README.md','README.en.md','LICENSE','CMakeLists.txt','.clang-format','.gitignore') | ForEach-Object { Join-Path $workspacePath $_ }
Compress-Archive -LiteralPath $sourceItems -DestinationPath $sourceZip -Force
Get-Item -LiteralPath $zipFile,$sourceZip,(Join-Path $distributionPath "ConsoleLAN-$appVersion-win64-setup.exe") | Get-FileHash -Algorithm SHA256 | ForEach-Object { "$($_.Hash.ToLower())  $([IO.Path]::GetFileName($_.Path))" } | Set-Content -LiteralPath (Join-Path $distributionPath 'SHA256SUMS.txt') -Encoding utf8
Write-Output "Paquetes creados en $distributionPath"
