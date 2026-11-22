#ifndef PayloadRoot
  #error PayloadRoot must contain the verified QQEasyTool.vst3 bundle.
#endif
[Setup]
AppId=QingAudio.QQEasyTool
AppName=QQEasyTool
AppPublisher=Qing Audio
AppPublisherURL=https://github.com/Ziqing-Gu/QQEasyTool
AppVersion=1.09
VersionInfoVersion=1.0.9.0
VersionInfoProductVersion=1.09
VersionInfoDescription=QQEasyTool 1.09 Windows x64 Installer
DefaultDirName={code:DefaultVST3}
DisableDirPage=yes
UsePreviousAppDir=no
DisableProgramGroupPage=yes
DisableWelcomePage=no
WizardStyle=modern
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
Uninstallable=yes
CreateUninstallRegKey=StandardDestination
UninstallFilesDir={code:UninstallerDirectory}
UninstallDisplayName=QQEasyTool 1.09
OutputDir={#OutputRoot}
OutputBaseFilename=QQEasyTool-1.09-Windows-x64-Setup
LicenseFile={#LicensePath}
Compression=lzma2
SolidCompression=yes
CloseApplications=no
RestartApplications=no
SetupMutex=QingAudio-QQEasyTool-Install
SetupLogging=yes
DirExistsWarning=no

[Messages]
WelcomeLabel1=QQEasyTool 1.09%n欢迎安装 / Welcome
WelcomeLabel2=本安装程序会将 QQEasyTool 安装到标准 VST3 目录。请先保存工程并完全退出音频宿主。%n%nThis installs QQEasyTool into the standard VST3 directory. Save your session and fully quit audio hosts first.
ButtonBack=< 上一步 / Back
ButtonNext=下一步 / Next >
ButtonInstall=安装 / Install
ButtonCancel=取消 / Cancel
ButtonFinish=完成 / Finish
FinishedLabelNoIcons=安装完成。请重新打开宿主并扫描 VST3。ARA 通过宿主的事件扩展入口使用。%n%nInstallation complete. Reopen your host and rescan VST3. Use the host's event extension command for ARA.

[Files]
Source: "{#PayloadRoot}\QQEasyTool.vst3\Contents\x86_64-win\QQEasyTool.vst3"; DestDir: "{app}\QQEasyTool.vst3\Contents\x86_64-win"; Flags: ignoreversion
Source: "{#PayloadRoot}\QQEasyTool.vst3\Contents\Resources\moduleinfo.json"; DestDir: "{app}\QQEasyTool.vst3\Contents\Resources"; Flags: ignoreversion

[Code]
function Attributes(Path: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';

function DefaultVST3(Param: String): String;
begin
  if IsAdminInstallMode then Result := ExpandConstant('{commoncf64}\VST3')
  else Result := ExpandConstant('{localappdata}\Programs\Common\VST3');
end;

function StandardDestination: Boolean;
begin
  Result := CompareText(RemoveBackslashUnlessRoot(ExpandConstant('{app}')), DefaultVST3('')) = 0;
end;

function UninstallerDirectory(Param: String): String;
begin
  if StandardDestination then Result := ExpandConstant('{autopf}\Qing Audio\QQEasyTool\Uninstall')
  else Result := ExpandConstant('{app}\.QQEasyTool-Uninstall');
end;

function SafeParents(Path: String): Boolean;
var A: LongWord; Parent: String;
begin
  Result := False;
  repeat
    A := Attributes(Path);
    if (A <> $FFFFFFFF) and ((A and $400) <> 0) then Exit;
    Parent := ExtractFileDir(Path);
    if (Parent = Path) or (Parent = '') then Break;
    Path := Parent;
  until False;
  Result := True;
end;

function CheckTarget: String;
var Locator, Service, Hosts: Variant; I: Integer; Path: String; Stream: TFileStream;
begin
  Result := '';
  if (IsAdminInstallMode and not StandardDestination) or not SafeParents(ExpandConstant('{app}')) then begin
    Result := '安装目录不安全或不是标准目录。 / Use the standard directory without symbolic links.'; Exit;
  end;
  if StandardDestination then begin
    try
      Locator := CreateOleObject('WbemScripting.SWbemLocator');
      Service := Locator.ConnectServer('', 'root\CIMV2');
      Hosts := Service.ExecQuery('SELECT Name FROM Win32_Process WHERE Name LIKE ''Cubase%'' OR Name LIKE ''Nuendo%'' OR Name = ''Studio One.exe'' OR Name LIKE ''Fender Studio%'' OR Name = ''reaper.exe'' OR Name LIKE ''Ableton Live%'' OR Name = ''FL64.exe'' OR Name LIKE ''Bitwig%'' OR Name LIKE ''WaveLab%'' OR Name LIKE ''QQ%Host%'' OR Name LIKE ''ACE%Studio%'' OR Name LIKE ''%PluginScanner%''');
      if Hosts.Count > 0 then begin Result := '请完全退出音频宿主后重试。 / Fully quit audio hosts and retry.'; Exit; end;
    except
      Result := '无法检查宿主状态，请关闭宿主后重试。 / Cannot check host state. Close hosts and retry.'; Exit;
    end;
  end;
  for I := 0 to 1 do begin
    if I = 0 then Path := ExpandConstant('{app}\QQEasyTool.vst3\Contents\x86_64-win\QQEasyTool.vst3')
    else Path := ExpandConstant('{app}\QQEasyTool.vst3\Contents\Resources\moduleinfo.json');
    if not SafeParents(Path) then begin Result := '插件路径包含符号链接。 / The plug-in path contains a symbolic link.'; Exit; end;
    if FileExists(Path) then begin
      try
        Stream := TFileStream.Create(Path, fmOpenReadWrite or fmShareExclusive);
        Stream.Free;
      except
        Result := '插件正在使用或无法写入，请关闭宿主后重试。 / The plug-in is in use or not writable. Close hosts and retry.'; Exit;
      end;
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var InstalledMS, InstalledLS, SetupMS, SetupLS: Cardinal;
begin
  Result := CheckTarget;
  if Result <> '' then Exit;
  if GetVersionNumbers(ExpandConstant('{app}\QQEasyTool.vst3\Contents\x86_64-win\QQEasyTool.vst3'), InstalledMS, InstalledLS) then begin
    if not GetVersionNumbers(ExpandConstant('{srcexe}'), SetupMS, SetupLS) then begin Result := 'Cannot verify installer version.'; Exit; end;
    if (InstalledMS > SetupMS) or ((InstalledMS = SetupMS) and (InstalledLS > SetupLS)) then
      Result := '已安装更高版本，不能降级。 / A newer version is already installed.';
  end;
end;

procedure CurStepChanged(Step: TSetupStep);
begin
  if Step = ssPostInstall then begin
    if CompareText(GetSHA256OfFile(ExpandConstant('{app}\QQEasyTool.vst3\Contents\x86_64-win\QQEasyTool.vst3')), '{#BinarySHA}') <> 0 then
      RaiseException('Installed plug-in verification failed.');
    if CompareText(GetSHA256OfFile(ExpandConstant('{app}\QQEasyTool.vst3\Contents\Resources\moduleinfo.json')), '{#ModuleSHA}') <> 0 then
      RaiseException('Installed metadata verification failed.');
  end;
end;

function InitializeUninstall: Boolean;
var Error: String;
begin
  Error := CheckTarget;
  Result := Error = '';
  if not Result then MsgBox(Error, mbError, MB_OK);
end;
