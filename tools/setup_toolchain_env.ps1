$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

function Show-ChangePrompt {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$CurrentValue
    )

    $message = "[$Name]`n`nCurrent value:`n$CurrentValue`n`nPress 'Yes' to change path, 'No' to keep current value, 'Cancel' to stop setup."
    return [System.Windows.Forms.MessageBox]::Show(
        $message,
        "Toolchain path setup",
        [System.Windows.Forms.MessageBoxButtons]::YesNoCancel,
        [System.Windows.Forms.MessageBoxIcon]::Question
    )
}

function Select-Path {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][ValidateSet("file", "folder")][string]$Kind,
        [Parameter(Mandatory = $true)][string]$CurrentValue,
        [Parameter(Mandatory = $true)][string]$DefaultValue
    )

    $startPath = $null
    if (-not [string]::IsNullOrWhiteSpace($DefaultValue) -and (Test-Path -LiteralPath $DefaultValue)) {
        $startPath = $DefaultValue
    } elseif (-not [string]::IsNullOrWhiteSpace($CurrentValue) -and (Test-Path -LiteralPath $CurrentValue)) {
        $startPath = $CurrentValue
    }

    if ($Kind -eq "file") {
        $dialog = New-Object System.Windows.Forms.OpenFileDialog
        $dialog.Title = "Select file for $Name"
        $dialog.CheckFileExists = $true
        $dialog.Multiselect = $false

        if (-not [string]::IsNullOrWhiteSpace($startPath)) {
            if ((Get-Item -LiteralPath $startPath).PSIsContainer) {
                $dialog.InitialDirectory = $startPath
            } else {
                $dialog.InitialDirectory = [System.IO.Path]::GetDirectoryName($startPath)
                $dialog.FileName = [System.IO.Path]::GetFileName($startPath)
            }
        }

        if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
            return $dialog.FileName
        }
    } else {
        $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
        $dialog.Description = "Select folder for $Name"
        $dialog.ShowNewFolderButton = $false

        if (-not [string]::IsNullOrWhiteSpace($startPath)) {
            if ((Get-Item -LiteralPath $startPath).PSIsContainer) {
                $dialog.SelectedPath = $startPath
            } else {
                $dialog.SelectedPath = [System.IO.Path]::GetDirectoryName($startPath)
            }
        }

        if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
            return $dialog.SelectedPath
        }
    }

    return $null
}

$variables = @(
    @{
        Name = "MAKE_EXE"
        Kind = "file"
        Default = "C:\Users\$env:USERNAME\.niiet_aspect\riscv_kit_windows\bin\make.exe"
    },
    @{
        Name = "COMPILER_PATH"
        Kind = "folder"
        Default = "C:\Users\$env:USERNAME\.niiet_aspect\riscv_gcc_windows\bin"
    },
    @{
        Name = "OPENOCD_EXE"
        Kind = "file"
        Default = "C:\Users\$env:USERNAME\.niiet_aspect\riscv_kit_windows\bin\openocd.exe"
    },
    @{
        Name = "OPENOCD_SCRIPTS_ROOT"
        Kind = "folder"
        Default = "C:\Users\$env:USERNAME\.niiet_aspect\riscv_kit_windows"
    },
    @{
        Name = "GDB_EXE"
        Kind = "file"
        Default = "C:\Users\$env:USERNAME\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-gdb.exe"
    },
    @{
        Name = "OBJCOPY_EXE"
        Kind = "file"
        Default = "C:\Users\$env:USERNAME\.niiet_aspect\riscv_gcc_windows\bin\riscv64-unknown-elf-objcopy.exe"
    }
)

Write-Host "Interactive toolchain environment setup (User scope)"

$values = @{}
foreach ($var in $variables) {
    $name = $var.Name
    $kind = $var.Kind
    $current = [Environment]::GetEnvironmentVariable($name, "User")
    if ([string]::IsNullOrWhiteSpace($current)) {
        $current = $var.Default
    }

    $choice = Show-ChangePrompt -Name $name -CurrentValue $current
    if ($choice -eq [System.Windows.Forms.DialogResult]::Cancel) {
        Write-Host "Setup canceled by user."
        exit 1
    }

    if ($choice -eq [System.Windows.Forms.DialogResult]::No) {
        $values[$name] = $current
        continue
    }

    $selected = Select-Path -Name $name -Kind $kind -CurrentValue $current -DefaultValue $var.Default
    if ([string]::IsNullOrWhiteSpace($selected)) {
        # Dialog closed without selection: keep current value.
        $values[$name] = $current
    } else {
        $values[$name] = $selected
    }
}

Write-Host ""
Write-Host "Saving variables..."
$total = $variables.Count
$index = 0
foreach ($var in $variables) {
    $index++
    $name = $var.Name
    $value = $values[$name]
    Write-Host "[$index/$total] $name"
    [Environment]::SetEnvironmentVariable($name, $value, "User")
    Write-Host "$name = $value"
}

Write-Host ""
Write-Host "Done. Restart Cursor/terminal so tasks can see updated values."
