param([switch]$ValidateOnly, [ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'reports') | Out-Null

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$settingsPath = Join-Path $projectRoot 'private\launcher-usa.json'
$nativePath = Join-Path $projectRoot ('prototype-usa\out\build\win-amd64-' + $Configuration.ToLower() + '\majin.exe')
$has60 = Test-Path -LiteralPath $nativePath

$form = New-Object System.Windows.Forms.Form
$form.Text = 'Majin FK Launcher (USA 0.0.0.2)'
$form.ClientSize = New-Object System.Drawing.Size(1060,670)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.Font = New-Object System.Drawing.Font('Segoe UI',10)
$form.BackColor = [System.Drawing.Color]::FromArgb(20,29,28)
$form.ForeColor = [System.Drawing.Color]::FromArgb(235,235,218)

$assetRoot = Join-Path $projectRoot 'assets\launcher'
$imagePath = Join-Path $assetRoot 'background.png'
$logoPath = Join-Path $assetRoot 'logo.png'
$launcherLogo = $null
if (Test-Path -LiteralPath $logoPath) { $launcherLogo = [System.Drawing.Image]::FromFile($logoPath) }
$iconPath = Join-Path $assetRoot 'majin-fk.ico'
$launcherIcon = $null
if (Test-Path -LiteralPath $iconPath) {
    $launcherIcon = New-Object System.Drawing.Icon($iconPath)
    $form.Icon = $launcherIcon
}
$form.BackgroundImageLayout = 'Zoom'
if (Test-Path -LiteralPath $imagePath) { $form.BackgroundImage = [System.Drawing.Image]::FromFile($imagePath) }

$paintBackground = {
    param($sender,$eventArgs)
    if ($sender.ClientSize.Width -le 0 -or $sender.ClientSize.Height -le 0) { return }
    $rect = New-Object System.Drawing.Rectangle(0,0,[Math]::Min(520,$sender.ClientSize.Width),$sender.ClientSize.Height)
    $shade = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect,
        [System.Drawing.Color]::FromArgb(250,12,23,22),
        [System.Drawing.Color]::FromArgb(0,12,23,22),0.0)
    try { $eventArgs.Graphics.FillRectangle($shade,$rect) } finally { $shade.Dispose() }
}
$form.Add_Paint($paintBackground)

function Add-Label($text,$y,$width=385) {
    $label = New-Object System.Windows.Forms.Label
    $label.BackColor = [System.Drawing.Color]::Transparent
    $label.Text = $text
    $label.SetBounds(32,$y,$width,28)
    $form.Controls.Add($label)
}

function Add-Choice($title,$y,$items) {
    Add-Label $title $y 150
    $box = New-Object System.Windows.Forms.ComboBox
    $box.DropDownStyle = 'DropDownList'
    $box.SetBounds(187,$y,230,28)
    $box.BackColor = [System.Drawing.Color]::FromArgb(38,49,46)
    $box.ForeColor = $form.ForeColor
    $box.FlatStyle = 'Flat'
    $box.Items.AddRange([object[]]$items)
    $box.SelectedIndex = 0
    $form.Controls.Add($box)
    $box.BringToFront()
    return $box
}

$heading = New-Object System.Windows.Forms.Label
$heading.BackColor = [System.Drawing.Color]::Transparent
$heading.SetBounds(28,12,720,124)
$heading.Add_Paint({
    param($sender,$eventArgs)
    if (!$launcherLogo) { return }
    $sourceRect = New-Object System.Drawing.Rectangle(24,168,2067,385)
    $factor = [Math]::Min($sender.ClientSize.Width/[double]$sourceRect.Width,
        $sender.ClientSize.Height/[double]$sourceRect.Height)
    if ($factor -le 0) { return }
    $destination = New-Object System.Drawing.Rectangle(0,0,
        [int][Math]::Round($sourceRect.Width*$factor),[int][Math]::Round($sourceRect.Height*$factor))
    $eventArgs.Graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $eventArgs.Graphics.DrawImage($launcherLogo,$destination,$sourceRect,[System.Drawing.GraphicsUnit]::Pixel)
})
$form.Controls.Add($heading)

Add-Label 'DISPLAY SETTINGS (USA EDITION)' 145
$size = Add-Choice 'Window size' 185 @('1280x720','1920x1080','2560x1440','3840x2160')
$size.SelectedIndex = 1 # Default 1920x1080
$mode = Add-Choice 'Display mode' 225 @('Fullscreen','Windowed')
$mode.SelectedIndex = 1 # Default Windowed
$scale = Add-Choice 'Render scale' 265 @('1x - Native (720p)','2x - 1440p (QHD)','3x - 2160p (4K UHD)')
$effect = Add-Choice 'Anti-aliasing' 305 @('Original','FXAA')
$sync = Add-Choice 'VSync / FPS' 345 @('On','Off')
$sync.Width = 82

