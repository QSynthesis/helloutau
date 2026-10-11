; The page of the uninstaller that offers to remove the settings and the temporary files as well.
; See docs/Distribution.md.
;
; The uninstaller of Inno Setup has no wizard pages, so the page is added to the inner notebook of
; the window of the uninstaller, before its page of the progress. The page therefore has the
; header, the image and the size of that window. The controls are laid out for their texts: the
; buttons take the width that the window calculates for their captions, the texts wrap and adjust
; their heights, and the check box holds a short caption alone, because a check box does not wrap.

[Code]

var
    OptionsPage: TNewNotebookPage;
    RemoveDataCheckBox: TNewCheckBox;
    UninstallButton: TNewButton;

// Removes OpenVPI\HelloUtau in Parent, and then OpenVPI if it is empty. RemoveDir() removes only
// an empty directory, which keeps the directories of the other applications of OpenVPI.
procedure RemoveAppDirectory(const Parent: String);
var
    Organization: String;
begin
    if Parent = '' then
        Exit;
    Organization := AddBackslash(Parent) + 'OpenVPI';
    DelTree(Organization + '\{#AppName}', True, True, True);
    RemoveDir(Organization);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
    if (CurUninstallStep <> usPostUninstall) or (RemoveDataCheckBox = nil) or
        not RemoveDataCheckBox.Checked then
        Exit;
    // OpenVPI\HelloUtau in the documents folder is never removed. It holds the voice banks and
    // the plugins of the user, which the user usually obtained elsewhere and arranged by hand,
    // and which cannot be restored. diffscope removes its documents folder, HelloUtau does not.
    RemoveAppDirectory(ExpandConstant('{userappdata}'));
    RemoveAppDirectory(ExpandConstant('{localappdata}'));
    // The temporary files of the application are in one directory, see TemporaryStorage.
    RemoveAppDirectory(GetEnv('TMP'));
    RemoveAppDirectory(GetEnv('TEMP'));
end;

// Returns a text of the page at Top, which wraps within the width of the status label of the
// window and takes the height of its lines.
function CreatePageText(const Caption: String; const Top, Indent: Integer): TNewStaticText;
begin
    Result := TNewStaticText.Create(UninstallProgressForm);
    Result.Parent := OptionsPage;
    Result.Left := UninstallProgressForm.StatusLabel.Left + Indent;
    Result.Top := Top;
    Result.Width := UninstallProgressForm.StatusLabel.Width - Indent;
    Result.WordWrap := True;
    Result.ShowAccelChar := False;
    Result.Caption := Caption;
    Result.AdjustHeight;
end;

// Shows the page of the options before the uninstallation, unless the uninstallation is silent.
// A silent uninstallation removes no data of the user.
procedure InitializeUninstallProgressForm();
var
    Form: TUninstallProgressForm;
    Text: TNewStaticText;
    PageName, PageDescription: String;
    CancelLeft, CancelWidth, CancelModalResult, ButtonWidth: Integer;
    CancelEnabled: Boolean;
begin
    if UninstallSilent then
        Exit;
    Form := UninstallProgressForm;

    OptionsPage := TNewNotebookPage.Create(Form);
    OptionsPage.Notebook := Form.InnerNotebook;
    OptionsPage.Parent := Form.InnerNotebook;
    OptionsPage.Align := alClient;

    Text := CreatePageText(ExpandConstant('{cm:UninstallOptionsLabel}'), Form.StatusLabel.Top, 0);

    RemoveDataCheckBox := TNewCheckBox.Create(Form);
    RemoveDataCheckBox.Parent := OptionsPage;
    RemoveDataCheckBox.Left := Form.StatusLabel.Left;
    RemoveDataCheckBox.Top := Text.Top + Text.Height + ScaleY(16);
    RemoveDataCheckBox.Width := Form.StatusLabel.Width;
    RemoveDataCheckBox.Height := ScaleY(17);
    RemoveDataCheckBox.Caption := ExpandConstant('{cm:RemoveUserData}');
    RemoveDataCheckBox.Checked := True;

    // Indented as far as the caption of the check box
    CreatePageText(ExpandConstant('{cm:RemoveUserDataNote}'),
        RemoveDataCheckBox.Top + RemoveDataCheckBox.Height + ScaleY(4), ScaleX(18));

    // Uninstall to the left of Cancel, both as wide as the wider caption requires, with the right
    // edge of Cancel kept
    UninstallButton := TNewButton.Create(Form);
    UninstallButton.Parent := Form;
    UninstallButton.Caption := ExpandConstant('{cm:UninstallButton}');
    UninstallButton.ModalResult := mrOK;
    CancelLeft := Form.CancelButton.Left;
    CancelWidth := Form.CancelButton.Width;
    ButtonWidth := Form.CalculateButtonWidth([UninstallButton.Caption, Form.CancelButton.Caption]);
    if ButtonWidth < CancelWidth then
        ButtonWidth := CancelWidth;
    Form.CancelButton.Left := CancelLeft + CancelWidth - ButtonWidth;
    Form.CancelButton.Width := ButtonWidth;
    UninstallButton.Top := Form.CancelButton.Top;
    UninstallButton.Height := Form.CancelButton.Height;
    UninstallButton.Width := ButtonWidth;
    UninstallButton.Left := Form.CancelButton.Left - ButtonWidth - ScaleX(6);
    UninstallButton.TabOrder := Form.CancelButton.TabOrder;
    Form.CancelButton.TabOrder := UninstallButton.TabOrder + 1;

    Form.InnerNotebook.ActivePage := OptionsPage;
    PageName := Form.PageNameLabel.Caption;
    PageDescription := Form.PageDescriptionLabel.Caption;
    Form.PageNameLabel.Caption := ExpandConstant('{cm:UninstallOptionsTitle}');
    Form.PageDescriptionLabel.Caption := ExpandConstant('{cm:UninstallOptionsDescription}');
    CancelEnabled := Form.CancelButton.Enabled;
    CancelModalResult := Form.CancelButton.ModalResult;
    Form.CancelButton.Enabled := True;
    Form.CancelButton.ModalResult := mrCancel;

    if Form.ShowModal = mrCancel then
        Abort;

    // The page of the progress follows with the standard header and buttons.
    UninstallButton.Visible := False;
    Form.CancelButton.Left := CancelLeft;
    Form.CancelButton.Width := CancelWidth;
    Form.CancelButton.Enabled := CancelEnabled;
    Form.CancelButton.ModalResult := CancelModalResult;
    Form.PageNameLabel.Caption := PageName;
    Form.PageDescriptionLabel.Caption := PageDescription;
    Form.InnerNotebook.ActivePage := Form.InstallingPage;
end;
