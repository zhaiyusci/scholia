#define AppName "Scholia"
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
#define SourceDir GetEnv("SCHOLIA_STAGE")
#if SourceDir == ""
#define SourceDir "..\..\..\dist\scholia-pdf\app"
#endif
#define OutputDir GetEnv("SCHOLIA_OUTPUT")
#if OutputDir == ""
#define OutputDir "..\..\..\dist"
#endif
#define StemTeXSupportUrl GetEnv("SCHOLIA_STEMTEX_SUPPORT_URL")
#if StemTeXSupportUrl == ""
#define StemTeXSupportUrl "https://github.com/zhaiyusci/scholia/releases/download/v" + AppVersion + "/Scholia-" + AppVersion + "-StemTeX-Support.exe"
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
SetupIconFile={#AppIconFile}
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\bin\scholia.exe
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associatepdf"; Description: "Associate .pdf files with Scholia"; GroupDescription: "File associations:"
Name: "stemtexsupport"; Description: "Install bundled StemTeX TeX tree support package"; GroupDescription: "Optional downloads:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Scholia"; Filename: "{app}\bin\scholia.exe"; IconFilename: "{app}\bin\scholia.ico"
Name: "{autodesktop}\Scholia"; Filename: "{app}\bin\scholia.exe"; IconFilename: "{app}\bin\scholia.ico"; Tasks: desktopicon

[Registry]
Root: HKCR; Subkey: ".pdf"; ValueType: string; ValueName: ""; ValueData: "Scholia.Document"; Flags: uninsdeletevalue; Tasks: associatepdf
Root: HKCR; Subkey: "Scholia.Document"; ValueType: string; ValueName: ""; ValueData: "PDF Document"; Flags: uninsdeletekey; Tasks: associatepdf
Root: HKCR; Subkey: "Scholia.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\application-pdf.ico"; Tasks: associatepdf
Root: HKCR; Subkey: "Scholia.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\scholia.exe"" ""%1"""; Tasks: associatepdf

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ Runtime..."; Flags: waituntilterminated runhidden; Check: NeedsMsvcRuntime
Filename: "{tmp}\Scholia-StemTeX-Support.exe"; Parameters: "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR=""{app}"""; StatusMsg: "Installing StemTeX support package..."; Flags: waituntilterminated runhidden; Check: ShouldInstallStemTeXSupport
Filename: "{app}\bin\scholia.exe"; Description: "Launch Scholia"; Flags: nowait postinstall skipifsilent

[Code]
var
  DownloadPage: TDownloadWizardPage;

function NeedsMsvcRuntime: Boolean;
var
  Installed: Cardinal;
begin
  Result := not (RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1));
end;

function ShouldInstallStemTeXSupport: Boolean;
begin
  Result := WizardIsTaskSelected('stemtexsupport');
end;

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), SetupMessage(msgPreparingDesc), nil);
  DownloadPage.ShowBaseNameInsteadOfUrl := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Error: String;
begin
  Result := True;
  if (CurPageID = wpReady) and (NeedsMsvcRuntime or ShouldInstallStemTeXSupport) then begin
    DownloadPage.Clear;
    if NeedsMsvcRuntime then
      DownloadPage.Add('https://aka.ms/vs/17/release/vc_redist.x64.exe', 'vc_redist.x64.exe', '');
    if ShouldInstallStemTeXSupport then
      DownloadPage.Add('{#StemTeXSupportUrl}', 'Scholia-StemTeX-Support.exe', '');
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
      except
        if DownloadPage.AbortedByUser then
          Log('MSVC runtime download was aborted by user.')
        else begin
          Error := Format('%s: %s', [DownloadPage.LastBaseNameOrUrl, GetExceptionMessage]);
          SuppressibleMsgBox(AddPeriod(Error), mbCriticalError, MB_OK, IDOK);
        end;
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end;
end;
