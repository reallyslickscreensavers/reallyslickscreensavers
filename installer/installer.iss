; -----------------------------------------------------------------------------
;
; Script for building an installer for Really Slick Screensavers
;
; -----------------------------------------------------------------------------
[ISPP]
#define AppName "Really Slick Screensavers"
#define AppShortName "ReallySlickScreensavers"
#define AppLowerName LowerCase(AppShortName)
#define AppPublisher "Terence M. Welsh"
#define AppUrl "http://www.reallyslick.com"

#ifndef VERSION
#define VERSION GetEnv('APPVEYOR_BUILD_VERSION')
#if VERSION == "."
  #define VERSION = "v0.0.0"
#endif
#pragma message "Detected Version: " + VERSION
#endif

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{699C3D60-79CD-42BE-9B08-95C23AF48DF4}
AppName={#AppName}
AppVersion={#VERSION}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}
AppUpdatesURL={#AppUrl}
DefaultDirName={sys}
DefaultGroupName={#AppName}
Compression=lzma
SolidCompression=yes

; root directory
SourceDir=.\..\bin

; output dir is relative to SourceDir
OutputDir=.\..\installer\output

; output installer file name
OutputBaseFilename={#AppLowerName}_{#VERSION}

UninstallDisplayIcon="installer.ico"
UninstallDisplayName={#AppName}

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
