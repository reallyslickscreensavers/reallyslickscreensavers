; Script for building an installer for Really Slick Screensavers

;--------------------------------

Function .onInit
  System::Call 'kernel32::CreateMutexA(i 0, i 0, t "rssaversInstallerMutex") i .r1 ?e'
  Pop $R0
 
  StrCmp $R0 0 +3
    MessageBox MB_OK|MB_ICONEXCLAMATION "The installer for Really Slick Screensavers is already running."
    Abort

  MessageBox MB_YESNO "This will install Really Slick Screensavers.$\n$\nThe following screensavers will be installed:$\nCyclone$\nEuphoria$\nField Lines$\nFlocks$\nFlux$\nHelios$\nHyperspace$\nLattice$\nMicrocosm$\nPlasma$\nSkyrocket$\nSolar Winds" IDYES NoAbort
    Abort ; causes installer to quit.
  NoAbort:

  MessageBox MB_OK|MB_ICONINFORMATION "During this installation, you will be prompted to install OpenAL 1.1 for$\nsound.  The Skyrocket screensaver requires OpenAL and will not$\nfunction without it.  It is recommended that you install OpenAL."
FunctionEnd

;--------------------------------

; The name of the installer
Name "Really Slick Screensavers 0.2 beta"

Icon "installer.ico"

; The file to write
OutFile "ReallySlickScreensavers-0.2_installer.exe"

; The default installation directory
InstallDir "$PROGRAMFILES\ReallySlickScreensavers"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\ReallySlickScreensavers" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "Installing Really Slick Screensavers 0.2"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File "Online instructions.url"
  File "Really Slick website.url"
  File "urlicon.ico"
  File "GPL.txt"
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\ReallySlickScreensavers "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ReallySlickScreensavers" "DisplayName" "Really Slick Screensavers 0.2"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ReallySlickScreensavers" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ReallySlickScreensavers" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ReallySlickScreensavers" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

  ; Set output path to C:\Windows\System32 or equivalent directory depending on Windows version
  SetOutPath $SYSDIR

  File "Cyclone.scr"
  File "Euphoria.scr"
  File "FieldLines.scr"
  File "Flocks.scr"
  File "Flux.scr"
  File "Helios.scr"
  File "Hyperspace.scr"
  File "Lattice.scr"
  File "Microcosm.scr"
  File "Plasma.scr"
  File "Skyrocket.scr"
  File "SolarWinds.scr"

SectionEnd

; Installs OpenAL runtime libraries, using Creative's installer
Section "OpenAL"

  SetOutPath "$INSTDIR"
  File "oalinst.exe"
  ExecWait '$INSTDIR\oalinst.exe'
  Delete $INSTDIR\oalinst.exe

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Really Slick Screensavers"
  CreateShortCut "$SMPROGRAMS\Really Slick Screensavers\Online instructions.lnk" "$INSTDIR\Online instructions.url" "" "$INSTDIR\urlicon.ico" 0
  CreateShortCut "$SMPROGRAMS\Really Slick Screensavers\Really Slick website.lnk" "$INSTDIR\Really Slick website.url" "" "$INSTDIR\urlicon.ico" 0
  CreateShortCut "$SMPROGRAMS\Really Slick Screensavers\License.lnk" "notepad.exe" "$INSTDIR\GPL.txt" 0
  CreateShortCut "$SMPROGRAMS\Really Slick Screensavers\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ReallySlickScreensavers"
  DeleteRegKey HKLM SOFTWARE\ReallySlickScreensavers
  DeleteRegKey HKCU "Software\Really Slick\Cyclone"
  DeleteRegKey HKCU "Software\Really Slick\Euphoria"
  DeleteRegKey HKCU "Software\Really Slick\FieldLines"
  DeleteRegKey HKCU "Software\Really Slick\Flocks"
  DeleteRegKey HKCU "Software\Really Slick\Flux"
  DeleteRegKey HKCU "Software\Really Slick\Helios"
  DeleteRegKey HKCU "Software\Really Slick\Hyperspace"
  DeleteRegKey HKCU "Software\Really Slick\Lattice"
  DeleteRegKey HKCU "Software\Really Slick\Microcosm"
  DeleteRegKey HKCU "Software\Really Slick\Plasma"
  DeleteRegKey HKCU "Software\Really Slick\Skyrocket"
  DeleteRegKey HKCU "Software\Really Slick\Solar Winds"

  ; Remove files and uninstaller
  Delete $SYSDIR\Cyclone.scr
  Delete $SYSDIR\Euphoria.scr
  Delete $SYSDIR\FieldLines.scr
  Delete $SYSDIR\Flocks.scr
  Delete $SYSDIR\Flux.scr
  Delete $SYSDIR\Helios.scr
  Delete $SYSDIR\Hyperspace.scr
  Delete $SYSDIR\Lattice.scr
  Delete $SYSDIR\Microcosm.scr
  Delete $SYSDIR\Plasma.scr
  Delete $SYSDIR\Skyrocket.scr
  Delete $SYSDIR\SolarWinds.scr
  Delete "$INSTDIR\Online instructions.url"
  Delete "$INSTDIR\Really Slick website.url"
  Delete $INSTDIR\urlicon.ico
  Delete $INSTDIR\GPL.txt
  Delete $INSTDIR\uninstall.exe

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\Really Slick Screensavers\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\Really Slick Screensavers"
  RMDir "$INSTDIR"

SectionEnd
