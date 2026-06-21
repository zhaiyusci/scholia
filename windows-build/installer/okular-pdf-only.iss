#define AppName "Scholia"
#define ScholiaVersionFile AddBackslash(SourcePath) + "..\..\VERSION.txt"
#define ScholiaVersionHandle FileOpen(ScholiaVersionFile)
#define ScholiaVersionFromFile Trim(FileRead(ScholiaVersionHandle))
#expr FileClose(ScholiaVersionHandle)
#define AppVersion GetEnv("SCHOLIA_VERSION")
#if AppVersion == ""
#define AppVersion GetEnv("OKULAR_PDF_VERSION")
#endif
#if AppVersion == ""
#define AppVersion ScholiaVersionFromFile
#endif
#define FileVersion GetEnv("SCHOLIA_FILE_VERSION")
#if FileVersion == ""
#define FileVersion GetEnv("OKULAR_PDF_FILE_VERSION")
#endif
#if FileVersion == ""
#define FileVersion ScholiaVersionFromFile + ".0"
#endif
#define SourceDir GetEnv("SCHOLIA_STAGE")
#if SourceDir == ""
#define SourceDir GetEnv("OKULAR_PDF_STAGE")
#endif
#if SourceDir == ""
#define SourceDir "..\..\..\dist\scholia-pdf\app"
#endif
#define OutputDir GetEnv("SCHOLIA_OUTPUT")
#if OutputDir == ""
#define OutputDir GetEnv("OKULAR_PDF_OUTPUT")
#endif
#if OutputDir == ""
#define OutputDir "..\..\..\dist"
#endif

[Setup]
AppId={{06A28C09-9BB5-47D0-8F43-24BC9019C8E4}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Yu Zhai
VersionInfoVersion={#FileVersion}
VersionInfoProductVersion={#FileVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
VersionInfoOriginalFileName=Scholia-{#AppVersion}-Setup.exe
DefaultDirName={autopf}\Scholia
DefaultGroupName=Scholia
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Scholia-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\bin\scholia.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associatepdf"; Description: "Associate .pdf files with Scholia"; GroupDescription: "File associations:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Scholia"; Filename: "{app}\bin\scholia.exe"
Name: "{autodesktop}\Scholia"; Filename: "{app}\bin\scholia.exe"; Tasks: desktopicon

[Registry]
Root: HKCR; Subkey: ".pdf"; ValueType: string; ValueName: ""; ValueData: "Scholia.Document"; Flags: uninsdeletevalue; Tasks: associatepdf
Root: HKCR; Subkey: "Scholia.Document"; ValueType: string; ValueName: ""; ValueData: "PDF Document"; Flags: uninsdeletekey; Tasks: associatepdf
Root: HKCR; Subkey: "Scholia.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\scholia.exe,0"; Tasks: associatepdf
Root: HKCR; Subkey: "Scholia.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\scholia.exe"" ""%1"""; Tasks: associatepdf

[Run]
Filename: "{app}\bin\scholia.exe"; Description: "Launch Scholia"; Flags: nowait postinstall skipifsilent