$frameRate = New-Object System.Windows.Forms.ComboBox
$frameRate.DropDownStyle = 'DropDownList'
$frameRate.SetBounds(277,345,140,28)
$frameRate.BackColor = $sync.BackColor
$frameRate.ForeColor = $sync.ForeColor
$frameRate.FlatStyle = 'Flat'
$frameRate.Items.AddRange([object[]]@('Original (30 FPS)','60 FPS'))
$frameRate.SelectedIndex = 1 # Default 60 FPS
$form.Controls.Add($frameRate)
$frameRate.Enabled = $has60

$renderingPath = Add-Choice 'Rendering path' 385 @('Default','ROV Compatibility')
$renderingPath.SelectedIndex = 1 # Default ROV for NVIDIA/AMD EDRAM compatibility
$cutsceneSkip = Add-Choice 'Skip cutscenes' 425 @('Hold Select / Tab','Disabled')
$cutsceneSkip.SelectedIndex = 0 # Default Enabled

$renderTooltip = New-Object System.Windows.Forms.ToolTip
$renderTooltip.SetToolTip($frameRate,'60 FPS mode with normalized title timer, or Original 30 FPS.')
$renderTooltip.SetToolTip($renderingPath,'ROV Compatibility is recommended for NVIDIA/AMD GPUs so 3D EDRAM scenes render properly.')
$renderTooltip.SetToolTip($cutsceneSkip,'Allows skipping in-game 3D cutscenes directly to the end by holding Select/Back on controller or Tab on keyboard.')

$inputMode = Add-Choice 'Input' 465 @('Controller','Keyboard + mouse')
$controlHelp = New-Object System.Windows.Forms.Button
$controlHelp.Text = 'Controls / sensitivity'
$controlHelp.SetBounds(187,497,230,28)
$controlHelp.FlatStyle = 'Flat'
$controlHelp.BackColor = [System.Drawing.Color]::FromArgb(38,49,46)
$form.Controls.Add($controlHelp)
$script:mouseSensitivity = 1.0

$controlHelp.Add_Click({
    $dialog = New-Object System.Windows.Forms.Form
    $dialog.Text = 'Keyboard + mouse settings'
    $dialog.ClientSize = New-Object System.Drawing.Size(480,390)
    $dialog.StartPosition = 'CenterParent'; $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false; $dialog.MinimizeBox = $false
    $dialog.Font = $form.Font; $dialog.BackColor = $form.BackColor; $dialog.ForeColor = $form.ForeColor
    $help = New-Object System.Windows.Forms.Label
    $help.SetBounds(20,16,440,270)
    $help.Text = "WASD: move | Mouse / IJKL: camera`nSpace: A (jump / confirm)`nHold Shift: B (run / dash)`nE: B (interact / back)`nLeft mouse: X (attack) | R: Y (roll)`nHold Q: LT (crouch) | Right mouse: RT (command)`nF: LB (throw mode) | C: RB (ability)`nArrows: D-pad | V: LS click | Middle mouse: RS click`nEnter: pause (Start) | Tab: map (Back) | Esc: quit"
    $dialog.Controls.Add($help)
    $caption = New-Object System.Windows.Forms.Label
    $caption.Text = 'Mouse sensitivity'; $caption.SetBounds(20,290,180,28)
    $dialog.Controls.Add($caption)
    $sensitivity = New-Object System.Windows.Forms.NumericUpDown
    $sensitivity.SetBounds(220,290,120,28); $sensitivity.DecimalPlaces = 2
    $sensitivity.Minimum = 0.01; $sensitivity.Maximum = 10; $sensitivity.Increment = 0.1
    $sensitivity.Value = [decimal]$script:mouseSensitivity
    $dialog.Controls.Add($sensitivity)
    $ok = New-Object System.Windows.Forms.Button
    $ok.Text = 'OK'; $ok.SetBounds(340,340,120,32); $ok.DialogResult = 'OK'
    $dialog.Controls.Add($ok); $dialog.AcceptButton = $ok
    if ($dialog.ShowDialog($form) -eq 'OK') { $script:mouseSensitivity = [double]$sensitivity.Value }
    $dialog.Dispose()
})

