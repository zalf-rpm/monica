;NSIS Modern User Interface
;Start Menu Folder Selection Example Script
;Written by Joost Verburg

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"
	!include "EnvVarUpdate.nsh"

	!ifdef X86
		!define Arch "x86"
		!define WinPlatform "Win32"
		!define ArchBit "32bit"
	!else
		!define Arch "x64"
		!define WinPlatform "x64"
		!define ArchBit "64bit"
	!endif

	!define VCversion "14"

	!define ParamsRepoDir "..\..\monica-parameters"
	!define SysLibsDir "..\..\sys-libs"
  !define UtilDir "..\..\util"

  !include "..\build-number.nsh"

;--------------------------------
;General

  ;Name and file
  Name "MONICA"
  OutFile "MONICA-Setup-2.0.0-beta-${Arch}-${ArchBit}-build${BuildNumber}.exe"

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
  File /oname=monica.exe "..\project-files\${WinPlatform}\Release\monica.exe"  
  File /oname=monica-run.exe "..\project-files\${WinPlatform}\Release\monica-run.exe"  
  File /oname=monica-zmq-run.exe "..\project-files\${WinPlatform}\Release\monica-zmq-run.exe"  
  File /oname=monica-zmq-server.exe "..\project-files\${WinPlatform}\Release\monica-zmq-server.exe"  
  File /oname=libmonica.dll "..\project-files\${WinPlatform}\Release\libmonica.dll"  
	File /oname=monica_python.pyd "..\project-files\${WinPlatform}\Release\monica_python.pyd"  
  File /oname=monica-zmq-control.exe "..\project-files\${WinPlatform}\release\monica-zmq-control.exe"  
  File /oname=monica-zmq-control-send.exe "..\project-files\${WinPlatform}\release\monica-zmq-control-send.exe"  
	File /oname=monica-zmq-control-client.exe "..\project-files\${WinPlatform}\release\monica-zmq-control-client.exe"  
  File /oname=monica-zmq-proxy.exe "..\project-files\${WinPlatform}\release\monica-zmq-proxy.exe"  
	File "C:\Program Files (x86)\Microsoft Visual Studio ${VCversion}.0\VC\redist\${Arch}\Microsoft.VC${VCversion}0.CRT\*.dll"
	File "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\Remote Debugger\${Arch}\api-ms-win-crt-runtime-l1-1-0.dll"
	;File "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\Remote Debugger\${Arch}\api-ms-win-crt-locale-l1-1-0.dll"
  File "${SysLibsDir}\binaries\windows\vc14\${Arch}\libsodium-1.0.10\libsodium.dll"
  File "${SysLibsDir}\binaries\windows\vc14\${Arch}\zeromq-dev-master\libzmq.dll"
	File "${SysLibsDir}\binaries\windows\vc14\${Arch}\boost-python\boost_python-vc140-mt-1_61.dll"
	File "..\LICENSE"
	File "..\documentation\de_benutzerhandbuch_MONICA_windows.pdf"
	File "..\documentation\en_user_manual_MONICA_windows.pdf"
  File "..\documentation\version.txt"
  File "..\src\python\env_json_from_json_config.py"
  File "${UtilDir}\soil\soil_conversion.py"

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

	;create parameter directories
  CreateDirectory "$PROFILE\MONICA\monica-parameters"
	;the json version files with name name convention
	SetOutPath "$PROFILE\MONICA\monica-parameters"
	File /r "${ParamsRepoDir}\crop-residues"
	File /r "${ParamsRepoDir}\crops"
	File /r "${ParamsRepoDir}\mineral-fertilisers"
	File /r "${ParamsRepoDir}\organic-fertilisers"
	File /r "${ParamsRepoDir}\user-parameters"
		
	;the example directory
  CreateDirectory "$PROFILE\MONICA\Examples\Hohenfinow2"
	;the json version files with name name convention
	SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2
	File "Hohenfinow2\crop.json"
	File "Hohenfinow2\site.json"
	File "Hohenfinow2\sim.json"
	File "Hohenfinow2\climate.csv"
	File "Hohenfinow2\climate.ods"
	
	CreateDirectory "$PROFILE\MONICA\Examples\Hohenfinow2\python"
	;example files for the python version
	SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2\python
	File "Hohenfinow2\python\carbiocial.sqlite"
	File "Hohenfinow2\python\db-connections.ini"
	File "Hohenfinow2\python\run-monica.py"
	File "Hohenfinow2\python\run-work-collector.py"
	File "Hohenfinow2\python\run-work-producer.py"
	File "Hohenfinow2\python\site-soil-profile-from-db.json"
	
	
  SetOutPath "$INSTDIR"
  
  ;Store installation folder
  WriteRegStr "HKCU" "Software\MONICA" "" $INSTDIR
  	  
	${EnvVarUpdate} $0 "PATH" "P" "HKCU" "$INSTDIR"
	${EnvVarUpdate} $0 "PYTHONPATH" "P" "HKCU" "$INSTDIR"
	
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
  Delete "$INSTDIR\monica-run.exe"
  Delete "$INSTDIR\monica-zmq-run.exe"
  Delete "$INSTDIR\monica-zmq-server.exe"
  Delete "$INSTDIR\monica-zmq-control.exe"
  Delete "$INSTDIR\monica-zmq-control-send.exe"
  Delete "$INSTDIR\monica-zmq-proxy.exe"
  Delete "$INSTDIR\*.dll"
  
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
	${un.EnvVarUpdate} $0 "PYTHONPATH" "R" "HKCU" "$INSTDIR"
	
SectionEnd
