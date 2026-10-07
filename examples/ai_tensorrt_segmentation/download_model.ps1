[CmdletBinding()]
param(
    [string]$OutputPath = "models/fcn-resnet50-12.onnx",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
# The GitHub ONNX Model Zoo is the source project; its large Git-LFS payloads
# are now mirrored by the ONNX Model Zoo account on Hugging Face.
$url = "https://huggingface.co/onnxmodelzoo/fcn-resnet50-12/resolve/main/fcn-resnet50-12.onnx?download=true"
$output = [System.IO.Path]::GetFullPath($OutputPath)

if ((Test-Path -LiteralPath $output) -and -not $Force) {
    throw "File already exists: $output (use -Force only after checking its provenance)"
}
$parent = Split-Path -Parent $output
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }

Write-Host "Downloading FCN ResNet-50 ONNX from the ONNX Model Zoo..."
Invoke-WebRequest -Uri $url -OutFile $output
$size = (Get-Item -LiteralPath $output).Length
if ($size -lt 1MB) {
    Remove-Item -LiteralPath $output -Force
    throw "Downloaded file is unexpectedly small; the server may have returned an error page"
}
Write-Host "Saved $output ($size bytes). Pin and verify a SHA-256 digest before production use."
