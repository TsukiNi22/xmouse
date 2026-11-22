; Edition:
;;  22/11/2026 by Tsukini
;
; File Name:
;;  xmouse.nsi
;
; File Description:
;;  Windows installer of xmouse (NSIS): the files of the bundle, Start menu, firewall of the private networks,
;;  start with Windows (optional), Add or remove programs, uninstaller
;;  makensis -DVERSION=x.y.z -DSOURCE=<bundle folder> -DOUTFILE=<setup.exe> packaging/windows/xmouse.nsi

Unicode true
SetCompressor /SOLID lzma
SetCompressorDictSize 64

!ifndef VERSION
    !define VERSION "0.0.0"
!endif
!ifndef SOURCE
    !error "SOURCE (folder of the bundle: xmouse.exe and its dll) is required"
!endif
!ifndef OUTFILE
    !define OUTFILE "xmouse-windows-x64-setup.exe"
!endif
!define PORT 47900 ; tls + websocket port of xmouse
!define DISCOVERY_PORT 47901 ; udp announces on the LAN
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\xmouse"

!include "MUI2.nsh"
!include "x64.nsh"

Name "xmouse ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\xmouse"
InstallDirRegKey HKLM "Software\xmouse" "InstallDir"
RequestExecutionLevel admin
BrandingText "xmouse ${VERSION}"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "xmouse"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "xmouse installer"
VIAddVersionKey "CompanyName" "TsukiNi22"
VIAddVersionKey "LegalCopyright" "MIT"

!define MUI_ICON "${__FILEDIR__}\xmouse.ico"
!define MUI_UNICON "${__FILEDIR__}\xmouse.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\xmouse.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS "--gui"
!define MUI_FINISHPAGE_RUN_TEXT "Start xmouse"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE}\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "xmouse needs a 64 bits Windows."
        Abort
    ${EndIf}
    SetRegView 64
    SetShellVarContext all
FunctionEnd

Function un.onInit
    SetRegView 64
    SetShellVarContext all
FunctionEnd

Section "xmouse" SecMain
    SectionIn RO
    nsExec::Exec 'taskkill /F /IM xmouse.exe' ; an update: the running one stops first
    Pop $0
    SetOutPath "$INSTDIR"
    File /r "${SOURCE}\*"
    File "/oname=xmouse.ico" "${__FILEDIR__}\xmouse.ico"
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Start menu
    CreateDirectory "$SMPROGRAMS\xmouse"
    CreateShortcut "$SMPROGRAMS\xmouse\xmouse.lnk" "$INSTDIR\xmouse.exe" "--gui" "$INSTDIR\xmouse.ico"
    CreateShortcut "$SMPROGRAMS\xmouse\Uninstall xmouse.lnk" "$INSTDIR\uninstall.exe"

    ; Add or remove programs
    WriteRegStr HKLM "Software\xmouse" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "xmouse"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "TsukiNi22"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "URLInfoAbout" "https://github.com/TsukiNi22/xmouse"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\xmouse.ico"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1

    ; The members of the private networks reach this PC (never the public ones)
    nsExec::Exec 'netsh advfirewall firewall delete rule name=xmouse'
    Pop $0
    nsExec::Exec 'netsh advfirewall firewall add rule name=xmouse dir=in action=allow program="$INSTDIR\xmouse.exe" protocol=TCP localport=${PORT} profile=private,domain'
    Pop $0
    nsExec::Exec 'netsh advfirewall firewall add rule name=xmouse dir=in action=allow program="$INSTDIR\xmouse.exe" protocol=UDP localport=${DISCOVERY_PORT} profile=private,domain'
    Pop $0
    nsExec::Exec 'netsh advfirewall firewall add rule name=xmouse dir=in action=allow program="$INSTDIR\xmouse.exe" protocol=UDP localport=${PORT} profile=private,domain' ; the encrypted udp of the moves
    Pop $0
SectionEnd

Section /o "Start with Windows" SecAutostart
    CreateShortcut "$SMSTARTUP\xmouse.lnk" "$INSTDIR\xmouse.exe" "--gui" "$INSTDIR\xmouse.ico"
SectionEnd

LangString DESC_MAIN ${LANG_ENGLISH} "xmouse, its libraries, the Start menu and the firewall rule of the private networks."
LangString DESC_MAIN ${LANG_FRENCH} "xmouse, ses bibliothèques, le menu Démarrer et la règle du pare-feu des réseaux privés."
LangString DESC_AUTOSTART ${LANG_ENGLISH} "Start xmouse in the tray at every login."
LangString DESC_AUTOSTART ${LANG_FRENCH} "Lancer xmouse dans la zone de notification à chaque connexion."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} $(DESC_MAIN)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAutostart} $(DESC_AUTOSTART)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
    nsExec::Exec 'taskkill /F /IM xmouse.exe'
    Pop $0
    nsExec::Exec 'netsh advfirewall firewall delete rule name=xmouse'
    Pop $0
    Delete "$SMSTARTUP\xmouse.lnk"
    RMDir /r "$SMPROGRAMS\xmouse"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKLM "${UNINSTALL_KEY}"
    DeleteRegKey HKLM "Software\xmouse"
    ; the settings, the network and the logs stay in %LOCALAPPDATA%\xmouse and %APPDATA%\xmouse
SectionEnd
