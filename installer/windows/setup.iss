#ifndef BundleDir
  #error BundleDir must name the prepared installer bundle
#endif
#ifndef InstallerOutput
  #error InstallerOutput must name the output directory
#endif
#ifndef PackageVersion
  #define PackageVersion "0.1.0"
#endif
#ifndef NumericVersion
  #define NumericVersion "0.1.0.0"
#endif

[Setup]
AppId={code:InstallationId}
AppName=NextClient
AppVersion={#PackageVersion}
VersionInfoVersion={#NumericVersion}
AppPublisher=NextClient
AppPublisherURL=https://github.com/CS-NextClient/NextClient
DefaultDirName=C:\Games\Counter Strike 1.6 NextClient
AppendDefaultDirName=no
DirExistsWarning=no
UsePreviousAppDir=no
UsePreviousLanguage=no
ShowLanguageDialog=yes
LanguageDetectionMethod=uilanguage
DisableProgramGroupPage=yes
DisableReadyPage=yes
PrivilegesRequired=lowest
OutputDir={#InstallerOutput}
OutputBaseFilename=NextClient-{#PackageVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
AllowCancelDuringInstall=yes
UninstallFilesDir={app}
UninstallDisplayName=NextClient ({app})

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl,languages\english.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl,languages\russian.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl,languages\ukrainian.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl,languages\italian.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl,languages\spanish.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl,languages\turkish.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl,languages\portuguese.isl"
Name: "brazilian"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl,languages\brazilian.isl"
Name: "schinese"; MessagesFile: "translations\ChineseSimplified.isl,languages\schinese.isl"

[Files]
Source: "{#BundleDir}\*"; DestDir: "{tmp}\payload"; Flags: dontcopy recursesubdirs createallsubdirs
Source: "{#BundleDir}\tool\nextclient-install.exe"; DestDir: "{app}\.nextclient\installer"; Flags: ignoreversion
Source: "{#BundleDir}\licenses\*"; DestDir: "{app}\.nextclient\installer\licenses"; Flags: ignoreversion

[Icons]
Name: "{userprograms}\NextClient\{code:ShortcutName}"; Filename: "{app}\cstrike.exe"; WorkingDir: "{app}"

[UninstallDelete]
Type: files; Name: "{userdesktop}\{code:ShortcutName}.lnk"
Type: files; Name: "{app}\.nextclient\installer\unins000.exe"
Type: files; Name: "{app}\.nextclient\installer\unins000.dat"
Type: dirifempty; Name: "{app}\.nextclient\installer\licenses"
Type: dirifempty; Name: "{app}\.nextclient\installer"
Type: dirifempty; Name: "{app}\.nextclient"
Type: dirifempty; Name: "{app}"

[Code]
const
  kWindowStyle = -16;
  kButtonMultiline = $2000;

var
  g_ModePage: TInputOptionWizardPage;
  g_SourcePage: TInputDirWizardPage;
  g_PreparationPage: TOutputMarqueeProgressWizardPage;
  g_ImportPage: TOutputProgressWizardPage;
  g_PayloadReady: Boolean;
  g_DiscoveryDone: Boolean;
  g_OperationId: String;
  g_OperationDestination: String;
  g_SetupDone: Boolean;
  g_Removing: Boolean;
  g_CompletionFailed: Boolean;
  g_RemovalMode: String;
  g_RequiredSpaceLabel: TNewStaticText;
  g_NewRequiredBytes: Int64;
  g_CopyCancelButton: TNewButton;
  g_DesktopShortcutCheck: TNewCheckBox;
  g_Copying: Boolean;
  g_CancelRequested: Boolean;
  g_CancellationFinished: Boolean;
  g_CancelFile: String;

function GetCurrentProcessId: LongWord;
  external 'GetCurrentProcessId@kernel32.dll stdcall';

function EnableWindow(window: HWND; enable: Integer): Integer;
  external 'EnableWindow@user32.dll stdcall';

function GetWindowLong(window: HWND; index: Integer): Longint;
  external 'GetWindowLongW@user32.dll stdcall';

function SetWindowLong(window: HWND; index: Integer; value: Longint): Longint;
  external 'SetWindowLongW@user32.dll stdcall';

function Quoted(value: String): String;
var
  position, backslash_count: Integer;
  character: Char;
begin
  Result := '"';
  backslash_count := 0;
  for position := 1 to Length(value) do begin
    character := value[position];
    if character = '\' then
      backslash_count := backslash_count + 1
    else begin
      if character = '"' then
        Result := Result + StringOfChar('\', backslash_count * 2 + 1) + character
      else
        Result := Result + StringOfChar('\', backslash_count) + character;
      backslash_count := 0;
    end;
  end;
  Result := Result + StringOfChar('\', backslash_count * 2) + '"';
end;

function PathId(value: String): String;
begin
  Result := GetSHA256OfString(Lowercase(RemoveBackslash(ExpandFileName(value))));
end;

function InstallationId(param: String): String;
begin
  if WizardForm = nil then
    Result := ''
  else
    Result := 'NextClient-' + PathId(WizardDirValue);
end;

function ShortcutName(param: String): String;
begin
  Result := 'Counter Strike 1.6 NextClient (' + Copy(PathId(ExpandConstant('{app}')), 1, 12) + ')';
end;

function ExistingParent(directory: String): String;
var
  parent: String;
begin
  while not DirExists(directory) do begin
    parent := ExtractFileDir(RemoveBackslash(directory));
    if (parent = '') or (parent = directory) then
      Break;
    directory := parent;
  end;
  Result := directory;
end;

procedure BrowseDestination(sender: TObject);
var
  directory: String;
begin
  directory := ExistingParent(WizardDirValue);
  if BrowseForFolder(CustomMessage('SelectDestination'), directory, True) then begin
    if g_ModePage.SelectedValueIndex = 0 then
      directory := AddBackslash(directory) + 'Counter Strike 1.6 NextClient';
    WizardForm.DirEdit.Text := directory;
  end;
end;

procedure BrowseSource(sender: TObject);
var
  directory: String;
begin
  directory := ExistingParent(g_SourcePage.Values[0]);
  if BrowseForFolder(CustomMessage('SelectSteamFolder'), directory, True) then
    g_SourcePage.Values[0] := directory;
end;

function DiskSpaceText(bytes: Int64): String;
begin
  if bytes > 1073741824 then
    Result := FmtMessage(CustomMessage('SpaceGigabytes'), [Format('%.2f', [bytes / 1073741824.0])])
  else
    Result := FmtMessage(CustomMessage('SpaceMegabytes'), [IntToStr((bytes + 1048575) div 1048576)]);
end;

procedure RequestCancellation;
begin
  if g_CancelRequested then
    Exit;
  if MsgBox(CustomMessage('CancelInstallation'),
      mbConfirmation, MB_YESNO) <> IDYES then
    Exit;
  if not SaveStringToFile(g_CancelFile, 'cancel', False) then begin
    MsgBox(CustomMessage('CancelRequestFailed'), mbError, MB_OK);
    Exit;
  end;
  g_CancelRequested := True;
  g_CopyCancelButton.Enabled := False;
  g_ImportPage.SetText(CustomMessage('CancelingInstallation'), CustomMessage('RestoringFiles'));
end;

procedure CopyCancelClick(sender: TObject);
begin
  RequestCancellation;
end;

procedure CancelButtonClick(cur_page_id: Integer; var cancel, confirm: Boolean);
begin
  if g_Copying then begin
    cancel := False;
    confirm := False;
    RequestCancellation;
  end else if g_CancellationFinished then
    confirm := False;
end;

function InitializeSetup: Boolean;
var
  mode: String;
begin
  mode := Lowercase(ExpandConstant('{param:MODE|new}'));
  Result := (mode = 'new') or (mode = 'update');
  if not Result then
    MsgBox(CustomMessage('UnknownInstallationMode'), mbError, MB_OK);
end;

procedure PreparePayload;
begin
  if not g_PayloadReady then begin
    ExtractTemporaryFiles('{tmp}\payload\*');
    g_PayloadReady := True;
  end;
end;

function ProgressText(value: String): String;
var
  fields: TArrayOfString;
  arguments: array[0..1] of String;
  index: Integer;
begin
  fields := StringSplit(value, [';'], stAll);
  for index := 1 to GetArrayLength(fields) - 1 do
    arguments[index - 1] := fields[index];
  // The native FmtMessage binding requires an array constructor; progress has at most two arguments.
  Result := FmtMessage(CustomMessage(fields[0]), [arguments[0], arguments[1]]);
end;

procedure HelperProgress(const line: String; const error, first_line: Boolean);
var
  fields: TArrayOfString;
  percent: Integer;
begin
  Log(line);
  if g_Copying then begin
    // ExecAndLogOutput disables the wizard while waiting for its child process.
    EnableWindow(WizardForm.Handle, 1);
  end;
  if error or (Pos('NEXTCLIENT_PROGRESS|', line) <> 1) then
    Exit;
  fields := StringSplit(line, ['|'], stAll);
  if GetArrayLength(fields) <> 4 then
    Exit;
  percent := StrToIntDef(fields[1], 0);
  if g_Removing then begin
    UninstallProgressForm.StatusLabel.Caption := ProgressText(fields[2]);
    UninstallProgressForm.ProgressBar.Position := percent;
  end else begin
    if g_CancelRequested then
      Exit;
    g_ImportPage.SetText(ProgressText(fields[2]), ProgressText(fields[3]));
    if (fields[3] = 'ProgressWait') and (percent < 100) then
      g_ImportPage.ProgressBar.Style := npbstMarquee
    else
      g_ImportPage.ProgressBar.Style := npbstNormal;
    g_ImportPage.SetProgress(percent, 100);
  end;
end;

function Helper(action, arguments: String; installed: Boolean): String;
var
  executable, status_file, state: String;
  exit_code: Integer;
begin
  status_file := ExpandConstant('{tmp}\nextclient-helper-status.ini');
  DeleteFile(status_file);
  if installed then
    executable := ExpandConstant('{app}\.nextclient\installer\nextclient-install.exe')
  else begin
    PreparePayload;
    executable := ExpandConstant('{tmp}\payload\tool\nextclient-install.exe');
  end;
  if not ExecAndLogOutput(executable, action + ' ' + arguments + ' --language ' + ActiveLanguage +
      ' --progress --status-file ' + Quoted(status_file),
      '', SW_SHOWNORMAL, ewWaitUntilTerminated, exit_code, @HelperProgress) then
    Result := CustomMessage('HelperStartFailed')
  else if exit_code <> 0 then
    Result := GetIniString('result', 'error', CustomMessage('HelperFailed'), status_file)
  else begin
    state := GetIniString('result', 'state', '', status_file);
    if (state = 'passed') or (state = 'pending') then
      Result := ''
    else
      Result := CustomMessage('HelperResultMissing');
  end;
end;

function RollbackCanceledOperation: String;
begin
  Result := Helper('rollback', '--destination ' + Quoted(g_OperationDestination) +
    ' --operation-id ' + Quoted(g_OperationId), False);
  if Result = '' then begin
    g_OperationId := '';
    g_CancellationFinished := True;
    Result := CustomMessage('InstallationCanceled');
  end else
    Result := FmtMessage(CustomMessage('CancellationRecoveryFailed'), [Result]);
end;

procedure DiscoverSource;
var
  arguments, failure: String;
begin
  if g_DiscoveryDone or (g_SourcePage.Values[0] <> '') then
    Exit;
  g_DiscoveryDone := True;
  g_PreparationPage.SetText(CustomMessage('FindingSteam'), CustomMessage('ProgressWait'));
  g_PreparationPage.Show;
  g_PreparationPage.Animate;
  try
    arguments := '';
    if ExpandConstant('{param:STEAMROOT|}') <> '' then
      arguments := '--steam-root ' + Quoted(ExpandConstant('{param:STEAMROOT|}'));
    failure := Helper('discover', arguments, False);
    if failure = '' then
      g_SourcePage.Values[0] := GetIniString('result', 'source', '', ExpandConstant('{tmp}\nextclient-helper-status.ini'));
  finally
    g_PreparationPage.Hide;
  end;
end;

function InspectSource: String;
begin
  g_NewRequiredBytes := 0;
  DiscoverSource;
  if g_SourcePage.Values[0] = '' then begin
    Result := CustomMessage('SelectSteamInstallation');
    Exit;
  end;
  g_PreparationPage.SetText(CustomMessage('CheckingGameSpace'), CustomMessage('ProgressWait'));
  g_PreparationPage.Show;
  g_PreparationPage.Animate;
  try
    Result := Helper('inspect', '--bundle ' + Quoted(ExpandConstant('{tmp}\payload')) +
      ' --source ' + Quoted(g_SourcePage.Values[0]), False);
    if Result = '' then begin
      g_NewRequiredBytes := StrToInt64Def(GetIniString('result', 'required_bytes', '',
        ExpandConstant('{tmp}\nextclient-helper-status.ini')), 0);
      if g_NewRequiredBytes <= 0 then
        Result := CustomMessage('RequiredSpaceUnknown');
    end;
  finally
    g_PreparationPage.Hide;
  end;
end;

procedure UpdateSpaceLabel;
begin
  if g_ModePage.SelectedValueIndex = 0 then
    g_RequiredSpaceLabel.Caption := FmtMessage(CustomMessage('RequiredSpace'), [DiskSpaceText(g_NewRequiredBytes)])
  else
    g_RequiredSpaceLabel.Caption := CustomMessage('UpdateSpace');
end;

function NextButtonClick(cur_page_id: Integer): Boolean;
var
  failure, directory, parent: String;
  free_bytes, total_bytes: Int64;
begin
  Result := True;
  if (cur_page_id = g_SourcePage.ID) and (g_ModePage.SelectedValueIndex = 0) then begin
    failure := InspectSource;
    if failure <> '' then begin
      MsgBox(failure, mbError, MB_OK);
      Result := False;
    end;
  end;
  if (cur_page_id = wpSelectDir) and (g_ModePage.SelectedValueIndex = 0) then begin
    if g_NewRequiredBytes <= 0 then begin
      failure := InspectSource;
      if failure <> '' then begin
        MsgBox(failure, mbError, MB_OK);
        Result := False;
        Exit;
      end;
    end;
    directory := ExpandFileName(WizardDirValue);
    while not DirExists(directory) do begin
      parent := ExtractFileDir(directory);
      if parent = directory then
        Break;
      directory := parent;
    end;
    if not GetSpaceOnDisk64(directory, free_bytes, total_bytes) then
      failure := FmtMessage(CustomMessage('FreeSpaceUnknown'), [directory])
    else if free_bytes < g_NewRequiredBytes then
      failure := FmtMessage(CustomMessage('NotEnoughSpace'), [DiskSpaceText(g_NewRequiredBytes)]);
    if failure <> '' then begin
      MsgBox(failure, mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure InitializeWizard;
begin
  g_ModePage := CreateInputOptionPage(wpWelcome, CustomMessage('InstallationMode'), CustomMessage('ChooseMode'),
    CustomMessage('UpdatePreservesFiles'), True, False);
  g_ModePage.Add(CustomMessage('NewInstallation'));
  g_ModePage.Add(CustomMessage('UpdateInstallation'));
  g_ModePage.SelectedValueIndex := 0;
  if Lowercase(ExpandConstant('{param:MODE|new}')) = 'update' then
    g_ModePage.SelectedValueIndex := 1;
  g_SourcePage := CreateInputDirPage(g_ModePage.ID, CustomMessage('SteamFolder'), CustomMessage('ChooseSteamFolder'),
    CustomMessage('SteamFolderDescription'), False, '');
  g_SourcePage.Add(CustomMessage('HalfLifeFolder'));
  g_SourcePage.Buttons[0].OnClick := @BrowseSource;
  g_SourcePage.Values[0] := ExpandConstant('{param:GAMEDIR|}');
  g_PreparationPage := CreateOutputMarqueeProgressPage(CustomMessage('PreparingSetup'), CustomMessage('FindingFiles'));
  g_ImportPage := CreateOutputProgressPage(CustomMessage('InstallingNextClient'), CustomMessage('PreparingInstallation'));
  WizardForm.DirBrowseButton.OnClick := @BrowseDestination;
  g_CopyCancelButton := TNewButton.Create(WizardForm);
  g_CopyCancelButton.Parent := WizardForm;
  g_CopyCancelButton.SetBounds(WizardForm.CancelButton.Left, WizardForm.CancelButton.Top,
    WizardForm.CancelButton.Width, WizardForm.CancelButton.Height);
  g_CopyCancelButton.Caption := SetupMessage(msgButtonCancel);
  g_CopyCancelButton.Cancel := True;
  g_CopyCancelButton.OnClick := @CopyCancelClick;
  g_CopyCancelButton.Visible := False;
  g_DesktopShortcutCheck := TNewCheckBox.Create(WizardForm);
  g_DesktopShortcutCheck.Parent := WizardForm.FinishedPage;
  g_DesktopShortcutCheck.SetBounds(WizardForm.RunList.Left, WizardForm.RunList.Top,
    WizardForm.RunList.Width, ScaleY(24));
  g_DesktopShortcutCheck.Caption := CustomMessage('DesktopShortcut');
  g_DesktopShortcutCheck.Checked := not WizardSilent;
  g_DesktopShortcutCheck.Visible := False;
  g_RequiredSpaceLabel := TNewStaticText.Create(WizardForm);
  g_RequiredSpaceLabel.Parent := WizardForm.DiskSpaceLabel.Parent;
  g_RequiredSpaceLabel.AutoSize := False;
  g_RequiredSpaceLabel.WordWrap := True;
  g_RequiredSpaceLabel.SetBounds(WizardForm.DiskSpaceLabel.Left, WizardForm.DiskSpaceLabel.Top,
    WizardForm.DiskSpaceLabel.Width, ScaleY(34));
  WizardForm.DiskSpaceLabel.Visible := False;
end;

function ShouldSkipPage(page_id: Integer): Boolean;
begin
  Result := (page_id = g_SourcePage.ID) and (g_ModePage.SelectedValueIndex = 1);
end;

procedure CurPageChanged(cur_page_id: Integer);
begin
  if cur_page_id = g_SourcePage.ID then
    DiscoverSource;
  if cur_page_id = wpSelectDir then begin
    WizardForm.DiskSpaceLabel.Visible := False;
    UpdateSpaceLabel;
  end;
  if cur_page_id = wpInstalling then
    WizardForm.FilenameLabel.Visible := False;
  if cur_page_id = wpFinished then begin
    g_DesktopShortcutCheck.Top := WizardForm.RunList.Top;
    g_DesktopShortcutCheck.Visible := g_ModePage.SelectedValueIndex = 0;
  end;
end;

function PrepareToInstall(var needs_restart: Boolean): String;
var
  arguments, action: String;
begin
  Result := '';
  if g_OperationId <> '' then begin
    if g_CancelRequested then begin
      Result := RollbackCanceledOperation;
      if g_CancellationFinished then
        WizardForm.Close;
    end;
    Exit;
  end;
  PreparePayload;
  arguments := '--bundle ' + Quoted(ExpandConstant('{tmp}\payload')) +
    ' --destination ' + Quoted(WizardDirValue) + ' --defer-commit --owner-pid ' + IntToStr(GetCurrentProcessId);
  g_CancelFile := ExpandConstant('{tmp}\nextclient-cancel');
  DeleteFile(g_CancelFile);
  g_CancelRequested := False;
  arguments := arguments + ' --cancel-file ' + Quoted(g_CancelFile);
  if g_ModePage.SelectedValueIndex = 0 then begin
    DiscoverSource;
    if g_SourcePage.Values[0] = '' then begin
      Result := CustomMessage('SelectSteamInstallation');
      Exit;
    end;
    action := 'install';
    arguments := arguments + ' --source ' + Quoted(g_SourcePage.Values[0]);
  end else
    action := 'update';
  g_ImportPage.SetText(CustomMessage('PreparingFiles'), CustomMessage('ProgressWait'));
  g_ImportPage.ProgressBar.Style := npbstMarquee;
  g_ImportPage.Show;
  g_Copying := True;
  g_CopyCancelButton.Enabled := True;
  g_CopyCancelButton.Visible := True;
  g_CopyCancelButton.BringToFront;
  try
    Result := Helper(action, arguments, False);
    if Result = '' then begin
      g_OperationId := GetIniString('result', 'operation_id', '', ExpandConstant('{tmp}\nextclient-helper-status.ini'));
      g_OperationDestination := WizardDirValue;
      if g_OperationId = '' then
        Result := CustomMessage('OperationIdMissing');
    end;
    if g_CancelRequested then begin
      if (Result = '') and (g_OperationId <> '') then begin
        Result := RollbackCanceledOperation;
      end else if GetIniString('result', 'state', '',
          ExpandConstant('{tmp}\nextclient-helper-status.ini')) = 'canceled' then
        g_CancellationFinished := True;
    end;
  finally
    g_Copying := False;
    g_CopyCancelButton.Visible := False;
    g_ImportPage.Hide;
    DeleteFile(g_CancelFile);
  end;
  if g_CancellationFinished then
    WizardForm.Close;
end;

procedure CurStepChanged(cur_step: TSetupStep);
var
  failure: String;
begin
  if cur_step = ssDone then begin
    failure := Helper('commit', '--destination ' + Quoted(g_OperationDestination) +
      ' --operation-id ' + Quoted(g_OperationId), False);
    g_SetupDone := failure = '';
    g_CompletionFailed := not g_SetupDone;
    if g_CompletionFailed then
      MsgBox(FmtMessage(CustomMessage('FinalizeFailed'), [failure]), mbError, MB_OK);
    if g_SetupDone then begin
      DeleteFile(ExpandConstant('{app}\.nextclient\installer\unins000.exe'));
      DeleteFile(ExpandConstant('{app}\.nextclient\installer\unins000.dat'));
    end;
    if g_SetupDone and (g_ModePage.SelectedValueIndex = 0) and g_DesktopShortcutCheck.Checked then begin
      try
        CreateShellLink(ExpandConstant('{userdesktop}\') + ShortcutName('') + '.lnk', 'NextClient',
          ExpandConstant('{app}\cstrike.exe'), '', ExpandConstant('{app}'), '', 0, SW_SHOWNORMAL);
      except
        MsgBox(FmtMessage(CustomMessage('ShortcutFailed'), [GetExceptionMessage]), mbError, MB_OK);
      end;
    end;
  end;
end;

function GetCustomSetupExitCode: Integer;
begin
  if g_CompletionFailed then
    Result := 1
  else
    Result := 0;
end;

procedure DeinitializeSetup;
var
  failure: String;
begin
  if (g_OperationId <> '') and not g_SetupDone then begin
    failure := Helper('rollback', '--destination ' + Quoted(g_OperationDestination) +
      ' --operation-id ' + Quoted(g_OperationId), False);
    if failure <> '' then
      Log('Recovery data retained: ' + failure);
  end;
end;

function InitializeUninstall: Boolean;
var
  form: TSetupForm;
  preserve_button, full_button: TNewRadioButton;
  accept_button, cancel_button: TNewButton;
begin
  g_RemovalMode := Lowercase(ExpandConstant('{param:REMOVEMODE|preserve}'));
  Result := (g_RemovalMode = 'preserve') or (g_RemovalMode = 'full');
  if not Result then begin
    MsgBox(CustomMessage('UnknownRemovalMode'), mbError, MB_OK);
    Exit;
  end;
  if UninstallSilent then
    Exit;
  form := CreateCustomForm(ScaleX(620), ScaleY(220), False, True);
  try
    form.Caption := CustomMessage('RemoveNextClient');
    form.ClientWidth := ScaleX(620);
    form.ClientHeight := ScaleY(220);
    preserve_button := TNewRadioButton.Create(form);
    preserve_button.Parent := form;
    preserve_button.SetBounds(ScaleX(16), ScaleY(20), ScaleX(588), ScaleY(48));
    preserve_button.Caption := CustomMessage('PreserveUserFiles');
    preserve_button.Checked := g_RemovalMode = 'preserve';
    SetWindowLong(preserve_button.Handle, kWindowStyle,
      GetWindowLong(preserve_button.Handle, kWindowStyle) or kButtonMultiline);
    full_button := TNewRadioButton.Create(form);
    full_button.Parent := form;
    full_button.SetBounds(ScaleX(16), ScaleY(80), ScaleX(588), ScaleY(48));
    full_button.Caption := CustomMessage('DeleteAllFiles');
    full_button.Checked := g_RemovalMode = 'full';
    SetWindowLong(full_button.Handle, kWindowStyle,
      GetWindowLong(full_button.Handle, kWindowStyle) or kButtonMultiline);
    accept_button := TNewButton.Create(form);
    accept_button.Parent := form;
    accept_button.SetBounds(ScaleX(420), ScaleY(170), ScaleX(88), ScaleY(26));
    accept_button.Caption := CustomMessage('RemoveButton');
    accept_button.ModalResult := mrOk;
    accept_button.Default := True;
    cancel_button := TNewButton.Create(form);
    cancel_button.Parent := form;
    cancel_button.SetBounds(ScaleX(516), ScaleY(170), ScaleX(88), ScaleY(26));
    cancel_button.Caption := SetupMessage(msgButtonCancel);
    cancel_button.ModalResult := mrCancel;
    cancel_button.Cancel := True;
    Result := form.ShowModal = mrOk;
    if full_button.Checked then
      g_RemovalMode := 'full'
    else
      g_RemovalMode := 'preserve';
  finally
    form.Free;
  end;
end;

procedure CurUninstallStepChanged(cur_uninstall_step: TUninstallStep);
var
  failure: String;
begin
  if cur_uninstall_step = usUninstall then begin
    g_Removing := True;
    failure := Helper('uninstall', '--destination ' + Quoted(ExpandConstant('{app}')) +
      ' --keep-installer --removal-mode ' + g_RemovalMode, True);
    if failure <> '' then
      RaiseException(FmtMessage(CustomMessage('RemovalFailed'), [failure]));
  end;
end;
