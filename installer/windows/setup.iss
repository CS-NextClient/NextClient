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
  if BrowseForFolder('Select the installation folder', directory, True) then begin
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
  if BrowseForFolder('Select the Steam Half-Life folder', directory, True) then
    g_SourcePage.Values[0] := directory;
end;

function DiskSpaceText(bytes: Int64): String;
begin
  if bytes > 1073741824 then
    Result := Format('%.2f GB', [bytes / 1073741824.0])
  else
    Result := IntToStr((bytes + 1048575) div 1048576) + ' MB';
end;

procedure RequestCancellation;
begin
  if g_CancelRequested then
    Exit;
  if MsgBox('Cancel installation? Setup will restore any files changed by this operation.',
      mbConfirmation, MB_YESNO) <> IDYES then
    Exit;
  if not SaveStringToFile(g_CancelFile, 'cancel', False) then begin
    MsgBox('Could not request cancellation. Please wait for the operation to finish.', mbError, MB_OK);
    Exit;
  end;
  g_CancelRequested := True;
  g_CopyCancelButton.Enabled := False;
  g_ImportPage.SetText('Canceling installation...', 'Restoring files. Please wait...');
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
    MsgBox('Unknown installation mode. Use /MODE=new or /MODE=update.', mbError, MB_OK);
end;

procedure PreparePayload;
begin
  if not g_PayloadReady then begin
    ExtractTemporaryFiles('{tmp}\payload\*');
    g_PayloadReady := True;
  end;
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
    UninstallProgressForm.StatusLabel.Caption := fields[2];
    UninstallProgressForm.ProgressBar.Position := percent;
  end else begin
    if g_CancelRequested then
      Exit;
    g_ImportPage.SetText(fields[2], fields[3]);
    if (fields[3] = 'Please wait...') and (percent < 100) then
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
  if not ExecAndLogOutput(executable, action + ' ' + arguments + ' --progress --status-file ' + Quoted(status_file),
      '', SW_SHOWNORMAL, ewWaitUntilTerminated, exit_code, @HelperProgress) then
    Result := 'Could not start the installation helper.'
  else if exit_code <> 0 then
    Result := GetIniString('result', 'error', 'The installation helper failed.', status_file)
  else begin
    state := GetIniString('result', 'state', '', status_file);
    if (state = 'passed') or (state = 'pending') then
      Result := ''
    else
      Result := 'The installation helper did not write a successful result.';
  end;
end;

function RollbackCanceledOperation: String;
begin
  Result := Helper('rollback', '--destination ' + Quoted(g_OperationDestination) +
    ' --operation-id ' + Quoted(g_OperationId), False);
  if Result = '' then begin
    g_OperationId := '';
    g_CancellationFinished := True;
    Result := 'Installation canceled.';
  end else
    Result := 'Could not finish cancellation. Retry recovery before continuing: ' + Result;
end;

procedure DiscoverSource;
var
  arguments, failure: String;
begin
  if g_DiscoveryDone or (g_SourcePage.Values[0] <> '') then
    Exit;
  g_DiscoveryDone := True;
  g_PreparationPage.SetText('Finding your Steam installation...', 'Please wait...');
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
    Result := 'Select a Steam Counter-Strike installation or supply /GAMEDIR=path.';
    Exit;
  end;
  g_PreparationPage.SetText('Checking game version and disk space...', 'Please wait...');
  g_PreparationPage.Show;
  g_PreparationPage.Animate;
  try
    Result := Helper('inspect', '--bundle ' + Quoted(ExpandConstant('{tmp}\payload')) +
      ' --source ' + Quoted(g_SourcePage.Values[0]), False);
    if Result = '' then begin
      g_NewRequiredBytes := StrToInt64Def(GetIniString('result', 'required_bytes', '',
        ExpandConstant('{tmp}\nextclient-helper-status.ini')), 0);
      if g_NewRequiredBytes <= 0 then
        Result := 'Could not determine the required disk space.';
    end;
  finally
    g_PreparationPage.Hide;
  end;
end;

