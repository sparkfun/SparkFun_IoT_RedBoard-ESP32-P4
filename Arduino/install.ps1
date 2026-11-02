# Install the RedBoard ESP32-P4 board files, the ESP32-C5 drop-in (drop-in/) and the
# Ethernet patch (patches/, needed by the PoE board) into an arduino-esp32 3.3.12 core.
# Windows. macOS and Linux: install.sh.
#
#   .\install.ps1                     this board only (default)
#   .\install.ps1 -AllP4              patch the stock libs for every ESP32-P4 board
#   .\install.ps1 -Uninstall          undo either
#   .\install.ps1 -ArduinoDir DIR     Arduino data folder, if not the default
#
# Close the Arduino IDE first. If scripts are blocked, run:
#   powershell -ExecutionPolicy Bypass -File .\install.ps1

param(
    [switch]$AllP4,
    [switch]$Uninstall,
    [string]$ArduinoDir = ""
)

$ErrorActionPreference = "Stop"

$CoreVersion = "3.3.12"
$Board = "sparkfun_esp32p4_iot_redboard_wifi"
$LibsDirName = "esp32p4-c5-libs"
$BeginMark = "# BEGIN SparkFun RedBoard ESP32-P4 C5 drop-in"
$EndMark = "# END SparkFun RedBoard ESP32-P4 C5 drop-in"
$Variants = @("sparkfun_esp32p4_iot_redboard_wifi", "sparkfun_esp32p4_iot_redboard_poe")
$DropInFiles = @(
    "lib/libespressif__esp_hosted.a",
    "lib/libespressif__esp_wifi_remote.a",
    "qio_qspi/include/sdkconfig.h",
    "dio_qspi/include/sdkconfig.h",
    "sdkconfig"
)
# Ethernet library files replaced by drop-in/Ethernet (patches/0001-ETH-esp32p4-rmii-clock-loopback.patch),
# with their stock 3.3.12 checksums.
$EthFiles = [ordered]@{
    "ETH.h"   = "5c8fe676e61ffe33be8363d20d23c5bf242c11f1b85e2a2a91d100177dd79904"
    "ETH.cpp" = "90a68fa2e6ad7d9de3e31ec20b035ba2ba560978d4e1cbe541b750f3059b109a"
}

# Join path segments; "/" in any segment becomes the native separator.
function P { [IO.Path]::Combine([string[]]($args | ForEach-Object { $_.Replace("/", [IO.Path]::DirectorySeparatorChar) })) }

$Here = $PSScriptRoot
$DropIn = P $Here "drop-in"
$BoardFiles = $Here   # boards.txt and variants/

function Die([string]$msg) { Write-Host "error: $msg" -ForegroundColor Red; exit 1 }
function Info([string]$msg) { Write-Host "  $msg" }
function Sha256([string]$path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLower() }

if (-not $ArduinoDir) {
    if (Get-Command arduino-cli -ErrorAction SilentlyContinue) {
        try { $ArduinoDir = (& arduino-cli config get directories.data 2>$null | Out-String).Trim() } catch { $ArduinoDir = "" }
    }
    if (-not $ArduinoDir -or -not (Test-Path -LiteralPath $ArduinoDir)) {
        $ArduinoDir = P $env:LOCALAPPDATA "Arduino15"
    }
}

$Core = P $ArduinoDir "packages/esp32/hardware/esp32" $CoreVersion
$Stock = P $ArduinoDir "packages/esp32/tools/esp32p4-libs" $CoreVersion
$StockBackup = "$Stock.stock"
$BoardLibs = P $Core "tools" $LibsDirName
$BoardsTxt = P $Core "boards.txt"
$EthSrc = P $Core "libraries/Ethernet/src"
$EthBackup = P $Core "tools/redboard-p4-eth-stock"

