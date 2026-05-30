#define AppName "Okular PDF"
#define BuildDate GetDateTimeString("yyyymmdd", "", "")
#define BuildFileDate GetDateTimeString("mmdd", "", "")
#define AppVersion GetEnv("OKULAR_PDF_VERSION")
#if AppVersion == ""
#define AppVersion "26.07.70-patch" + BuildDate
#endif
#define FileVersion GetEnv("OKULAR_PDF_FILE_VERSION")
#if FileVersion == ""
#define FileVersion "26.7.70." + BuildFileDate
#endif
#define SourceDir GetEnv("OKULAR_PDF_STAGE")
#if SourceDir == ""
#define SourceDir "..\..\..\dist\okular-pdf-only\app"
#endif
#define OutputDir GetEnv("OKULAR_PDF_OUTPUT")
#if OutputDir == ""
#define OutputDir "..\..\..\dist"
#endif

[Setup]
AppId={{7B84094E-594F-49ED-9F73-4B30FA4A01D8}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Yu Zhai
VersionInfoVersion={#FileVersion}
VersionInfoProductVersion={#FileVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
VersionInfoOriginalFileName=Okular-PDF-{#AppVersion}-Setup.exe
DefaultDirName={autopf}\Okular PDF
DefaultGroupName=Okular PDF
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Okular-PDF-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\bin\okular.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associatepdf"; Description: "Associate .pdf files with Okular PDF"; GroupDescription: "File associations:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Okular PDF"; Filename: "{app}\bin\okular.exe"
Name: "{autodesktop}\Okular PDF"; Filename: "{app}\bin\okular.exe"; Tasks: desktopicon

[Registry]
Root: HKCR; Subkey: ".pdf"; ValueType: string; ValueName: ""; ValueData: "OkularPDF.Document"; Flags: uninsdeletevalue; Tasks: associatepdf
Root: HKCR; Subkey: "OkularPDF.Document"; ValueType: string; ValueName: ""; ValueData: "PDF Document"; Flags: uninsdeletekey; Tasks: associatepdf
Root: HKCR; Subkey: "OkularPDF.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\okular.exe,0"; Tasks: associatepdf
Root: HKCR; Subkey: "OkularPDF.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\okular.exe"" ""%1"""; Tasks: associatepdf

[Run]
Filename: "{app}\bin\okular.exe"; Description: "Launch Okular PDF"; Flags: nowait postinstall skipifsilent
