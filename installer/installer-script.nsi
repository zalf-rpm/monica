;NSIS Modern User Interface
;Start Menu Folder Selection Example Script
;Written by Joost Verburg

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

	!ifdef X86
		!define Arch "x86"
		!define ArchBit "32bit"
	!else
		!define Arch "x64"
		!define ArchBit "64bit"
	!endif
	
	!ifdef HERMES
		!define MonicaVersion "hermes"
	!else
		!define MonicaVersion "json"
	!endif
	
	!ifdef VC11
		!define VCversion "11"
	!else
		!define VCversion "12"
	!endif
	
;--------------------------------
;General

  ;Name and file
  Name "MONICA"
  OutFile "MONICA-Setup-1.2-${Arch}-${ArchBit}.exe"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\MONICA"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\MONICA" ""

  ;Request application privileges for Windows Vista
  RequestExecutionLevel admin

;--------------------------------
;Variables

  Var StartMenuFolder

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_LEFT
  !define MUI_HEADERIMAGE_BITMAP "orange.bmp"
  !define ICON		"monica_icon.ico"
  !define MUI_ICON	"monica_icon.ico"

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "license.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  
  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\MONICA" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  
  !insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder
  
  !insertmacro MUI_PAGE_INSTFILES
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "German"

;--------------------------------
;Installer Sections

Section "MONICA - Model for Nitrogen and Carbon in Agro-ecosystems" SecDummy

	;SetOutPath "c:\temp"
	;install Visual C++ 2012 runtime
	;File "vcredist_${Arch}.exe"
	;ExecWait "c:\temp\vcredist_${ARCH}.exe /q:a"
	;Delete "c:\temp\vcredist_${ARCH}.exe"

  SetOutPath "$INSTDIR"
  
  ;ADD YOUR OWN FILES HERE...
  File /oname=monica.exe "..\release\monica-${MonicaVersion}.exe"  
	File "C:\Program Files (x86)\Microsoft Visual Studio ${VCversion}.0\VC\redist\${Arch}\Microsoft.VC${VCversion}0.CRT\*.dll"
	File "..\db-connections.ini"
	File "license.txt"
	File "..\documentation\de_benutzerhandbuch_MONICA_windows.pdf"
	File "..\documentation\en_user_manual_MONICA_windows.pdf"
  	File "..\documentation\version.txt"
  
  ;CreateDirectory "$PROFILE\MONICA\sqlite-db"
  SetOutPath $INSTDIR\sqlite-db
  File "..\sqlite-db\monica.sqlite"
    
  CreateDirectory $PROFILE\MONICA\Examples\Hohenfinow2
  SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2
  File "Hohenfinow2\monica.ini"
  File "Hohenfinow2\Irrig.TXT"
  File "Hohenfinow2\MET_HF.991"
  File "Hohenfinow2\MET_HF.992"
  File "Hohenfinow2\MET_HF.993"
  File "Hohenfinow2\MET_HF.994"
  File "Hohenfinow2\MET_HF.995"
  File "Hohenfinow2\MET_HF.996"
  File "Hohenfinow2\MET_HF.997"
  
  File "Hohenfinow2\ROTATION.txt"
  File "Hohenfinow2\SLAGDUNG.txt"
  File "Hohenfinow2\SOIL.txt"

  CreateDirectory $PROFILE\MONICA\Examples\Hohenfinow2\json
  SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2\json
  File "Hohenfinow2\json\test.crop.json"
  File "Hohenfinow2\json\test.sim.json"
  File "Hohenfinow2\json\test.site.json"
  	
  SetOutPath "$INSTDIR"
  
  ;Store installation folder
  WriteRegStr HKCU "Software\MONICA" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
	;Create shortcuts
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"	
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\MONICA.lnk" "$INSTDIR\monica.exe" "$\"%USERPROFILE%$\"\MONICA\Examples\Hohenfinow2"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Documentation MONICA for Windows.lnk" "$INSTDIR\en_user_manual_MONICA_windows.pdf"	
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Dokumentation MONICA for Windows.lnk" "$INSTDIR\de_benutzerhandbuch_MONICA_windows.pdf"
	;CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Modellbeschreibung MONICA for Windows.lnk" "$INSTDIR\Modellbeschreibung_MONICA_Version_1-1-1.pdf"		
	CreateShortCut "$DESKTOP\MONICA.lnk" "$INSTDIR\monica.exe" "$\"%USERPROFILE%$\"\MONICA\Examples\Hohenfinow2"
	
  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_SecDummy ${LANG_GERMAN} "MONICA - Model for Nitrogen and Carbon in Agro-ecosystems"

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDummy} $(DESC_SecDummy)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"

  Delete "$INSTDIR\monica.exe"
  Delete "$INSTDIR\db-connections.ini"
  ;Delete "$INSTDIR\*.dll"
  ;Delete "$INSTDIR\climate.dll"
  ;Delete "$INSTDIR\tools.dll"
  ;Delete "$INSTDIR\db.dll"
  ;Delete "$INSTDIR\libmysql.dll"  
  ;Delete "$INSTDIR\mingwm10.dll"  
  ;Delete "$INSTDIR\libgcc_s_dw2-1.dll"
  Delete "$INSTDIR\gmon.out"
  Delete "$INSTDIR\license.txt"
  ;Delete "$INSTDIR\Readme.pdf"
	Delete "$INSTDIR\en_user_manual_MONICA_windows.pdf"	
	Delete "$INSTDIR\de_benutzerhandbuch_MONICA_windows.pdf"
	Delete "$INSTDIR\version.txt"
	
  Delete "$INSTDIR\sqlite-db\monica.sqlite"
  RMDir $INSTDIR\sqlite-db
  
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\monica.ini
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\Irrig.TXT
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\MET_HF.991
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\MET_HF.992
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\MET_HF.993
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\MET_HF.994
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\MET_HF.995
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\MET_HF.996
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\MET_HF.997  
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\ROTATION.txt
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\rmout.dat
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\smout.dat
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\gmon.out
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\SLAGDUNG.txt
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\SOIL.txt
  
  RMDir  $PROFILE\MONICA\Examples\Hohenfinow2
  RMDir  $PROFILE\MONICA\Examples
  RMDir  $PROFILE\MONICA
  
  Delete "$INSTDIR\Uninstall.exe"

  RMDir "$INSTDIR"
  
  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
    
  Delete "$SMPROGRAMS\$StartMenuFolder\MONICA.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Readme.lnk"
  Delete "$DESKTOP\MONICA.lnk"
  RMDir "$SMPROGRAMS\$StartMenuFolder"
  
  DeleteRegKey /ifempty HKCU "Software\MONICA"

SectionEnd
