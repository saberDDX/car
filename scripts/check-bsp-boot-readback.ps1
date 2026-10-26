# Read Windows image files only. Does not access devices or invoke RKDevTool.
param(
    [string]$ExportImagePath,
    [Alias('BackupGzipPath')]
    [string]$BackupImagePath
)
$ErrorActionPreference = 'Stop'

function Select-ImageFile([string]$Title, [string]$Filter) {
    Add-Type -AssemblyName System.Windows.Forms
    $picker = New-Object System.Windows.Forms.OpenFileDialog
    try {
        $picker.Title = $Title
        $picker.Filter = $Filter
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
            throw 'No file selected; nothing changed.'
        }
        return $picker.FileName
    } finally {
        $picker.Dispose()
    }
}

function Read-ImagePrefix([string]$Path, [bool]$Gzip) {
    $file = [System.IO.File]::OpenRead($Path)
    $stream = $file
    try {
        if ($Gzip) {
            $stream = [System.IO.Compression.GZipStream]::new(
                $file, [System.IO.Compression.CompressionMode]::Decompress)
        }
        $buffer = New-Object byte[] 4096
        $offset = 0
        while ($offset -lt $buffer.Length) {
            $count = $stream.Read($buffer, $offset, $buffer.Length - $offset)
            if ($count -eq 0) { throw 'Image prefix is truncated.' }
            $offset += $count
        }
        return ,$buffer
    } finally {
        $stream.Dispose()
        if ($Gzip) { $file.Dispose() }
    }
}

function Read-AuditedBackup([string]$Path, [bool]$Gzip) {
    # Validate the uncompressed bytes, so re-compressing an unchanged backup
    # cannot cause a false failure. Keep the recorded raw SHA256 mandatory.
    $expectedBytes = 134217728
    $expectedRawHash = '0b4b656ab67b9365fe7b5e3de90940f45f8e95c293ad26823815a09a4c133444'
    $file = [System.IO.File]::OpenRead($Path)
    $stream = $file
    $hash = [System.Security.Cryptography.SHA256]::Create()
    try {
        if ($Gzip) {
            $stream = [System.IO.Compression.GZipStream]::new(
                $file, [System.IO.Compression.CompressionMode]::Decompress)
        }
        $buffer = New-Object byte[] 1048576
        $prefix = New-Object byte[] 4096
        $prefixBytes = 0
        [long]$total = 0
        while ($true) {
            $limit = [int][Math]::Min($buffer.Length, $expectedBytes + 1 - $total)
            $count = $stream.Read($buffer, 0, $limit)
            if ($count -eq 0) { break }
            $total += $count
            if ($total -gt $expectedBytes) {
                throw 'Original backup expands beyond 128 MiB. Select the original boot partition backup.'
            }
            if ($prefixBytes -lt $prefix.Length) {
                $copy = [Math]::Min($count, $prefix.Length - $prefixBytes)
                [Array]::Copy($buffer, 0, $prefix, $prefixBytes, $copy)
                $prefixBytes += $copy
            }
            [void]$hash.TransformBlock($buffer, 0, $count, $buffer, 0)
        }
        [void]$hash.TransformFinalBlock([byte[]]@(), 0, 0)
        $rawHash = [BitConverter]::ToString($hash.Hash).Replace('-', '').ToLowerInvariant()
        Write-Host "Original backup raw bytes: $total"
        Write-Host "Original backup raw SHA256: $rawHash"
        if ($total -ne $expectedBytes -or $rawHash -ne $expectedRawHash) {
            throw "Original raw backup does not match the recorded snapshot. Expected SHA256=$expectedRawHash. Re-select the ORIGINAL backup; do not select the new RKDevTool export."
        }
        return [pscustomobject]@{ Prefix = $prefix; RawSha256 = $rawHash; RawBytes = $total }
    } finally {
        $hash.Dispose()
        $stream.Dispose()
        if ($Gzip) { $file.Dispose() }
    }
}

if (-not $ExportImagePath) {
    $ExportImagePath = Select-ImageFile 'Select the NEW 128 MiB RKDevTool boot export' 'Raw images (*.img)|*.img|All files (*.*)|*.*'
}
$ExportImagePath = (Resolve-Path -LiteralPath $ExportImagePath).ProviderPath
Write-Host "Selected NEW export: $ExportImagePath"
$size = (Get-Item -LiteralPath $ExportImagePath).Length
if ($size -ne 134217728) {
    throw "Wrong export size: $size bytes; expected 134217728. Select the new full boot export, not the 20 KiB GPT file."
}