$script:recordPerformance = $true
$script:includeHardware = $false

$guide = New-Object System.Windows.Forms.Panel
$guide.SetBounds(650,150,380,444)
$guide.BackColor = [System.Drawing.Color]::FromArgb(28,40,36)
$form.Controls.Add($guide)
$guideHeading = New-Object System.Windows.Forms.Label
$guideHeading.Text = 'KEYBOARD + MOUSE'
$guideHeading.SetBounds(16,12,348,26)
$guideHeading.ForeColor = [System.Drawing.Color]::FromArgb(236,225,173)
$guide.Controls.Add($guideHeading)
$rows = @(
    @('WASD','Move (left stick)'),
    @('Mouse / IJKL','Camera (right stick)'),
    @('Space','Jump / confirm (A)'),
    @('Hold Shift','Run / dash (B)'),
    @('E','Interact / back (B)'),
    @('Left mouse','Attack (X)'),
    @('R','Roll (Y)'),
    @('Hold Q','Crouch (LT)'),
    @('Right mouse','Command Majin (RT)'),
    @('F / C','Throw mode (LB) / ability (RB)'),
    @('Arrow keys','D-pad'),
    @('V / Middle mouse','Stick clicks (LS / RS)'),
    @('Enter / Tab','Pause menu / map'),
    @('Esc','Quit confirmation')
)
$y = 46
foreach ($row in $rows) {
    $key = New-Object System.Windows.Forms.Label
    $key.Text = $row[0]; $key.SetBounds(16,$y,145,25)
    $key.ForeColor = [System.Drawing.Color]::FromArgb(236,225,173)
    $guide.Controls.Add($key)
    $action = New-Object System.Windows.Forms.Label
    $action.Text = $row[1]; $action.SetBounds(163,$y,210,25)
    $action.Font = New-Object System.Drawing.Font('Segoe UI',9)
    $guide.Controls.Add($action)
    $y += 25
}
$note = New-Object System.Windows.Forms.Label
$note.Text = 'Xbox prompts remain in-game. Actions depend on context.'
$note.SetBounds(16,402,348,38)
$note.Font = New-Object System.Drawing.Font('Segoe UI',9)
$guide.Controls.Add($note)

$defaultGameData = Join-Path $projectRoot 'game-data-usa'
$dataLabel = New-Object System.Windows.Forms.Label
$dataLabel.Text = 'Game data folder'; $dataLabel.SetBounds(32,610,150,26)
$dataLabel.BackColor = [System.Drawing.Color]::Transparent
$form.Controls.Add($dataLabel)

$dataPath = New-Object System.Windows.Forms.TextBox
$dataPath.SetBounds(187,607,650,28)
$dataPath.Text = $defaultGameData
$form.Controls.Add($dataPath)

$browse = New-Object System.Windows.Forms.Button
$browse.Text = 'Browse...'; $browse.SetBounds(853,605,150,32)
$form.Controls.Add($browse)
$browse.Add_Click({
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Select the extracted Majin USA game folder containing default.xex.'
    $dialog.ShowNewFolderButton = $false
    if (Test-Path -LiteralPath $dataPath.Text -PathType Container) { $dialog.SelectedPath = $dataPath.Text }
    try { if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $dataPath.Text = $dialog.SelectedPath } }
    finally { $dialog.Dispose() }
})

if (Test-Path -LiteralPath $settingsPath) {
    try {
        $saved = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
        if ($size.Items.Contains([string]$saved.WindowSize)) { $size.SelectedItem = [string]$saved.WindowSize }
        if ($saved.Fullscreen -is [bool]) { $mode.SelectedIndex = [int](-not $saved.Fullscreen) }
        if ($saved.VSync -is [bool]) { $sync.SelectedIndex = [int](-not $saved.VSync) }
        if ($saved.RenderScale -in @(1,2,3)) { $scale.SelectedIndex = [int]$saved.RenderScale - 1 }
        if ($has60) { $frameRate.SelectedIndex = if ($saved.FrameRateMode -eq '60') { 1 } else { 0 } }
        $renderingPath.SelectedIndex = if ($saved.RenderingPath -eq 'Default') { 0 } else { 1 }
        if ($saved.SkipCutscenes -is [bool]) { $cutsceneSkip.SelectedIndex = if ($saved.SkipCutscenes) { 0 } else { 1 } }
        $effect.SelectedIndex = if ($saved.PostEffect -eq 'fxaa') { 1 } else { 0 }
        if ($saved.RecordPerformance -is [bool]) { $script:recordPerformance = $saved.RecordPerformance }
        if ($saved.InputMode -eq 'KeyboardMouse') { $inputMode.SelectedIndex = 1 } else { $inputMode.SelectedIndex = 0 }
        if ($saved.MouseSensitivity -is [ValueType] -and $saved.MouseSensitivity -ge 0.01 -and $saved.MouseSensitivity -le 10) {
            $script:mouseSensitivity = [double]$saved.MouseSensitivity
        }
        if ($saved.GameDataRoot -and (Test-Path -LiteralPath $saved.GameDataRoot)) {
            $dataPath.Text = [string]$saved.GameDataRoot
        }
    } catch {
        [System.Windows.Forms.MessageBox]::Show('Saved settings could not be read. Defaults will be used.','Majin') | Out-Null
    }
}

