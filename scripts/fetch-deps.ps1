param([switch]$Packaging)
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$dependencyPath = Join-Path $workspacePath 'deps'
New-Item -ItemType Directory -Force -Path $dependencyPath | Out-Null
function Fetch-Verified([string]$Url, [string]$Name, [string]$ExpectedHash) {
    $targetFile = Join-Path $dependencyPath $Name
    if (-not (Test-Path -LiteralPath $targetFile)) { Invoke-WebRequest -Uri $Url -OutFile $targetFile }
    if ($ExpectedHash -and (Get-FileHash -LiteralPath $targetFile -Algorithm SHA256).Hash -ne $ExpectedHash) { throw "SHA256 incorrecto: $Name" }
    return $targetFile
}
$sdkFile = Fetch-Verified 'https://npcap.com/dist/npcap-sdk-1.17.zip' 'npcap-sdk-1.17.zip' 'AB7E1A6CB8D07427E0071EC11BE380BD9D2BB19AE6AB3F5EDE0698C301638258'
if (-not (Test-Path -LiteralPath (Join-Path $dependencyPath 'npcap-sdk-1.17\Include\pcap.h'))) {
    Expand-Archive -LiteralPath $sdkFile -DestinationPath (Join-Path $dependencyPath 'npcap-sdk-1.17')
}
$wintunZip = Fetch-Verified 'https://www.wintun.net/builds/wintun-0.14.1.zip' 'wintun-0.14.1.zip' '07C256185D6EE3652E09FA55C0B673E2624B565E02C4B9091C79CA7D2F24EF51'
if (-not (Test-Path -LiteralPath (Join-Path $dependencyPath 'wintun-0.14.1\wintun\include\wintun.h'))) {
    Expand-Archive -LiteralPath $wintunZip -DestinationPath (Join-Path $dependencyPath 'wintun-0.14.1')
}
if ((Get-AuthenticodeSignature -LiteralPath (Join-Path $dependencyPath 'wintun-0.14.1\wintun\bin\amd64\wintun.dll')).Status -ne 'Valid') { throw 'Firma de Wintun inválida.' }
$internetArgs = @((Join-Path $PSScriptRoot 'fetch-internet.py'))
if ($Packaging) { $internetArgs += '--sources' }
& python @internetArgs
if ($LASTEXITCODE -ne 0) { throw 'No se pudieron preparar las dependencias de Internet.' }
if ($Packaging) {
    Fetch-Verified 'https://download.qt.io/archive/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz' 'qtbase-everywhere-src-6.12.0.tar.xz' 'A951BD163C7B80FC6B8C88D7668FB56ABF91C152373E13C10666763238131307' | Out-Null
    Fetch-Verified 'https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/main/LICENSE.TXT' 'DirectXShaderCompiler-LICENSE.txt' '' | Out-Null
    $runtimeFile = Fetch-Verified 'https://aka.ms/vc14/vc_redist.x64.exe' 'vc_redist.x64.exe' ''
    if ((Get-AuthenticodeSignature -LiteralPath $runtimeFile).Status -ne 'Valid') { throw 'Firma Microsoft Runtime inválida.' }
    $innoInstaller = Fetch-Verified 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_4/innosetup-6.7.4.exe' 'innosetup-6.7.4.exe' 'A549929319F9120716677C685535CA4671E4B937B5FDC87E2585F32D8EB30E8C'
    if ((Get-AuthenticodeSignature -LiteralPath $innoInstaller).Status -ne 'Valid') { throw 'Firma Inno Setup inválida.' }
    if (-not (Test-Path -LiteralPath (Join-Path $dependencyPath 'inno\ISCC.exe'))) {
        $innoFolder = Join-Path $dependencyPath 'inno'
        $installerProcess = Start-Process -FilePath $innoInstaller -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CURRENTUSER',"/DIR=`"$innoFolder`"",'/NOICONS' -WindowStyle Hidden -Wait -PassThru
        if ($installerProcess.ExitCode -and $installerProcess.ExitCode -ne 0) { throw 'No se pudo instalar el compilador Inno Setup local.' }
    }
}
Write-Output 'Dependencias preparadas. Npcap runtime debe instalarlo cada usuario por separado.'
