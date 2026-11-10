param(
 [Parameter(Mandatory=$true)][ValidateRange(1,65535)][int]$ProcessId,
 [Parameter(Mandatory=$true)][ValidateSet('W','A','S','D')][string]$Key,
 [ValidateRange(100,15000)][int]$DurationMs=2000
)
$ErrorActionPreference='Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class UnderdogWinGameKeys {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
}
"@
$p=Get-Process -Id $ProcessId -ErrorAction Stop
if($p.ProcessName -ne 'veloren-voxygen' -or $p.MainWindowHandle -eq 0){throw 'Expected Veloren interactive process missing'}
$codes=@{W=0x57;A=0x41;S=0x53;D=0x44}
$vk=[byte]$codes[$Key]
[void][UnderdogWinGameKeys]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep -Milliseconds 300
[UnderdogWinGameKeys]::keybd_event($vk,0,0,[UIntPtr]::Zero)
try {Start-Sleep -Milliseconds $DurationMs}
finally {[UnderdogWinGameKeys]::keybd_event($vk,0,2,[UIntPtr]::Zero)}
Write-Output ('KEY_HOLD='+$Key+' DURATION_MS='+$DurationMs+' PROCESS_ID='+$ProcessId)