procedure UpdateSpaceLabel;
begin
  if g_ModePage.SelectedValueIndex = 0 then
    g_RequiredSpaceLabel.Caption := 'At least ' + DiskSpaceText(g_NewRequiredBytes) +
      ' of free disk space is required, including the Steam game files.'
  else
    g_RequiredSpaceLabel.Caption := 'Space for replacement files and backups will be checked before updating.';
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
      failure := 'Could not determine free disk space in ' + directory
    else if free_bytes < g_NewRequiredBytes then
      failure := 'Insufficient free disk space. At least ' +
        DiskSpaceText(g_NewRequiredBytes) + ' is required, including the Steam game files.';
    if failure <> '' then begin
      MsgBox(failure, mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure InitializeWizard;
begin
  g_ModePage := CreateInputOptionPage(wpWelcome, 'Installation mode', 'Choose how to install NextClient.',
    'An update preserves your settings and files outside the release package.', True, False);
  g_ModePage.Add('New installation from Steam');
  g_ModePage.Add('Update an existing NextClient installation');
  g_ModePage.SelectedValueIndex := 0;
  if Lowercase(ExpandConstant('{param:MODE|new}')) = 'update' then
    g_ModePage.SelectedValueIndex := 1;
  g_SourcePage := CreateInputDirPage(g_ModePage.ID, 'Steam game folder', 'Choose your Counter-Strike installation.',
    'Select the Half-Life folder from the steam_legacy branch. Setup creates a separate copy.', False, '');
  g_SourcePage.Add('Half-Life folder:');
  g_SourcePage.Buttons[0].OnClick := @BrowseSource;
  g_SourcePage.Values[0] := ExpandConstant('{param:GAMEDIR|}');
  g_PreparationPage := CreateOutputMarqueeProgressPage('Preparing setup', 'Finding game files.');
  g_ImportPage := CreateOutputProgressPage('Installing NextClient', 'Preparing your installation.');
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
  g_DesktopShortcutCheck.Caption := 'Create a desktop shortcut';
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
      Result := 'Select a Steam Counter-Strike installation or supply /GAMEDIR=path.';
      Exit;
    end;
    action := 'install';
    arguments := arguments + ' --source ' + Quoted(g_SourcePage.Values[0]);
  end else
    action := 'update';
  g_ImportPage.SetText('Preparing game files...', 'Please wait...');
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
        Result := 'The installation helper did not return an operation identifier.';
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
      MsgBox('Could not finalize installation: ' + failure, mbError, MB_OK);
    if g_SetupDone then begin
      DeleteFile(ExpandConstant('{app}\.nextclient\installer\unins000.exe'));
      DeleteFile(ExpandConstant('{app}\.nextclient\installer\unins000.dat'));
    end;
    if g_SetupDone and (g_ModePage.SelectedValueIndex = 0) and g_DesktopShortcutCheck.Checked then begin
      try
        CreateShellLink(ExpandConstant('{userdesktop}\') + ShortcutName('') + '.lnk', 'NextClient',
          ExpandConstant('{app}\cstrike.exe'), '', ExpandConstant('{app}'), '', 0, SW_SHOWNORMAL);
      except
        MsgBox('Could not create the desktop shortcut: ' + GetExceptionMessage, mbError, MB_OK);
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
    MsgBox('Unknown removal mode. Use /REMOVEMODE=preserve or /REMOVEMODE=full.', mbError, MB_OK);
    Exit;
  end;
  if UninstallSilent then
    Exit;
  form := CreateCustomForm(ScaleX(520), ScaleY(170), False, True);
  try
    form.Caption := 'Remove NextClient';
    form.ClientWidth := ScaleX(520);
    form.ClientHeight := ScaleY(170);
    preserve_button := TNewRadioButton.Create(form);
    preserve_button.Parent := form;
    preserve_button.SetBounds(ScaleX(16), ScaleY(20), ScaleX(490), ScaleY(24));
    preserve_button.Caption := 'Keep settings, modified files, maps, demos, and other user files';
    preserve_button.Checked := g_RemovalMode = 'preserve';
    full_button := TNewRadioButton.Create(form);
    full_button.Parent := form;
    full_button.SetBounds(ScaleX(16), ScaleY(60), ScaleX(490), ScaleY(24));
    full_button.Caption := 'Delete the entire game folder, including all user files';
    full_button.Checked := g_RemovalMode = 'full';
    accept_button := TNewButton.Create(form);
    accept_button.Parent := form;
    accept_button.SetBounds(ScaleX(320), ScaleY(120), ScaleX(88), ScaleY(26));
    accept_button.Caption := 'Remove';
    accept_button.ModalResult := mrOk;
    accept_button.Default := True;
    cancel_button := TNewButton.Create(form);
    cancel_button.Parent := form;
    cancel_button.SetBounds(ScaleX(416), ScaleY(120), ScaleX(88), ScaleY(26));
    cancel_button.Caption := 'Cancel';
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
      RaiseException('Could not remove installation: ' + failure);
  end;
end;