Write-Host "Arduino data folder: $ArduinoDir"
if (-not (Test-Path -LiteralPath $Core)) { Die "arduino-esp32 $CoreVersion not found at $Core. Install it with Boards Manager (esp32 by Espressif Systems, version $CoreVersion)." }
if (-not (Test-Path -LiteralPath $Stock)) { Die "ESP32-P4 libraries not found at $Stock. Reinstall arduino-esp32 $CoreVersion." }
if (-not (Test-Path -LiteralPath $BoardsTxt)) { Die "$BoardsTxt not found." }

# boards.txt is read and written as LF-terminated UTF-8 without a BOM, as shipped.
$Utf8 = New-Object System.Text.UTF8Encoding $false
function Read-Lines([string]$path) { [IO.File]::ReadAllText($path, $Utf8).TrimEnd("`r", "`n") -split "\r?\n" }
function Read-Boards { Read-Lines $BoardsTxt }
function Write-Boards([string[]]$lines) { [IO.File]::WriteAllText($BoardsTxt, (($lines -join "`n") + "`n"), $Utf8) }

# Lines between our markers, and boards.txt without them.
function Split-Block {
    $inside = $false; $block = @(); $rest = @()
    foreach ($l in Read-Boards) {
        if ($l -eq $BeginMark) { $inside = $true; continue }
        if ($l -eq $EndMark) { $inside = $false; continue }
        if ($inside) { $block += $l } else { $rest += $l }
    }
    return @{ Block = $block; Rest = $rest; Found = ((Read-Boards) -contains $BeginMark) }
}

# Replace the Ethernet library files with the patched ones, keeping the stock files in $EthBackup.
# Leaves files alone that are neither stock nor already patched.
function Patch-Eth {
    $stock = @()
    foreach ($f in $EthFiles.Keys) {
        $sum = Sha256 (P $EthSrc $f)
        if ($sum -eq $EthFiles[$f]) {
            $stock += $f
        } elseif ($sum -ne (Sha256 (P $DropIn "Ethernet" $f))) {
            Write-Host "  warning: $(P $EthSrc $f) has been modified; not applying the Ethernet patch (the PoE board's Ethernet won't start)" -ForegroundColor Yellow
            return
        }
    }
    if ($stock.Count -eq 0) {
        Info "Ethernet library already patched"
        return
    }
    New-Item -ItemType Directory -Force -Path $EthBackup | Out-Null
    foreach ($f in $stock) {
        Copy-Item -LiteralPath (P $EthSrc $f) -Destination (P $EthBackup $f) -Force
        Copy-Item -LiteralPath (P $DropIn "Ethernet" $f) -Destination (P $EthSrc $f) -Force
    }
    Info "patched the Ethernet library (RMII clock loopback for the PoE board)"
}

function Unpatch-Eth {
    if (-not (Test-Path -LiteralPath $EthBackup)) { return }
    foreach ($f in $EthFiles.Keys) {
        $b = P $EthBackup $f
        if (Test-Path -LiteralPath $b) { Copy-Item -LiteralPath $b -Destination (P $EthSrc $f) -Force }
    }
    Remove-Item -LiteralPath $EthBackup -Recurse -Force
    Info "restored the stock Ethernet library"
}

function Do-Uninstall {
    Unpatch-Eth
    $s = Split-Block
    if ($s.Found) {
        if ($s.Block | Where-Object { $_.StartsWith("$Board.name=") }) {
            foreach ($v in $Variants) {
                $p = P $Core "variants" $v
                if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force }
            }
            Info "removed RedBoard ESP32-P4 board files"
        }
        Write-Boards $s.Rest
        Info "removed boards.txt entries"
    }
    if (Test-Path -LiteralPath $BoardLibs) {
        Remove-Item -LiteralPath $BoardLibs -Recurse -Force
        Info "removed $BoardLibs"
    }
    if (Test-Path -LiteralPath $StockBackup) {
        Remove-Item -LiteralPath $Stock -Recurse -Force
        Rename-Item -LiteralPath $StockBackup -NewName (Split-Path $Stock -Leaf)
        Info "restored stock ESP32-P4 libraries"
    }
}

