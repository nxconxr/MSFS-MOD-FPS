# CBC SmartBoost v2.0 - Hardware Auto-Tuner
# PowerShell Script für sichere Windows-Optimierungen
# ALS ADMINISTRATOR AUSFÜHREN!

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "CBC SmartBoost v2.0 - Hardware Tuner" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Prüfen ob als Admin ausgeführt
$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "FEHLER: Bitte als Administrator ausführen!" -ForegroundColor Red
    Write-Host "Rechtsklick auf das Script -> 'Als Administrator ausführen'" -ForegroundColor Yellow
    exit 1
}

Write-Host "[1/5] HAGS (Hardware-accelerated GPU Scheduling) aktivieren..." -ForegroundColor Yellow
try {
    Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" `
                     -Name "HwSchMode" -Value 2 -Force
    Write-Host "  ✓ HAGS aktiviert" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Fehler bei HAGS: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "[2/5] HPET (High Precision Event Timer) deaktivieren..." -ForegroundColor Yellow
try {
    bcdedit /set useplatformclock false | Out-Null
    Write-Host "  ✓ HPET deaktiviert (benötigt Neustart)" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Fehler bei HPET: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "[3/5] Ultimate Performance Power Plan aktivieren..." -ForegroundColor Yellow
try {
    # GUID des Ultimate Performance Plans
    $guid = "e9a42b02-d5df-448d-aa00-03f14749eb61"
    powercfg -setactive $guid | Out-Null
    
    # Falls nicht verfügbar, erstellen
    if ($LASTEXITCODE -ne 0) {
        powercfg -duplicatescheme $guid | Out-Null
        powercfg -setactive $guid | Out-Null
    }
    Write-Host "  ✓ Ultimate Performance Plan aktiviert" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Fehler bei Power Plan: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "[4/5] Pagefile optimieren (für NVMe SSD)..." -ForegroundColor Yellow
try {
    # Aktuelles Laufwerk prüfen wo Windows installiert ist
    $systemDrive = $env:SystemDrive.Substring(0,1)
    
    # Pagefile auf 16GB fixieren (empfohlen für MSFS mit 16GB+ RAM)
    wmic pagefileset where name="${systemDrive}\\pagefile.sys" set InitialSize=16384,MaximumSize=16384 | Out-Null
    Write-Host "  ✓ Pagefile auf 16GB fixiert (${systemDrive}:)" -ForegroundColor Green
    Write-Host "    Hinweis: Bei 32GB+ RAM kann dies auf 8GB reduziert werden" -ForegroundColor Gray
} catch {
    Write-Host "  ✗ Fehler bei Pagefile: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "[5/5] Startup-Programme bereinigen (Empfehlung)..." -ForegroundColor Yellow
Write-Host "  ℹ Manuelles Prüfen empfohlen:" -ForegroundColor Cyan
Write-Host "    Task-Manager -> Startup -> Nicht benötigte Apps deaktivieren" -ForegroundColor Gray
Write-Host "    Besonders: Browser, Chat-Clients, Cloud-Sync (außer OneDrive)" -ForegroundColor Gray

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "OPTIMIERUNG ABGESCHLOSSEN!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "WICHTIG: Bitte starte den JETZT neu, damit alle Änderungen wirksam werden." -ForegroundColor Yellow
Write-Host ""
Write-Host "Nach dem Neustart:" -ForegroundColor Cyan
Write-Host "  1. CBC SmartBoost Companion starten (pythonw predictive_loader.py)" -ForegroundColor White
Write-Host "  2. MSFS 2024 im Developer Mode testen" -ForegroundColor White
Write-Host "  3. Frametimes mit RTSS oder CapFrameX überwachen" -ForegroundColor White
Write-Host ""

# Optional: Neustart anbieten
$restart = Read-Host "Jetzt neu starten? (J/N)"
if ($restart -eq "J" -or $restart -eq "j") {
    Write-Host "System wird in 5 Sekunden neu gestartet..." -ForegroundColor Yellow
    Start-Sleep -Seconds 5
    Restart-Computer -Force
}
