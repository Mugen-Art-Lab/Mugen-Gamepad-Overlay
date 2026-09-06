; SPDX-FileCopyrightText: 2026 Mugen Art Lab
; SPDX-License-Identifier: GPL-2.0-or-later

#define MyAppName "Mugen Gamepad Overlay"
#ifndef MyAppVersion
  #define MyAppVersion "0.8.1"
#endif
#define MyAppPublisher "Mugen Art Lab"
#define MyAppURL "https://github.com/Mugen-Art-Lab/Mugen-Gamepad-Overlay"
#define MyPluginName "mugen-gamepad-overlay"

#ifndef PayloadRoot
  #error PayloadRoot is required. Use installer\build-installer.ps1.
#endif
#ifndef OutputDir
  #error OutputDir is required. Use installer\build-installer.ps1.
#endif
#ifndef RepoRoot
  #error RepoRoot is required. Use installer\build-installer.ps1.
#endif

[Setup]
AppId={{8A748DBA-EB9A-4CB6-A188-769CE18FDDA8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
AppCopyright=Copyright (C) 2026 Mugen Art Lab
DefaultDirName={commonappdata}\obs-studio\plugins\{#MyPluginName}
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Mugen-Gamepad-Overlay-{#MyAppVersion}-Windows-x64-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
Uninstallable=yes
CreateUninstallRegKey=yes
CloseApplications=no
RestartApplications=no
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"; InfoAfterFile: "{#InfoAfterEN}"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"; InfoAfterFile: "{#InfoAfterRU}"

[Messages]
english.SelectLanguageTitle=Select setup language / Выберите язык установки
english.SelectLanguageLabel=Choose the language for installation / Выберите язык установки
russian.SelectLanguageTitle=Select setup language / Выберите язык установки
russian.SelectLanguageLabel=Choose the language for installation / Выберите язык установки

[CustomMessages]
english.CloseObsTitle=OBS Studio is running
russian.CloseObsTitle=OBS Studio запущена
english.CloseObsRetry=Close OBS Studio completely, then click Retry.
russian.CloseObsRetry=Полностью закройте OBS Studio, затем нажмите «Повторить».
english.CloseObsUninstallTitle=OBS Studio is running
russian.CloseObsUninstallTitle=OBS Studio запущена
english.CloseObsUninstallRetry=Close OBS Studio completely, then click Retry to continue uninstalling.
russian.CloseObsUninstallRetry=Полностью закройте OBS Studio, затем нажмите «Повторить», чтобы продолжить удаление.
english.RetryButton=Retry
russian.RetryButton=Повторить
english.CancelButton=Cancel
russian.CancelButton=Отмена

[Files]
Source: "{#PayloadRoot}\{#MyPluginName}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
function ObsIsRunning(): Boolean;
var
  ResultCode: Integer;
  PowerShellPath: String;
begin
  PowerShellPath := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
  Result := False;
  if FileExists(PowerShellPath) then
  begin
    if Exec(PowerShellPath,
      '-NoProfile -NonInteractive -WindowStyle Hidden -Command "if (Get-Process obs64 -ErrorAction SilentlyContinue) { exit 10 } else { exit 0 }"',
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      Result := ResultCode = 10;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpReady then
  begin
    while ObsIsRunning() do
    begin
      if TaskDialogMsgBox(CustomMessage('CloseObsTitle'),
        CustomMessage('CloseObsRetry'), mbError, MB_RETRYCANCEL, [CustomMessage('RetryButton'), CustomMessage('CancelButton')], 0) <> IDRETRY then
      begin
        Result := False;
        Exit;
      end;
    end;
  end;
end;

function InitializeUninstall(): Boolean;
begin
  Result := True;
  while ObsIsRunning() do
  begin
    if TaskDialogMsgBox(CustomMessage('CloseObsUninstallTitle'),
      CustomMessage('CloseObsUninstallRetry'), mbError, MB_RETRYCANCEL, [CustomMessage('RetryButton'), CustomMessage('CancelButton')], 0) <> IDRETRY then
    begin
      Result := False;
      Exit;
    end;
  end;
end;