if (-not $BackupImagePath) {
    $BackupImagePath = Select-ImageFile 'Select the ORIGINAL original-boot.img.gz OR original-boot.img (NOT the new export)' 'Boot backups (*.gz;*.img)|*.gz;*.img|All files (*.*)|*.*'
}
$BackupImagePath = (Resolve-Path -LiteralPath $BackupImagePath).ProviderPath
if ($BackupImagePath -eq $ExportImagePath) {
    throw 'Select two different files: the NEW export and the ORIGINAL backup.'
}
Write-Host "Selected ORIGINAL backup: $BackupImagePath"
Write-Host "Original backup file bytes: $((Get-Item -LiteralPath $BackupImagePath).Length)"
$expectedBackupHash = '6fa3cc6ca8ce04684c74d66897a9345e9e5333e101553eea84ec157fa1d9679a'
$backupHash = (Get-FileHash -LiteralPath $BackupImagePath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "Original backup file SHA256: $backupHash"
$probe = [System.IO.File]::OpenRead($BackupImagePath)
try {
    $backupIsGzip = ($probe.ReadByte() -eq 0x1f -and $probe.ReadByte() -eq 0x8b)
} finally {
    $probe.Dispose()
}
$backup = Read-AuditedBackup $BackupImagePath $backupIsGzip
$original = $backup.Prefix
$readback = Read-ImagePrefix $ExportImagePath $false
if ([BitConverter]::ToUInt16($original, 1080) -ne 0xef53 -or
    [BitConverter]::ToUInt16($readback, 1080) -ne 0xef53) {
    throw 'Missing ext2 superblock signature. The start-sector input or selected file may be wrong.'
}
$uuidMatches = [BitConverter]::ToString($original, 1128, 16) -eq [BitConverter]::ToString($readback, 1128, 16)
if (-not $uuidMatches) { throw 'Filesystem UUID differs from the original boot backup.' }
foreach ($offset in @(1028, 1044, 1048, 1056, 1064)) {
    if ([BitConverter]::ToUInt32($original, $offset) -ne [BitConverter]::ToUInt32($readback, $offset)) {
        throw "Filesystem geometry differs from the original boot backup at offset $offset."
    }
}
$logBlockSize = [BitConverter]::ToUInt32($readback, 1048)
if ($logBlockSize -gt 6) { throw 'Unexpected ext2 block size.' }
$blockSize = [uint64]1024 -shl $logBlockSize
$filesystemBytes = [uint64][BitConverter]::ToUInt32($readback, 1028) * $blockSize
if ($filesystemBytes -eq 0 -or $filesystemBytes -gt $size) {
    throw 'Filesystem size does not fit the boot partition.'
}
$readbackHash = (Get-FileHash -LiteralPath $ExportImagePath -Algorithm SHA256).Hash.ToLowerInvariant()
$report = [ordered]@{
    scope = 'Read-only Windows-file verification; no partition write or restore performed.'
    export_bytes = $size
    expected_start_sector = 32768
    expected_sector_count = 262144
    sector_bytes = 512
    original_backup_encoding = $(if ($backupIsGzip) { 'gzip' } else { 'raw' })
    original_backup_file_sha256 = $backupHash
    original_backup_raw_sha256 = $backup.RawSha256
    original_gzip_hash_matches_record = ($backupIsGzip -and $backupHash -eq $expectedBackupHash)
    ext2_magic_ok = $true
    filesystem_uuid_matches_backup = $uuidMatches
    filesystem_geometry_matches_backup = $true
    filesystem_bytes = $filesystemBytes
    readback_sha256 = $readbackHash
    raw_hash_matches_original_backup = ($readbackHash -eq '0b4b656ab67b9365fe7b5e3de90940f45f8e95c293ad26823815a09a4c133444')
}
$json = $report | ConvertTo-Json
$reportName = 'boot-readback-check-' + (Get-Date -Format 'yyyyMMdd-HHmmssfff') + '.json'
$reportPath = Join-Path ([System.IO.Path]::GetDirectoryName($ExportImagePath)) $reportName
$json | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Output $json
Write-Output 'PASS: 128 MiB export has the same boot filesystem identity and geometry as the verified backup.'
Write-Output 'Raw hashes can differ after reboot because filesystem metadata may change; this check does not compare all filesystem contents.'
Write-Output "Report saved: $reportPath"
