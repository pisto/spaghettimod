Name "Sauerbraten"

OutFile "sauerbraten_2006_xx_xx_setup.exe"

InstallDir $PROGRAMFILES\Sauerbraten

InstallDirRegKey HKLM "Software\Sauerbraten" "Install_Dir"

SetCompressor /SOLID lzma
XPStyle on

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Sauerbraten (required)"

  SectionIn RO
  
  SetOutPath $INSTDIR
  
  File /r "..\..\*.*"
  
  WriteRegStr HKLM SOFTWARE\Sauerbraten "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sauerbraten" "DisplayName" "Sauerbraten"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sauerbraten" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sauerbraten" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sauerbraten" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

Section "Visual C++ redistributable runtime"

  ExecWait '"$INSTDIR\bin\vcredist_x86.exe"'
  
SectionEnd

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Sauerbraten"
  
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$INSTDIR\Sauerbraten.lnk"                "$INSTDIR\sauerbraten.bat" "" "$INSTDIR\sauerbraten.bat" 0
  CreateShortCut "$SMPROGRAMS\Sauerbraten\Sauerbraten.lnk" "$INSTDIR\sauerbraten.bat" "" "$INSTDIR\sauerbraten.bat" 0
  CreateShortCut "$SMPROGRAMS\Sauerbraten\Uninstall.lnk"   "$INSTDIR\uninstall.exe"   "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Sauerbraten\README.lnk"      "$INSTDIR\README.html"     "" "$INSTDIR\README.html" 0
  
SectionEnd

Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Sauerbraten"
  DeleteRegKey HKLM SOFTWARE\Sauerbraten

  RMDir /r "$SMPROGRAMS\Sauerbraten"
  RMDir /r "$INSTDIR"

SectionEnd
