#define AppVersion "1.3.2"
[Setup]
AppId={{4A5734ED-D8AD-42AB-A730-B639CC1D6804}
AppName=ConsoleLAN
AppVersion={#AppVersion}
AppPublisher=ConsoleLAN
DefaultDirName={autopf}\ConsoleLAN
DefaultGroupName=ConsoleLAN
OutputDir=..\dist
OutputBaseFilename=ConsoleLAN-1.3.2-win64-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\ConsoleLAN.exe
LicenseFile=..\LICENSE
InfoBeforeFile=INSTALL-NOTES.txt
CloseApplications=yes
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"; InfoBeforeFile: "INSTALL-NOTES.en.txt"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"; InfoBeforeFile: "INSTALL-NOTES.txt"

[CustomMessages]
spanish.FirewallTask=Autorizar UDP 32189-32190 solo para ConsoleLAN en mi interfaz VPN (requiere IPv4 VPN)
english.FirewallTask=Allow UDP 32189-32190 only for ConsoleLAN on my VPN interface (requires local VPN IPv4)
spanish.DesktopIcon=Crear acceso directo en el escritorio
english.DesktopIcon=Create a desktop shortcut
spanish.InstallingRuntime=Instalando Microsoft Visual C++ Runtime...
english.InstallingRuntime=Installing Microsoft Visual C++ Runtime...
spanish.DownloadNpcap=Abrir la descarga oficial de Npcap (si falta)
english.DownloadNpcap=Open the official Npcap download (if needed)
spanish.Launch=Abrir ConsoleLAN
english.Launch=Launch ConsoleLAN
spanish.FirewallTitle=Firewall de la VPN
english.FirewallTitle=VPN firewall
spanish.FirewallSubtitle=La regla se limita al ejecutable, a UDP y a esta interfaz.
english.FirewallSubtitle=The rule is limited to this executable, UDP and this interface.
spanish.FirewallInstructions=Indique la IPv4 local de la VPN ya configurada. Any permite todos los participantes de esa VPN; puede limitarlo a una IP o red IPv4/CIDR.
english.FirewallInstructions=Enter the local IPv4 of your existing VPN. Any allows all participants of this VPN; you can restrict it to one IPv4 address or CIDR network.
spanish.LocalVpn=IPv4 VPN de este PC:
english.LocalVpn=This PC VPN IPv4:
spanish.RemotePeers=Participantes permitidos (Any o IPv4/CIDR):
english.RemotePeers=Allowed peers (Any or IPv4/CIDR):
spanish.InvalidAddress=Introduzca una IPv4 VPN válida y Any o una IPv4/CIDR válida.
english.InvalidAddress=Enter a valid VPN IPv4 and Any or a valid IPv4/CIDR.
spanish.FirewallFailed=No se pudieron crear las reglas de firewall. Revise la IPv4 VPN y las políticas de Windows. Puede ejecutar scripts\firewall.ps1 con permisos de administrador; consulte el diagnóstico.
english.FirewallFailed=Could not create firewall rules. Check your VPN IPv4 and Windows policies. You can run scripts\firewall.ps1 as administrator; see diagnostics.

[Tasks]
Name: "firewall"; Description: "{cm:FirewallTask}"; Flags: unchecked; Check: IsAdminInstallMode
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; Flags: unchecked

[Files]
Source: "..\dist\ConsoleLAN-1.3.2-win64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\deps\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedRuntime

[Icons]
Name: "{group}\ConsoleLAN"; Filename: "{app}\ConsoleLAN.exe"
Name: "{group}\Manual"; Filename: "{code:GetManualFile}"
Name: "{autodesktop}\ConsoleLAN"; Filename: "{app}\ConsoleLAN.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /passive /norestart"; StatusMsg: "{cm:InstallingRuntime}"; Check: NeedRuntime; Flags: waituntilterminated
Filename: "https://npcap.com/#download"; Description: "{cm:DownloadNpcap}"; Flags: postinstall shellexec skipifsilent unchecked
Filename: "{app}\ConsoleLAN.exe"; Description: "{cm:Launch}"; Flags: postinstall nowait skipifsilent unchecked runasoriginaluser

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\firewall.ps1"" -ProgramPath ""{app}\ConsoleLAN.exe"" -Remove"; Check: IsAdminInstallMode; Flags: runhidden waituntilterminated; RunOnceId: "RemoveConsoleLANFirewall"

[Code]
var VpnPage: TInputQueryWizardPage;
function NeedRuntime: Boolean;
var Installed, Minor: Cardinal;
begin
  Result := not RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed);
  if not Result then Result := Installed <> 1;
  if not Result then begin
    Result := not RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Minor', Minor);
    if not Result then Result := Minor < 50;
  end;
end;
procedure InitializeWizard;
begin
  VpnPage := CreateInputQueryPage(wpSelectTasks, CustomMessage('FirewallTitle'), CustomMessage('FirewallSubtitle'), CustomMessage('FirewallInstructions'));
  VpnPage.Add(CustomMessage('LocalVpn'), False);
  VpnPage.Add(CustomMessage('RemotePeers'), False);
  VpnPage.Values[1] := 'Any';
end;
function ShouldSkipPage(PageID: Integer): Boolean;
begin Result := (PageID = VpnPage.ID) and not WizardIsTaskSelected('firewall'); end;
function IsIPv4(Value: String): Boolean;
var I, Part, Dots, Digits: Integer; C: Char;
begin
  Result := False; Part := 0; Dots := 0; Digits := 0;
  for I := 1 to Length(Value) do begin
    C := Value[I];
    if C = '.' then begin if (Digits = 0) or (Part > 255) then Exit; Dots := Dots + 1; Part := 0; Digits := 0; end
    else begin if (C < '0') or (C > '9') then Exit; Digits := Digits + 1; if Digits > 3 then Exit; Part := Part * 10 + Ord(C) - Ord('0'); end;
  end;
  Result := (Dots = 3) and (Digits > 0) and (Part <= 255);
end;
function NextButtonClick(CurPageID: Integer): Boolean;
var S, Network: String; P, Prefix: Integer;
begin
  Result := True;
  if CurPageID <> VpnPage.ID then Exit;
  Result := IsIPv4(VpnPage.Values[0]);
  S := VpnPage.Values[1];
  if S <> 'Any' then begin
    P := Pos('/', S); Network := S;
    if P > 0 then begin Network := Copy(S, 1, P-1); Prefix := StrToIntDef(Copy(S, P+1, 3), -1); Result := Result and (Prefix >= 0) and (Prefix <= 32) and (Length(S)-P <= 2); end;
    Result := Result and IsIPv4(Network);
  end;
  if not Result then MsgBox(CustomMessage('InvalidAddress'), mbError, MB_OK);
end;
function GetVpnAddress(Param: String): String; begin Result := VpnPage.Values[0]; end;
function GetRemoteNetwork(Param: String): String; begin Result := VpnPage.Values[1]; end;
procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer; Arguments: String;
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('firewall') then begin
    Arguments := '-NoProfile -ExecutionPolicy Bypass -File "' + ExpandConstant('{app}\scripts\firewall.ps1') + '" -ProgramPath "' + ExpandConstant('{app}\ConsoleLAN.exe') + '" -VpnAddress "' + VpnPage.Values[0] + '" -RemoteNetwork "' + VpnPage.Values[1] + '"';
    if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'), Arguments, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
      MsgBox(CustomMessage('FirewallFailed'), mbError, MB_OK);
  end;
end;

function GetManualFile(Param: String): String;
begin
  if ExpandConstant('{language}') = 'english' then Result := ExpandConstant('{app}\docs\MANUAL.en.md')
  else Result := ExpandConstant('{app}\docs\MANUAL.md');
end;
