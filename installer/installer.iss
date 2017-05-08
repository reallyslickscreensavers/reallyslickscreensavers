; -----------------------------------------------------------------------------
;
; Script for building an installer for Really Slick Screensavers
;
; -----------------------------------------------------------------------------

; information defines
#define ApplicationName "Really Slick Screensavers"
#define ApplicationShortName "ReallySlickScreensavers"
#define ApplicationLowerName LowerCase(ApplicationShortName)
#define ApplicationPublisher "Terence M. Welsh"
#define ApplicationUrl "http://www.reallyslick.com"

; read product version string from main executable
#define ApplicationVersion "0.2-beta" ; TODO: Make it dynamically updated with tag and build number

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{8CF886E0-9BCA-4D5D-8F9C-6DEA318D0E71}
AppName={#ApplicationName}
AppVersion={#ApplicationVersion}
AppPublisher={#ApplicationPublisher}
AppPublisherURL={#ApplicationUrl}
AppSupportURL={#ApplicationUrl}
AppUpdatesURL={#ApplicationUrl}
DefaultDirName={sys}
DefaultGroupName={#ApplicationName}
Compression=lzma
SolidCompression=yes

; root directory
SourceDir=.\..\bin

; output dir is relative to SourceDir
OutputDir=.\..\installer\output

; output installer file name
OutputBaseFilename={#ApplicationShortName + "_v" + ApplicationVersion}

UninstallDisplayIcon="installer.ico"
UninstallDisplayName={#ApplicationName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "cyclone.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "euphoria.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "fieldlines.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "flocks.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "flux.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "helios.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "hyperspace.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "lattice.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "microcosm.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "plasma.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "skyrocket.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "solarwinds.scr"; DestDir: "{app}"; Flags: ignoreversion
