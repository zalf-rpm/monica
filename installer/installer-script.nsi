;NSIS Modern User Interface
;Start Menu Folder Selection Example Script
;Written by Joost Verburg

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"
	!include "EnvVarUpdate.nsh"

	!ifdef X86
		!define Arch "x86"
		!define ArchBit "32bit"
	!else
		!define Arch "x64"
		!define ArchBit "64bit"
	!endif

	!define VCversion "14"

;--------------------------------
;General

  ;Name and file
  Name "MONICA"
  OutFile "MONICA-Setup-2.1-${Arch}-${ArchBit}.exe"

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
  !insertmacro MUI_PAGE_LICENSE "..\LICENSE"
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
  File /oname=monica.exe "..\project-files\release\monica.exe"  
	File "C:\Program Files (x86)\Microsoft Visual Studio ${VCversion}.0\VC\redist\${Arch}\Microsoft.VC${VCversion}0.CRT\*.dll"
	File "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\Remote Debugger\${Arch}\api-ms-win-crt-runtime-l1-1-0.dll"
	File "..\LICENSE"
	File "..\documentation\de_benutzerhandbuch_MONICA_windows.pdf"
	File "..\documentation\en_user_manual_MONICA_windows.pdf"
  File "..\documentation\version.txt"

	;SetOutPath $INSTDIR\meta.json
  ;File "..\meta.json\meta.crop.json"
  ;File "..\meta.json\meta.sim.json"
  ;File "..\meta.json\meta.site.json"
  ;File "..\meta.json\README.md"
		
	CreateDirectory "$PROFILE\MONICA"
	SetOutPath $PROFILE\MONICA
	File /oname=db-connections.ini "..\db-connections-install.ini"
	File "..\LICENSE"
	  
  CreateDirectory "$PROFILE\MONICA\sqlite-db"
  SetOutPath "$PROFILE\MONICA\sqlite-db"
  File "..\sqlite-db\monica.sqlite"
	File "..\sqlite-db\ka5-soil-data.sqlite"
  	
	;the example directory
  CreateDirectory "$PROFILE\MONICA\Examples\Hohenfinow2"
	;the json version files with name name convention
	SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2
	File "Hohenfinow2\crop.json"
	File "Hohenfinow2\site.json"
	File "Hohenfinow2\sim.json"
	File "Hohenfinow2\climate.csv"
	File "Hohenfinow2\climate.ods"

	;the json version files with project name prepended
  CreateDirectory $PROFILE\MONICA\Examples\Hohenfinow2\json-from-db
  SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2\json-from-db
  File "Hohenfinow2\json-from-db\test.crop.json"
  File "Hohenfinow2\json-from-db\test.sim.json"
  File "Hohenfinow2\json-from-db\test.site.json"
	File "Hohenfinow2\json-from-db\test.climate.csv"
  
	;the old hermes files
  SetOutPath "$PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files"
	File "Hohenfinow2\old-hermes-format-files\monica.ini"
  File "Hohenfinow2\old-hermes-format-files\Irrig.TXT"
  File "Hohenfinow2\old-hermes-format-files\MET_HF.991"
  File "Hohenfinow2\old-hermes-format-files\MET_HF.992"
  File "Hohenfinow2\old-hermes-format-files\MET_HF.993"
  File "Hohenfinow2\old-hermes-format-files\MET_HF.994"
  File "Hohenfinow2\old-hermes-format-files\MET_HF.995"
  File "Hohenfinow2\old-hermes-format-files\MET_HF.996"
  File "Hohenfinow2\old-hermes-format-files\MET_HF.997"
  File "Hohenfinow2\old-hermes-format-files\ROTATION.txt"
  File "Hohenfinow2\old-hermes-format-files\SLAGDUNG.txt"
  File "Hohenfinow2\old-hermes-format-files\SOIL.txt"
		
  SetOutPath "$INSTDIR"
  
  ;Store installation folder
  WriteRegStr "HKCU" "Software\MONICA" "" $INSTDIR
  	  
	${EnvVarUpdate} $0 "PATH" "P" "HKCU" "$INSTDIR"
	
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
	;Create shortcuts
	CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
	SetOutPath "$PROFILE\MONICA"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\MONICA.lnk" "$INSTDIR\monica.exe" "--debug Examples\Hohenfinow2\sim.json"
	CreateShortCut "$DESKTOP\MONICA.lnk" "$INSTDIR\monica.exe" "--debug Examples\Hohenfinow2\sim.json"
	
	SetOutPath "$INSTDIR"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Documentation MONICA for Windows.lnk" "$INSTDIR\en_user_manual_MONICA_windows.pdf"	
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Dokumentation MONICA for Windows.lnk" "$INSTDIR\de_benutzerhandbuch_MONICA_windows.pdf"
	;CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Modellbeschreibung MONICA for Windows.lnk" "$INSTDIR\Modellbeschreibung_MONICA_Version_1-1-1.pdf"
				
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
  Delete "$INSTDIR\*.dll"
  ;Delete "$INSTDIR\climate.dll"
  ;Delete "$INSTDIR\tools.dll"
  ;Delete "$INSTDIR\db.dll"
  ;Delete "$INSTDIR\libmysql.dll"  
  ;Delete "$INSTDIR\mingwm10.dll"  
  ;Delete "$INSTDIR\libgcc_s_dw2-1.dll"
  ;Delete "$INSTDIR\gmon.out"
  Delete "$INSTDIR\LICENSE"
  ;Delete "$INSTDIR\Readme.pdf"
	Delete "$INSTDIR\en_user_manual_MONICA_windows.pdf"	
	Delete "$INSTDIR\de_benutzerhandbuch_MONICA_windows.pdf"
	Delete "$INSTDIR\version.txt"
	
	
	Delete "$PROFILE\MONICA\LICENSE"
  Delete "$PROFILE\MONICA\db-connections.ini"
  Delete "$PROFILE\MONICA\sqlite-db\monica.sqlite"
	Delete "$PROFILE\MONICA\sqlite-db\ka5-soil-data.sqlite"
  RMDir "$PROFILE\MONICA\sqlite-db"
  
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\monica.ini
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\Irrig.TXT
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\MET_HF.991
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\MET_HF.992
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\MET_HF.993
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\MET_HF.994
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\MET_HF.995
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\MET_HF.996
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\MET_HF.997  
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\ROTATION.txt
  ;Delete $PROFILE\MONICA\Examples\Hohenfinow2\rmout.csv
  ;Delete $PROFILE\MONICA\Examples\Hohenfinow2\smout.csv
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\SLAGDUNG.txt
  Delete $PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files\SOIL.txt
	RMDir "$PROFILE\MONICA\Examples\Hohenfinow2\old-hermes-format-files"
	
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\json-from-db\test.climate.csv"
  Delete "$PROFILE\MONICA\Examples\Hohenfinow2\json-from-db\test.sim.json"
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\json-from-db\test.crop.json"
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\json-from-db\test.site.json"
	RMDir "$PROFILE\MONICA\Examples\Hohenfinow2\json-from-db"
	
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\climate.ods"
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\climate.csv"
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\sim.json"
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\crop.json"
	Delete "$PROFILE\MONICA\Examples\Hohenfinow2\site.json"
	
  RMDir  $PROFILE\MONICA\Examples\Hohenfinow2
  RMDir  $PROFILE\MONICA\Examples
  RMDir  $PROFILE\MONICA
  
  Delete "$INSTDIR\Uninstall.exe"

  RMDir "$INSTDIR"
  
  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
    
  Delete "$SMPROGRAMS\$StartMenuFolder\MONICA.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Readme.lnk"
	Delete "$SMPROGRAMS\$StartMenuFolder\Documentation MONICA for Windows.lnk"
	Delete "$SMPROGRAMS\$StartMenuFolder\Dokumentation MONICA for Windows.lnk"
	Delete "$DESKTOP\MONICA.lnk"
  RMDir "$SMPROGRAMS\$StartMenuFolder"
  
  DeleteRegKey /ifempty HKCU "Software\MONICA"

	${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR"
	
SectionEnd
