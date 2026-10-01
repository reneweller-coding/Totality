; Totality -- the Windows installer (after Ephemeris' Deploy\Ephemeris.iss).
;
; Built by Deploy\build_release.ps1, which stages everything under dist\stage first and only then calls the
; compiler. Nothing in here reaches into a build tree: what is in the staging folder is exactly what gets
; installed, so the payload can be looked at before the setup is made.
;
;   ISCC.exe /DVersion=1.0.0 Deploy\Totality.iss
;
; The binaries are linked against the static MSVC runtime (TOT_STATIC_RUNTIME=ON): no redistributable, no DLL
; beside the executable. Totality opens no data files -- every sound is synthesised -- so nothing is
; downloaded and the VST3 needs no copies of anything.

#ifndef Version
  #define Version "1.0.0"
#endif
#define AppName "Totality"
#define Publisher "Rene Weller"
#define Stage "..\dist\stage"

[Setup]
AppId={{20CEA445-B82E-47FB-A9AC-71BCB9EC3871}
AppName={#AppName}
AppVersion={#Version}
AppVerName={#AppName} {#Version}
AppPublisher={#Publisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile={#Stage}\LICENSE.txt
OutputDir=..\dist
OutputBaseFilename={#AppName}-{#Version}-Setup
SetupIconFile={#Stage}\totality.ico
UninstallDisplayIcon={app}\Totality.exe
UninstallDisplayName={#AppName} {#Version}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; Machine-wide by default, because the VST3 belongs in the shared plug-in folder; "just for me" (or
; /CURRENTUSER) installs into the per-user folders. Every path below is an {auto...} one for that reason.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
VersionInfoVersion={#Version}
VersionInfoCompany={#Publisher}
VersionInfoDescription={#AppName} installer

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[CustomMessages]
en.CompStandalone=Standalone application
en.CompVst3=VST3 plug-in (for a DAW)
en.CompRender=Offline renderer (tot_render, command line)
en.CompManual=Manual (PDF)
en.TaskDesktop=Create a desktop shortcut
en.NoAvx2=This processor reports no AVX2 support.%n%nTotality is built for AVX2, which every x86-64 processor since 2013 has. Without it, it will not start.%n%nInstall anyway?
de.CompStandalone=Eigenstaendiges Programm
de.CompVst3=VST3-Plugin (fuer eine DAW)
de.CompRender=Offline-Renderer (tot_render, Kommandozeile)
de.CompManual=Handbuch (PDF)
de.TaskDesktop=Verknuepfung auf dem Desktop anlegen
de.NoAvx2=Dieser Prozessor meldet keine AVX2-Unterstuetzung.%n%nTotality ist fuer AVX2 gebaut, das jeder x86-64-Prozessor seit 2013 hat. Ohne AVX2 startet es nicht.%n%nTrotzdem installieren?

[Types]
Name: "full"; Description: "Full"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "standalone"; Description: "{cm:CompStandalone}"; Types: full custom; Flags: fixed
Name: "vst3";       Description: "{cm:CompVst3}";       Types: full custom
Name: "render";     Description: "{cm:CompRender}";     Types: full custom
Name: "manual";     Description: "{cm:CompManual}";     Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Components: standalone

[InstallDelete]
; Before a single file is copied: what this installer itself put there, so an update leaves nothing stale
; behind (the lesson Noctuary paid for). The user's own files in Documents\Totality are never touched.
Type: filesandordirs; Name: "{autocf}\VST3\Totality.vst3"
Type: files;          Name: "{app}\Totality-Manual.pdf"

[Files]
Source: "{#Stage}\Totality.exe";          DestDir: "{app}";                 Flags: ignoreversion; Components: standalone
Source: "{#Stage}\totality.ico";          DestDir: "{app}";                 Flags: ignoreversion; Components: standalone
Source: "{#Stage}\README.txt";             DestDir: "{app}";                 Flags: ignoreversion isreadme; Components: standalone
Source: "{#Stage}\LICENSE.txt";            DestDir: "{app}";                 Flags: ignoreversion; Components: standalone
Source: "{#Stage}\tot_render.exe";         DestDir: "{app}";                 Flags: ignoreversion; Components: render
Source: "{#Stage}\Totality-Manual.pdf";   DestDir: "{app}";                 Flags: ignoreversion; Components: manual
Source: "{#Stage}\Totality.vst3\*";       DestDir: "{autocf}\VST3\Totality.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{autoprograms}\{#AppName}";          Filename: "{app}\Totality.exe"; IconFilename: "{app}\totality.ico"
Name: "{autoprograms}\{#AppName} Manual";   Filename: "{app}\Totality-Manual.pdf"; Components: manual
Name: "{autodesktop}\{#AppName}";           Filename: "{app}\Totality.exe"; IconFilename: "{app}\totality.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\Totality.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
function IsProcessorFeaturePresent(Feature: Integer): Boolean;
  external 'IsProcessorFeaturePresent@kernel32.dll stdcall';

// PF_AVX2_INSTRUCTIONS_AVAILABLE = 40: the core is built for AVX2 + FMA (Vec.h).
function InitializeSetup(): Boolean;
begin
  Result := True;
  if not IsProcessorFeaturePresent(40) then
    Result := MsgBox(CustomMessage('NoAvx2'), mbConfirmation, MB_YESNO) = IDYES;
end;