$play = New-Object System.Windows.Forms.Button
$play.Text = 'Play'; $play.SetBounds(207,538,210,46); $form.Controls.Add($play)
$cancel = New-Object System.Windows.Forms.Button
$cancel.Text = 'Cancel'; $cancel.SetBounds(32,538,155,46); $form.Controls.Add($cancel)
$play.FlatStyle = 'Flat'
$play.BackColor = [System.Drawing.Color]::FromArgb(191,165,104)
$play.ForeColor = [System.Drawing.Color]::FromArgb(20,29,28)
$cancel.FlatStyle = 'Flat'
$cancel.BackColor = [System.Drawing.Color]::FromArgb(38,49,46)
$cancel.Add_Click({ $form.Close() })
$form.CancelButton = $cancel

$play.Add_Click({
    try {
        $selectedGameData = $dataPath.Text.Trim()
        if (-not (Test-Path -LiteralPath (Join-Path $selectedGameData 'default.xex'))) {
            throw "default.xex was not found in: $selectedGameData"
        }
        $options = @{
            Configuration = $Configuration
            GameDataRoot = $selectedGameData
            WindowSize = [string]$size.SelectedItem
            Fullscreen = ($mode.SelectedIndex -eq 0)
            VSync = ($sync.SelectedIndex -eq 0)
            RenderScale = ($scale.SelectedIndex + 1)
            PostEffect = (@('none','fxaa')[$effect.SelectedIndex])
            FrameRateMode = (@('Original','60')[$frameRate.SelectedIndex])
            RenderingPath = (@('Default','ROV')[$renderingPath.SelectedIndex])
            SkipCutscenes = ($cutsceneSkip.SelectedIndex -eq 0)
            InputMode = (@('Controller','KeyboardMouse')[$inputMode.SelectedIndex])
            MouseSensitivity = $script:mouseSensitivity
            RecordPerformance = $script:recordPerformance
            IncludeHardware = $script:includeHardware
        }
        $folder = Split-Path $settingsPath -Parent
        New-Item -ItemType Directory -Force -Path $folder | Out-Null
        $tempPath = "$settingsPath.tmp"
        $options | ConvertTo-Json | Set-Content -LiteralPath $tempPath -Encoding UTF8
        Move-Item -LiteralPath $tempPath -Destination $settingsPath -Force
        $form.Hide()
        & (Join-Path $PSScriptRoot 'run-usa.ps1') @options
        $form.Close()
    } catch {
        $form.Show()
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message,'Unable to launch Majin') | Out-Null
    }
})

if ($ValidateOnly) {
    if (!$form.BackgroundImage -or !$launcherIcon -or !$launcherLogo) {
        throw 'Packaged launcher background, logo or icon was not loaded.'
    }
    $form.ShowInTaskbar = $false
    $form.StartPosition = 'Manual'
    $form.Location = New-Object System.Drawing.Point(-10000,-10000)
    $form.Show()
    $form.CreateControl()
    $form.PerformLayout()
    $bitmap = New-Object System.Drawing.Bitmap($form.Width,$form.Height)
    $form.DrawToBitmap($bitmap,(New-Object System.Drawing.Rectangle(0,0,$form.Width,$form.Height)))
    $bitmap.Save((Join-Path $projectRoot 'reports\launcher-usa-preview.png'))
    $bitmap.Dispose()
    Write-Output 'Launcher USA validated and preview saved to reports\launcher-usa-preview.png'
} else {
    [void]$form.ShowDialog()
}

if ($form.BackgroundImage) { $form.BackgroundImage.Dispose() }
$renderTooltip.Dispose()
$form.Dispose()
if ($launcherIcon) { $launcherIcon.Dispose() }
if ($launcherLogo) { $launcherLogo.Dispose() }
