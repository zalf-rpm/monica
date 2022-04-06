;NSIS Modern User Interface

;--------------------------------
;Include Modern UI

	!include "MUI2.nsh"
	!include "EnvVarUpdate.nsh"
	
	!ifdef X86
		!define Arch "x86"
		!define WinPlatform "Win32"
		!define ArchBit "32bit"
		#!define BoostBit "x32"
		!define ExecutableFolder "_cmake_win32\Release"
	!else
		!define Arch "x64"
		!define WinPlatform "x64"
		!define ArchBit "64bit"
		#!define BoostBit "x64"
		!define ExecutableFolder "_cmake_win64\Release"
	!endif

	!define ParamsRepoDir "..\..\monica-parameters"
	!define SysLibsDir "..\..\sys-libs"
	!define UtilDir "..\..\util"

	!ifndef VersionNumber
		!define VersionNumber "2.0.2"
	!endif
	!ifndef BuildNumber
		!include "..\build-number.nsh"
	!endif

;--------------------------------
;General

	;Name and file
	Name "MONICA"
	OutFile "MONICA-Setup-${VersionNumber}-${Arch}-${ArchBit}-build${BuildNumber}.exe"
	
	;Request application privileges for Windows Vista and higher
	RequestExecutionLevel user
	
	Function .onInit
	
		; Must set $INSTDIR here to avoid adding ${MUI_PRODUCT} to the end of the
		; path when user selects a new directory using the 'Browse' button.
		StrCpy $INSTDIR "$LOCALAPPDATA\MONICA"	
	
	FunctionEnd

	;Get installation folder from registry if available
	InstallDirRegKey HKCU "Software\MONICA" ""


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

	SetOutPath "$INSTDIR"
	
	;ADD YOUR OWN FILES HERE...
	;File /oname=monica.exe "..\${ExecutableFolder}\monica.exe"
	File /oname=monica-run.exe "..\${ExecutableFolder}\monica-run.exe"
	;File /oname=monica-zmq-run.exe "..\${ExecutableFolder}\monica-zmq-run.exe"
	File /oname=monica-zmq-server.exe "..\${ExecutableFolder}\monica-zmq-server.exe"
	;File /oname=monica-zmq-control.exe "..\${ExecutableFolder}\monica-zmq-control.exe"
	;File /oname=monica-zmq-control-send.exe "..\${ExecutableFolder}\monica-zmq-control-send.exe"
	File /oname=monica-zmq-proxy.exe "..\${ExecutableFolder}\monica-zmq-proxy.exe"
	File /oname=monica-capnp-proxy.exe "..\${ExecutableFolder}\monica-capnp-proxy.exe"
	File /oname=monica-capnp-server.exe "..\${ExecutableFolder}\monica-capnp-server.exe"
 
	;File /oname=monica_python.pyd "..\${ExecutableFolder}\monica_python.pyd"  
	;File "${SysLibsDir}\binaries\windows\vc141\${Arch}\boost-python\boost_python-vc141-mt-${BoostBit}-1_66.dll"
	
	File "..\LICENSE"
	File "..\documentation\de_benutzerhandbuch_MONICA_windows.pdf"
	File "..\documentation\en_user_manual_MONICA_windows.pdf"
	File "..\src\python\monica_io3.py"
	File "..\src\python\monica_io.py"
	File "..\src\python\ascii_io.py"
	File "${UtilDir}\soil\soil_io.py"
	File "${UtilDir}\soil\soil_io3.py"

	CreateDirectory "$PROFILE\MONICA"
	SetOutPath $PROFILE\MONICA
	File /oname=db-connections.ini "..\db-connections-install.ini"
	File "..\LICENSE"
		
	;CreateDirectory "$PROFILE\MONICA\sqlite-db"
	;SetOutPath "$PROFILE\MONICA\sqlite-db"
	;File "..\sqlite-db\monica.sqlite"
	;File "..\sqlite-db\ka5-soil-data.sqlite"

	;create conversion script dir
	CreateDirectory "$PROFILE\MONICA\monica-ini-to-json"
	SetOutPath "$PROFILE\MONICA\monica-ini-to-json"
	File "..\src\python\monica-ini-to-json\monica-ini-to-json.py"
	File "..\src\python\monica-ini-to-json\conversion-template-sim.json"
	File "..\src\python\monica-ini-to-json\conversion-template-site.json"
	File "..\src\python\monica-ini-to-json\conversion-template-crop.json"
	
	;create parameter directories
	CreateDirectory "$PROFILE\MONICA\monica-parameters"
	;the json version files with name name convention
	SetOutPath "$PROFILE\MONICA\monica-parameters"
	File /r "${ParamsRepoDir}\crop-residues"
	File /r "${ParamsRepoDir}\crops"
	File /r "${ParamsRepoDir}\mineral-fertilisers"
	File /r "${ParamsRepoDir}\organic-fertilisers"
	File /r "${ParamsRepoDir}\general"
	File /r "${ParamsRepoDir}\soil"
			
	;the example directory
	CreateDirectory "$PROFILE\MONICA\Examples\Hohenfinow2"
	;the json version files with name name convention
	SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2
	File "Hohenfinow2\crop-min.json"
	File "Hohenfinow2\crop-min-ic.json"
	File "Hohenfinow2\site-min.json"
	File "Hohenfinow2\sim-min.json"
	File "Hohenfinow2\sim-min-ic.json"
	File "Hohenfinow2\climate-min.csv"
	File "Hohenfinow2\crop.json"
	File "Hohenfinow2\site.json"
	File "Hohenfinow2\sim.json"
	File "Hohenfinow2\crop+.json"
	File "Hohenfinow2\site+.json"
	File "Hohenfinow2\sim+.json"
	File "Hohenfinow2\climate.csv"
	
	CreateDirectory "$PROFILE\MONICA\Examples\Hohenfinow2\python"
	;example files for the python version
	SetOutPath $PROFILE\MONICA\Examples\Hohenfinow2\python
	File "Hohenfinow2\python\carbiocial.sqlite"
	File "Hohenfinow2\python\db-connections.ini"
	File "Hohenfinow2\python\run-monica.py"
	File "Hohenfinow2\python\run-consumer.py"
	File "Hohenfinow2\python\run-producer.py"
	File "Hohenfinow2\python\site-soil-profile-from-db.json"
	
	SetOutPath "$INSTDIR"
	
	;Store installation folder
	WriteRegStr "HKCU" "Software\MONICA" "" $INSTDIR
			
	${EnvVarUpdate} $0 "PATH" "P" "HKCU" "$INSTDIR"
	${EnvVarUpdate} $0 "PYTHONPATH" "P" "HKCU" "$INSTDIR"
	${EnvVarUpdate} $0 "MONICA_HOME" "A" "HKCU" "$PROFILE\MONICA"
	${EnvVarUpdate} $0 "MONICA_PARAMETERS" "A" "HKCU" "$PROFILE\MONICA\monica-parameters"
	
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

;   Remove the installation directory
    RMDir /r "$INSTDIR"
	RMDir /r $PROFILE\MONICA	
	
	!insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
		
	Delete $SMPROGRAMS\$StartMenuFolder\MONICA.lnk
	Delete $SMPROGRAMS\$StartMenuFolder\Uninstall.lnk
	Delete $SMPROGRAMS\$StartMenuFolder\Readme.lnk
	Delete "$SMPROGRAMS\$StartMenuFolder\Documentation MONICA for Windows.lnk"
	Delete "$SMPROGRAMS\$StartMenuFolder\Dokumentation MONICA for Windows.lnk"
	Delete $DESKTOP\MONICA.lnk
	RMDir $SMPROGRAMS\$StartMenuFolder
	
	DeleteRegKey /ifempty HKCU "Software\MONICA"

	${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" "$INSTDIR"
	${un.EnvVarUpdate} $0 "PYTHONPATH" "R" "HKCU" "$INSTDIR"
	${un.EnvVarUpdate} $0 "MONICA_HOME" "R" "HKCU" "$PROFILE\MONICA"
	${un.EnvVarUpdate} $0 "MONICA_PARAMETERS" "R" "HKCU" "$PROFILE\MONICA"
	
SectionEnd