if ($Uninstall) {
    Write-Host "Uninstalling"
    Do-Uninstall
    Write-Host "Done. Restart the Arduino IDE."
    exit 0
}

# Check the drop-in files.
if (-not (Test-Path -LiteralPath $DropIn)) { Die "$DropIn not found; run this script from its own folder in the download." }
foreach ($line in Get-Content -LiteralPath (P $DropIn "SHA256SUMS")) {
    if (-not $line.Trim()) { continue }
    $sum, $file = $line -split "\s+", 2
    $path = P $DropIn $file
    if (-not (Test-Path -LiteralPath $path)) { Die "drop-in/$file is missing" }
    if ((Sha256 $path) -ne $sum) { Die "drop-in/$file is corrupt (checksum mismatch)" }
}

# The source must be the stock libs (or their backup if -AllP4 was run before).
$src = $Stock
if (Test-Path -LiteralPath $StockBackup) { $src = $StockBackup }
function Component-Versions([string]$path) { Get-Content -LiteralPath $path | Where-Object { $_ -match "__" } | Sort-Object }
if (Compare-Object (Component-Versions (P $src "versions.txt")) (Component-Versions (P $DropIn "versions.txt"))) {
    Die "the ESP32-P4 libraries at $src don't match the versions this drop-in was built for. Reinstall arduino-esp32 $CoreVersion."
}

if ($AllP4) { Write-Host "Installing (mode: all ESP32-P4 boards)" } else { Write-Host "Installing (mode: this board only)" }
Do-Uninstall   # start from a clean state; restores $Stock from the backup if there was one
$src = $Stock

# Board files, if the core doesn't have this board yet.
$block = @()
if (-not ((Read-Boards) | Where-Object { $_.StartsWith("$Board.name=") })) {
    $srcBoards = P $BoardFiles "boards.txt"
    if (-not (Test-Path -LiteralPath $srcBoards) -or -not (Test-Path -LiteralPath (P $BoardFiles "variants" $Board))) {
        Die "the core has no $Board board and no board files were found in $BoardFiles"
    }
    foreach ($v in $Variants) { Copy-Item -LiteralPath (P $BoardFiles "variants" $v) -Destination (P $Core "variants") -Recurse -Force }
    $block += Read-Lines $srcBoards
    Info "added RedBoard ESP32-P4 board files"
}

if ($AllP4) {
    Rename-Item -LiteralPath $Stock -NewName (Split-Path $StockBackup -Leaf)
    Copy-Item -LiteralPath $StockBackup -Destination $Stock -Recurse
    $target = $Stock
    Info "backed up stock ESP32-P4 libraries to $StockBackup"
} else {
    Copy-Item -LiteralPath $src -Destination $BoardLibs -Recurse
    $target = $BoardLibs
    $block += "$Board.tools.esp32-arduino-libs.path={runtime.platform.path}/tools/$LibsDirName"
    # Windows: cmd.exe COPY in the core's prebuild hooks rejects forward slashes in the path.
    $block += "$Board.tools.esp32-arduino-libs.path.windows={runtime.platform.path}\tools\$LibsDirName"
}

foreach ($f in $DropInFiles) {
    $from = P $DropIn $f
    $to = P $target $f
    Copy-Item -LiteralPath $from -Destination $to -Force
    if ((Sha256 $to) -ne (Sha256 $from)) { Die "copy of $f failed" }
}
Info "installed drop-in into $target"

Patch-Eth

if ($block.Count -gt 0) {
    Write-Boards (@(Read-Boards) + $BeginMark + $block + $EndMark)
    Info "updated boards.txt"
}

Write-Host "Done. Restart the Arduino IDE and select 'SparkFun IoT RedBoard - ESP32-P4 (WiFi/BLE)' or '(PoE)'."
Write-Host "Updating or reinstalling the esp32 core removes this; run the script again afterwards."
