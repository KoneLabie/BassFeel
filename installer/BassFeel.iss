; BassFeel one-click Windows installer (Inno Setup 6.3+)
#define AppName "BassFeel"
#define AppVersion "0.1.0"
#define Publisher "Kone Labie"
#define BuildDir "..\build\BassFeel_artefacts\Release"

[Setup]
AppId={{B5A5FEE1-0C0A-4C6E-9A57-BA55FEE10001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={commonpf64}\Common Files\VST3
; Skip every page that asks the user to make a decision
DisableWelcomePage=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableReadyMemo=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=output
OutputBaseFilename={#AppName}-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
UsePreviousAppDir=no
UninstallDisplayName={#AppName}
CreateUninstallRegKey=yes

[Files]
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; DestDir: "{commonpf64}\Common Files\VST3\{#AppName}.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.FinishedLabel.Caption :=
      'BassFeel is installed!' + #13#10#13#10 +
      'Open Ableton Live or FL Studio and look for "BassFeel" in your plugins.' + #13#10#13#10 +
      'Not showing up?' + #13#10 +
      '  Ableton: Preferences > Plug-ins > turn on "Use VST3 Plug-in System Folders", then Rescan.' + #13#10 +
      '  FL Studio: Options > Manage plugins > Find installed plugins.';
end;
