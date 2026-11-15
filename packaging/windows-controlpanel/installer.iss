; SPDX-License-Identifier: GPL-2.0-or-later
; Build with ISCC /DPackageRoot=... /DOutputRoot=... installer.iss
#ifndef PackageRoot
  #error PackageRoot is required
#endif
#ifndef OutputRoot
  #error OutputRoot is required
#endif
[Setup]
AppId={{F693D92D-5F77-45BB-AB7F-2E77564305EE}
AppName=KDE Control Panel (personal fork)
AppVersion=0.1.0-alpha.4
AppPublisher=xingxing7290 (unofficial personal fork)
AppPublisherURL=https://github.com/xingxing7290/kdeconnect-kde
DefaultDirName=D:\Apps\KDE-ControlPanel
DefaultGroupName=KDE Control Panel (personal fork)
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputRoot}
OutputBaseFilename=kdeconnect-controlpanel-0.1.0-alpha.4-windows-x64-setup
LicenseFile={#PackageRoot}\licenses\kdeconnect\GPL-3.0-only.txt
InfoBeforeFile={#PackageRoot}\THIRD-PARTY-NOTICES.txt
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\kdeconnect-app.exe
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes
[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "profile\*,*.pdb"
[Icons]
Name: "{group}\KDE Control Panel"; Filename: "{app}\Start-ControlPanel.cmd"; WorkingDir: "{app}"; IconFilename: "{app}\bin\kdeconnect-app.exe"
Name: "{group}\Uninstall KDE Control Panel"; Filename: "{uninstallexe}"
Name: "{group}\Third-party licenses and source"; Filename: "{app}\THIRD-PARTY-NOTICES.txt"
[Run]
Filename: "{app}\Start-ControlPanel.cmd"; Description: "Start KDE Control Panel"; Flags: postinstall shellexec skipifsilent unchecked
; No UninstallDelete: user identities, pairings and settings must survive uninstall.
