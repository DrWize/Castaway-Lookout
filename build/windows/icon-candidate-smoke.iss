#ifndef CandidateIcon
  #error CandidateIcon must name the ICO file to validate
#endif

#ifndef CandidateName
  #define CandidateName "candidate"
#endif

[Setup]
AppName=Johnny Castaway Icon Smoke Test
AppVersion=1.0
DefaultDirName={tmp}\JohnnyCastawayIconSmoke
PrivilegesRequired=lowest
Uninstallable=no
CreateUninstallRegKey=no
OutputDir=..\..\assets\icons\candidates\.render-temp\inno
OutputBaseFilename={#CandidateName}
SetupIconFile={#CandidateIcon}
