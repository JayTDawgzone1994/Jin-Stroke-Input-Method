#define AppVersion "0.5.0"
#define Root ".."
#define Payload Root + "\out\package\payload"

[Setup]
AppId={{15B2681E-9FBC-4BD4-AF97-37D171932E71}
AppName=錦筆劃輸入法
AppVersion={#AppVersion}
AppVerName=錦筆劃輸入法 {#AppVersion}
DefaultDirName={autopf}\StrokeIME
DefaultGroupName=錦筆劃輸入法
UsePreviousGroup=no
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.17763
OutputDir={#Root}\out\release
OutputBaseFilename=StrokeIME-Setup-{#AppVersion}-win64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
UninstallDisplayName=錦筆劃輸入法
SetupLogging=yes
InfoAfterFile=quick-start.txt
VersionInfoVersion={#AppVersion}.0

[Languages]
Name: "chinesetraditional"; MessagesFile: "compiler:Default.isl,TraditionalChinese.isl"

[Files]
Source: "{#Root}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Payload}\x64\*"; DestDir: "{app}\versions\{#AppVersion}\x64"; Flags: ignoreversion recursesubdirs createallsubdirs uninsrestartdelete
Source: "{#Payload}\x86\*"; DestDir: "{app}\versions\{#AppVersion}\x86"; Flags: ignoreversion recursesubdirs createallsubdirs uninsrestartdelete
Source: "{#Payload}\settings\*"; DestDir: "{app}\versions\{#AppVersion}\settings"; Excludes: "*.pdb,*.ilk"; Flags: ignoreversion recursesubdirs createallsubdirs uninsrestartdelete
Source: "quick-start.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\錦筆畫輸入法設定"; Filename: "{app}\versions\{#AppVersion}\settings\stroke_settings.exe"
Name: "{group}\使用說明與鍵位"; Filename: "{app}\quick-start.txt"
Name: "{group}\啟用錦筆劃輸入法"; Filename: "{app}\versions\{#AppVersion}\x64\stroke_setup.exe"; Parameters: "enable"
Name: "{group}\停用錦筆劃輸入法"; Filename: "{app}\versions\{#AppVersion}\x64\stroke_setup.exe"; Parameters: "disable"
Name: "{group}\解除安裝"; Filename: "{uninstallexe}"

; Remove only the shortcuts created by our earlier installer; preserve unrelated files.
[InstallDelete]
Type: files; Name: "{commonprograms}\五筆劃輸入法\按鍵設定.lnk"
Type: files; Name: "{commonprograms}\五筆劃輸入法\使用說明與鍵位.lnk"
Type: files; Name: "{commonprograms}\五筆劃輸入法\啟用筆劃輸入法.lnk"
Type: files; Name: "{commonprograms}\五筆劃輸入法\停用筆劃輸入法.lnk"
Type: files; Name: "{commonprograms}\五筆劃輸入法\解除安裝.lnk"
Type: dirifempty; Name: "{commonprograms}\五筆劃輸入法"

[Code]
const
  ClassKey = 'Software\Classes\CLSID\{65EAB844-8F22-46EF-AD6C-3443C1998260}\InprocServer32';
var
  Previous64, Previous32: String;

function ToolPath(Arch: String): String;
begin
  Result := ExpandConstant('{app}\versions\{#AppVersion}\') + Arch + '\stroke_setup.exe';
end;

function DllPath(Arch: String): String;
begin
  Result := ExpandConstant('{app}\versions\{#AppVersion}\') + Arch + '\stroke_tsf.dll';
end;

function RunTool(Arch, Arguments: String): Boolean;
var Code: Integer;
begin
  Result := Exec(ToolPath(Arch), Arguments, '', SW_HIDE, ewWaitUntilTerminated, Code);
  Result := Result and (Code = 0);
  Log('stroke_setup ' + Arch + ' ' + Arguments + ': ' + IntToStr(Code));
end;

function InitializeSetup(): Boolean;
begin
  RegQueryStringValue(HKLM64, ClassKey, '', Previous64);
  RegQueryStringValue(HKLM32, ClassKey, '', Previous32);
  Result := True;
end;

procedure RestoreRegistration();
begin
  RunTool('x64', 'unregister "' + DllPath('x64') + '"');
  RunTool('x86', 'unregister "' + DllPath('x86') + '"');
  if (Previous64 <> '') and FileExists(Previous64) then
    if not RunTool('x64', 'register "' + Previous64 + '"') then
      Log('WARNING: could not restore previous x64 registration');
  if (Previous32 <> '') and FileExists(Previous32) then
    if not RunTool('x86', 'register "' + Previous32 + '"') then
      Log('WARNING: could not restore previous x86 registration');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var Code: Integer; Enabled: Boolean;
begin
  if CurStep = ssPostInstall then begin
    { These checks run before any registration changes. }
    if not RunTool('x64', 'selftest "' + DllPath('x64') + '"') then
      RaiseException('64-bit input method validation failed.');
    if not RunTool('x86', 'selftest "' + DllPath('x86') + '"') then
      RaiseException('32-bit input method validation failed.');
    try
      if not RunTool('x64', 'register "' + DllPath('x64') + '"') then
        RaiseException('64-bit input method registration failed.');
      if not RunTool('x86', 'register "' + DllPath('x86') + '"') then
        RaiseException('32-bit input method registration failed.');
    except
      RestoreRegistration();
      RaiseException(GetExceptionMessage());
    end;
  end;
  if CurStep = ssDone then begin
    { Use the original desktop user, even if a different administrator approved UAC. }
    Enabled := ExecAsOriginalUser(ToolPath('x64'), 'enable', '', SW_HIDE, ewWaitUntilTerminated, Code);
    if (not Enabled) or (Code <> 0) then
      SuppressibleMsgBox('已安裝。請從開始功能表執行「啟用錦筆劃輸入法」，加入你帳號的輸入法清單。', mbInformation, MB_OK, IDOK);
  end;
end;

function BelongsToInstallation(RegistryRoot: Integer): Boolean;
var Registered: String;
begin
  Result := RegQueryStringValue(RegistryRoot, ClassKey, '', Registered) and
    (Pos(Lowercase(ExpandConstant('{app}\versions\')), Lowercase(Registered)) = 1);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then begin
    { Do not unregister a newer/development deployment owned by a different directory. }
    if BelongsToInstallation(HKLM64) or BelongsToInstallation(HKLM32) then begin
      RunTool('x64', 'disable');
      if BelongsToInstallation(HKLM64) then
        if not RunTool('x64', 'unregister "' + DllPath('x64') + '"') then
          RaiseException('Could not unregister the 64-bit input method. Please retry uninstall.');
      if BelongsToInstallation(HKLM32) then
        if not RunTool('x86', 'unregister "' + DllPath('x86') + '"') then
          RaiseException('Could not unregister the 32-bit input method. Please retry uninstall.');
    end;
  end;
end;





