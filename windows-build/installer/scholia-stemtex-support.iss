#define AppName "Scholia StemTeX Support"
#define AppIconFile AddBackslash(SourcePath) + "..\..\icons\scholia.ico"
#define ScholiaVersionFile AddBackslash(SourcePath) + "..\..\VERSION.txt"
#define ScholiaVersionHandle FileOpen(ScholiaVersionFile)
#define ScholiaVersionFromFile Trim(FileRead(ScholiaVersionHandle))
#expr FileClose(ScholiaVersionHandle)
#define AppVersion GetEnv("SCHOLIA_VERSION")
#if AppVersion == ""
#define AppVersion ScholiaVersionFromFile
#endif
#define FileVersion GetEnv("SCHOLIA_FILE_VERSION")
#if FileVersion == ""
#define FileVersion ScholiaVersionFromFile + ".0"
#endif
#define SourceDir GetEnv("SCHOLIA_SUPPORT_STAGE")
#if SourceDir == ""
#define SourceDir "..\..\..\dist\scholia-stemtex-support\app"
#endif
#define OutputDir GetEnv("SCHOLIA_OUTPUT")
#if OutputDir == ""
#define OutputDir "..\..\..\dist"
#endif

[Setup]
AppId={{71E9EF5B-AE3D-48E0-A524-F11BC3FB29B7}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Yu Zhai
VersionInfoVersion={#FileVersion}
VersionInfoProductVersion={#FileVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
VersionInfoOriginalFileName=Scholia-{#AppVersion}-StemTeX-Support.exe
DefaultDirName={autopf}\Scholia
DefaultGroupName=Scholia
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Scholia-{#AppVersion}-StemTeX-Support
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#AppIconFile}
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\bin\scholia.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
