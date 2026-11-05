param(
    [Parameter(Mandatory=$true)][string]$ProgramPath,
    [string]$VpnAddress,
    [string]$RemoteNetwork = 'Any',
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$resolvedProgram = [IO.Path]::GetFullPath($ProgramPath)
$ruleNames = @('ConsoleLAN-1-UDP-In','ConsoleLAN-1-UDP-Out')
foreach ($ruleName in $ruleNames) {
    $rule = Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
    if ($rule) {
        $filter = $rule | Get-NetFirewallApplicationFilter
        if ($filter.Program -ne $resolvedProgram) { throw 'Hay reglas ConsoleLAN para otra instalación; revíselas antes de sustituirlas.' }
        $rule | Remove-NetFirewallRule
    }
}
if ($Remove) { exit 0 }
if (-not (Test-Path -LiteralPath $resolvedProgram -PathType Leaf)) { throw 'No existe el ejecutable.' }
$parsedAddress = $null
if (-not [Net.IPAddress]::TryParse($VpnAddress, [ref]$parsedAddress) -or $parsedAddress.AddressFamily -ne 'InterNetwork') { throw 'IPv4 VPN inválida.' }
$vpnInterface = Get-NetIPAddress -AddressFamily IPv4 -IPAddress $VpnAddress -ErrorAction Stop | Select-Object -First 1
if ($RemoteNetwork -ne 'Any') {
    if ($RemoteNetwork -notmatch '^([0-9]{1,3}\.){3}[0-9]{1,3}(/[0-9]{1,2})?$') { throw 'Use Any o una IPv4/CIDR.' }
    $networkParts = $RemoteNetwork.Split('/')
    $networkAddress = $null
    if (-not [Net.IPAddress]::TryParse($networkParts[0], [ref]$networkAddress)) { throw 'Red remota inválida.' }
    if ($networkParts.Length -gt 1 -and [int]$networkParts[1] -gt 32) { throw 'Prefijo remoto inválido.' }
}
foreach ($direction in @('Inbound','Outbound')) {
    $ruleName = if ($direction -eq 'Inbound') { $ruleNames[0] } else { $ruleNames[1] }
    New-NetFirewallRule -Name $ruleName -DisplayName "ConsoleLAN 1.0 UDP ($direction)" -Group 'ConsoleLAN' -Direction $direction -Action Allow -Program $resolvedProgram -Protocol UDP -LocalPort 32189,32190 -InterfaceAlias $vpnInterface.InterfaceAlias -RemoteAddress $RemoteNetwork -Profile Any | Out-Null
}
Write-Output "Reglas creadas para $resolvedProgram en $($vpnInterface.InterfaceAlias), UDP 32189-32190, origen $RemoteNetwork."
