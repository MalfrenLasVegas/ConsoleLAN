#define AppVersion "1.0.0"
[Setup]
AppId={{4A5734ED-D8AD-42AB-A730-B639CC1D6804}
AppName=ConsoleLAN
AppVersion={#AppVersion}
AppPublisher=ConsoleLAN
DefaultDirName={autopf}\ConsoleLAN
DefaultGroupName=ConsoleLAN
OutputDir=..\dist
OutputBaseFilename=ConsoleLAN-1.0.0-win64-setup
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
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "firewall"; Description: "Autorizar UDP 32189-32190 solo para ConsoleLAN en mi interfaz VPN (requiere IPv4 VPN)"; Flags: unchecked; Check: IsAdminInstallMode
Name: "desktopicon"; Description: "Crear acceso directo en el escritorio"; Flags: unchecked

[Files]
Source: "..\dist\ConsoleLAN-1.0.0-win64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\deps\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedRuntime

[Icons]
Name: "{group}\ConsoleLAN"; Filename: "{app}\ConsoleLAN.exe"
Name: "{group}\Manual"; Filename: "{app}\docs\MANUAL.md"
Name: "{autodesktop}\ConsoleLAN"; Filename: "{app}\ConsoleLAN.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /passive /norestart"; StatusMsg: "Instalando Microsoft Visual C++ Runtime..."; Check: NeedRuntime; Flags: waituntilterminated
Filename: "https://npcap.com/#download"; Description: "Abrir la descarga oficial de Npcap (si falta)"; Flags: postinstall shellexec skipifsilent unchecked
Filename: "{app}\ConsoleLAN.exe"; Description: "Abrir ConsoleLAN"; Flags: postinstall nowait skipifsilent unchecked runasoriginaluser

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
  VpnPage := CreateInputQueryPage(wpSelectTasks, 'Firewall de la VPN', 'La regla se limita al ejecutable, a UDP y a esta interfaz.', 'Indique la IPv4 local de la VPN ya configurada. Any permite todos los participantes de esa VPN; puede limitarlo a una IP o red IPv4/CIDR.');
  VpnPage.Add('IPv4 VPN de este PC:', False);
  VpnPage.Add('Participantes permitidos (Any o IPv4/CIDR):', False);
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
  if not Result then MsgBox('Introduzca una IPv4 VPN válida y Any o una IPv4/CIDR válida.', mbError, MB_OK);
end;
function GetVpnAddress(Param: String): String; begin Result := VpnPage.Values[0]; end;
function GetRemoteNetwork(Param: String): String; begin Result := VpnPage.Values[1]; end;
procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer; Arguments: String;
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('firewall') then begin
    Arguments := '-NoProfile -ExecutionPolicy Bypass -File "' + ExpandConstant('{app}\scripts\firewall.ps1') + '" -ProgramPath "' + ExpandConstant('{app}\ConsoleLAN.exe') + '" -VpnAddress "' + VpnPage.Values[0] + '" -RemoteNetwork "' + VpnPage.Values[1] + '"';
    if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'), Arguments, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
      MsgBox('No se pudieron crear las reglas de firewall. Revise la IPv4 VPN y las políticas de Windows. Puede ejecutar scripts\firewall.ps1 con permisos de administrador; consulte el diagnóstico.', mbError, MB_OK);
  end;
end;
