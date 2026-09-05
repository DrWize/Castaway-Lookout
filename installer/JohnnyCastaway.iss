#define AppName "Johnny Castaway 2026"
#define AppVersion "2026.1.0"
#define AppPublisher "DrWize"

[Setup]
AppId={{8EBBA4D8-31CE-4B6C-A7F6-65A3F37D7260}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/DrWize/Castaway-Lookout
AppSupportURL=https://github.com/DrWize/Castaway-Lookout/issues
DefaultDirName={localappdata}\Programs\JohnnyCastaway
DefaultGroupName=Johnny Castaway 2026
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir=..\build\installer
OutputBaseFilename=JohnnyCastaway-Windows-11-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
SetupIconFile=..\assets\icons\candidates\castaway-lookout.ico
UninstallDisplayIcon={app}\JohnnyCastaway.exe
VersionInfoVersion=2026.1.0.4
VersionInfoDescription=Johnny Castaway 2026 Windows 11 x64 Setup
VersionInfoProductName=Johnny Castaway 2026
VersionInfoProductVersion=2026.1.0.4

[Files]
Source: "..\build\JohnnyCastaway.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\JohnnyCastaway.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "JohnnyCastaway.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist
Source: "Install-JohnnyData.ps1"; DestDir: "{app}\Setup"; Flags: ignoreversion
Source: "sound-manifest.json"; DestDir: "{app}\Setup"; Flags: ignoreversion

[InstallDelete]
Type: files; Name: "{localappdata}\JohnnyCastaway\JohnnyCastaway.ini"
Type: files; Name: "{localappdata}\JohnnyCastaway\config.txt"
Type: files; Name: "{%USERPROFILE}\.johnny_castaway_2026"

[UninstallDelete]
Type: files; Name: "{app}\JohnnyCastaway.ini"

[Icons]
Name: "{group}\Johnny Castaway"; Filename: "{app}\JohnnyCastaway.exe"; WorkingDir: "{app}"
Name: "{group}\Data and Sound Setup"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\Setup\Install-JohnnyData.ps1"""; WorkingDir: "{app}\Setup"; IconFilename: "{app}\JohnnyCastaway.exe"
Name: "{group}\Windows Screen Saver Settings"; Filename: "{sys}\control.exe"; Parameters: "desk.cpl,,1"; IconFilename: "{app}\JohnnyCastaway.exe"
Name: "{group}\Uninstall Johnny Castaway"; Filename: "{uninstallexe}"

[Code]
var
  SourcePage: TInputOptionWizardPage;
  ArchivePage: TInputFileWizardPage;
  OptionsPage: TInputOptionWizardPage;
  NoticePage: TOutputMsgMemoWizardPage;

function PowerShellPath: String;
begin
  Result := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
end;

function DataSetupScript: String;
begin
  Result := ExpandConstant('{app}\Setup\Install-JohnnyData.ps1');
end;

function ManagedDataDirectory: String;
begin
  Result := ExpandConstant('{localappdata}\JohnnyCastaway\scrantic');
end;

procedure InitializeWizard;
begin
  SourcePage := CreateInputOptionPage(
    wpSelectDir,
    'Original Johnny Castaway data',
    'Choose where Setup should obtain the required animation data.',
    'The data is not bundled with this installer. Automatic download uses the preserved scrantic-run.zip from Internet Archive.',
    True,
    False);
  SourcePage.Add('Download the verified archive from Internet Archive (recommended)');
  SourcePage.Add('Use an existing scrantic-run.zip');
  SourcePage.SelectedValueIndex := 0;

  ArchivePage := CreateInputFilePage(
    SourcePage.ID,
    'Select the preserved archive',
    'Choose your existing scrantic-run.zip.',
    'Setup will accept only the recorded CRC32 and SHA-256 values.');
  ArchivePage.Add('ZIP archive:', 'ZIP archives (*.zip)|*.zip|All files (*.*)|*.*', '.zip');

  OptionsPage := CreateInputOptionPage(
    ArchivePage.ID,
    'Sound and screensaver',
    'Choose the optional finishing steps.',
    'Sound effects can be downloaded later by running Data and Sound Setup from the Start menu.',
    False,
    False);
  OptionsPage.Add('Install the 23 verified sound effects now');
  OptionsPage.Add('Set Johnny Castaway as my current screensaver after data verification');
  OptionsPage.Values[0] := True;
  OptionsPage.Values[1] := True;

  NoticePage := CreateOutputMsgMemoPage(
    OptionsPage.ID,
    'Important source and signing notes',
    'Review before installation',
    'This installer is currently unsigned, so Windows may display an Unknown publisher or SmartScreen warning.',
    'Johnny Castaway requires original Sierra/Dynamix data. The installer downloads it from an independent Internet Archive preservation item and downloads optional WAV files from the independent JCOS project. The files are verified before use and are not embedded in this installer.' + #13#10 + #13#10 +
    'Sound is optional and can be downloaded later from Data and Sound Setup in the Start menu.' + #13#10 + #13#10 +
    'The application and screensaver share one JohnnyCastaway.ini file. Open Johnny Castaway to configure the screensaver display, audio, monitor, playback, and data-folder settings.' + #13#10 + #13#10 +
    'The screensaver is registered for your Windows account only. Nothing is copied to System32 and administrator rights are not required.');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = ArchivePage.ID) and (SourcePage.SelectedValueIndex = 0);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = ArchivePage.ID) and
     (SourcePage.SelectedValueIndex = 1) and
     (not FileExists(ArchivePage.Values[0])) then
  begin
    MsgBox('Select an existing scrantic-run.zip before continuing.', mbError, MB_OK);
    Result := False;
  end;
end;

procedure CaptureAndActivateScreenSaver;
var
  PreviousSaver: String;
  PreviousActive: String;
  InstallerKey: String;
  InstalledSaver: String;
begin
  InstallerKey := 'Software\DrWize\JohnnyCastaway\Installer';
  InstalledSaver := ExpandConstant('{app}\JohnnyCastaway.scr');

  if not RegValueExists(HKCU, InstallerKey, 'PreviousCaptured') then
  begin
    if RegQueryStringValue(HKCU, 'Control Panel\Desktop', 'SCRNSAVE.EXE', PreviousSaver) then
      RegWriteStringValue(HKCU, InstallerKey, 'PreviousScreenSaver', PreviousSaver);
    if RegQueryStringValue(HKCU, 'Control Panel\Desktop', 'ScreenSaveActive', PreviousActive) then
      RegWriteStringValue(HKCU, InstallerKey, 'PreviousScreenSaveActive', PreviousActive);
    RegWriteDWordValue(HKCU, InstallerKey, 'PreviousCaptured', 1);
  end;

  RegWriteStringValue(HKCU, InstallerKey, 'InstalledScreenSaver', InstalledSaver);
  RegWriteStringValue(HKCU, 'Control Panel\Desktop', 'SCRNSAVE.EXE', InstalledSaver);
  RegWriteStringValue(HKCU, 'Control Panel\Desktop', 'ScreenSaveActive', '1');
end;

procedure RunDataSetup;
var
  Parameters: String;
  ResultCode: Integer;
begin
  Parameters :=
    '-NoProfile -ExecutionPolicy Bypass -File ' + AddQuotes(DataSetupScript) +
    ' -TargetDirectory ' + AddQuotes(ManagedDataDirectory) +
    ' -Quiet';

  if SourcePage.SelectedValueIndex = 0 then
    Parameters := Parameters + ' -Mode Download'
  else
    Parameters := Parameters + ' -Mode Archive -ArchivePath ' + AddQuotes(ArchivePage.Values[0]);

  if OptionsPage.Values[0] then
    Parameters := Parameters + ' -InstallSound';

  WizardForm.StatusLabel.Caption := 'Downloading and verifying Johnny Castaway data...';
  if (not Exec(PowerShellPath, Parameters, ExpandConstant('{app}\Setup'), SW_SHOW,
      ewWaitUntilTerminated, ResultCode)) or (ResultCode <> 0) then
    RaiseException('Data and sound setup failed. The application files remain installed; rerun Setup or use Data and Sound Setup from the Start menu. Exit code: ' + IntToStr(ResultCode));

  if OptionsPage.Values[1] then
    CaptureAndActivateScreenSaver;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    RunDataSetup;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  CurrentSaver: String;
  InstalledSaver: String;
  PreviousSaver: String;
  PreviousActive: String;
  InstallerKey: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;

  InstallerKey := 'Software\DrWize\JohnnyCastaway\Installer';
  if RegQueryStringValue(HKCU, InstallerKey, 'InstalledScreenSaver', InstalledSaver) and
     RegQueryStringValue(HKCU, 'Control Panel\Desktop', 'SCRNSAVE.EXE', CurrentSaver) and
     (CompareText(CurrentSaver, InstalledSaver) = 0) then
  begin
    if RegQueryStringValue(HKCU, InstallerKey, 'PreviousScreenSaver', PreviousSaver) and
       (PreviousSaver <> '') then
      RegWriteStringValue(HKCU, 'Control Panel\Desktop', 'SCRNSAVE.EXE', PreviousSaver)
    else
      RegDeleteValue(HKCU, 'Control Panel\Desktop', 'SCRNSAVE.EXE');

    if RegQueryStringValue(HKCU, InstallerKey, 'PreviousScreenSaveActive', PreviousActive) then
      RegWriteStringValue(HKCU, 'Control Panel\Desktop', 'ScreenSaveActive', PreviousActive);
  end;

  RegDeleteKeyIncludingSubkeys(HKCU, InstallerKey);
end;
