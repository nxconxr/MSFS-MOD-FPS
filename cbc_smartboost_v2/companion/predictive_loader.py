import os
import sys
import time
import shutil
import xml.etree.ElementTree as ET
import json
from pathlib import Path

# --- KONFIG ---
LOG_DIR = Path.home() / "AppData" / "Local" / "CBC_Logs"
FLAG_FILE = LOG_DIR / "cbc_trigger_reload.flag"
PREDICT_FLAG = LOG_DIR / "cbc_predictive_switch.flag"
BACKUP_DIR = LOG_DIR / "backups"
PROFILE_DB = Path("profiles/scene_profiles.json")

# MSFS 2024 Pfade (Store & Steam)
OPTIONS_PATHS = [
    Path.home() / "AppData" / "Local" / "Packages" / "Microsoft.FlightSimulator_8wekyb3d8bbwe" / "LocalState" / "options.xml",
    Path.home() / "AppData" / "Roaming" / "Microsoft Flight Simulator 2024" / "options.xml"
]

def log(msg):
    print(f"[CBC-Predictive] {msg}")

def find_options_xml():
    for path in OPTIONS_PATHS:
        if path.exists():
            return path
    return None

def backup_xml(options_path):
    BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = time.strftime("%Y%m%d-%H%M%S")
    backup_path = BACKUP_DIR / f"options_{timestamp}.xml"
    try:
        shutil.copy2(options_path, backup_path)
        log(f"Backup erstellt: {backup_path.name}")
        return True
    except Exception as e:
        log(f"Backup fehlgeschlagen: {e}")
        return False

def load_profile(scene_key):
    """Lädt ein Profil basierend auf der Szene"""
    if not PROFILE_DB.exists():
        log("Keine Profile gefunden, nutze Defaults")
        return None
    
    try:
        with open(PROFILE_DB, 'r') as f:
            db = json.load(f)
        profile = db.get(scene_key)
        if profile:
            log(f"Profil geladen: {scene_key}")
            return profile
        else:
            log(f"Kein Profil für '{scene_key}', nutze Default")
            return None
    except Exception as e:
        log(f"Fehler beim Laden der Profile: {e}")
        return None

def patch_xml(profile_name: str):
    """Patcht options.xml mit dem passenden Profil"""
    options_path = find_options_xml()
    if not options_path:
        log("FEHLER: options.xml nicht gefunden!")
        return False

    # Profil laden
    profile = load_profile(profile_name)
    
    # Fallback Defaults wenn kein Profil existiert
    if profile is None:
        if profile_name == "AIRPORT_APPROACH":
            changes = {
                ".//TrafficDensity": "0.30",
                ".//LOD_Bias": "-2",
                ".//ShadowMapResolution": "1024",
                ".//TerrainLOD": "75",
                ".//ObjectLOD": "75",
                ".//CloudQuality": "2",
                ".//TextureResolution": "2048"
            }
        elif profile_name == "CRUISE":
            changes = {
                ".//TrafficDensity": "0.60",
                ".//LOD_Bias": "0",
                ".//ShadowMapResolution": "2048",
                ".//TerrainLOD": "150",
                ".//ObjectLOD": "100",
                ".//CloudQuality": "3",
                ".//TextureResolution": "4096"
            }
        else:
            changes = {} # Unbekannt, keine Änderung
    else:
        # Profil aus JSON verwenden
        changes = {}
        mapping = {
            "traffic": (".//TrafficDensity", str(profile.get('traffic', 0.5))),
            "lod_bias": (".//LOD_Bias", str(profile.get('lod_bias', 0))),
            "shadows": (".//ShadowMapResolution", str(profile.get('shadows', 2048))),
            "terrain": (".//TerrainLOD", str(profile.get('terrain', 100))),
            "objects": (".//ObjectLOD", str(profile.get('objects', 100))),
            "clouds": (".//CloudQuality", str(profile.get('clouds', 3))),
            "textures": (".//TextureResolution", str(profile.get('textures', 4096)))
        }
        for key, (xpath, value) in mapping.items():
            if key in profile:
                changes[xpath] = value

    if not changes:
        log("Keine Änderungen notwendig")
        return False

    try:
        tree = ET.parse(options_path)
        root = tree.getroot()
        changed = False
        
        for xpath, value in changes.items():
            elem = root.find(xpath)
            if elem is not None and elem.text != value:
                old_val = elem.text
                elem.text = value
                changed = True
                log(f"{xpath}: {old_val} -> {value}")
        
        if changed:
            # Backup vor Änderung
            backup_xml(options_path)
            
            # Atomar schreiben
            tmp_file = options_path.with_suffix(".tmp")
            tree.write(tmp_file, encoding="utf-8", xml_declaration=True)
            os.replace(tmp_file, options_path)
            log(f"XML erfolgreich gepatcht für: {profile_name}")
            return True
        else:
            log("XML bereits aktuell")
            return False
            
    except Exception as e:
        log(f"Fehler beim Patchen: {e}")
        # Rollback versuchen
        rollback_backup()
        return False

def rollback_backup():
    """Stellt das letzte Backup wieder her"""
    backups = sorted(BACKUP_DIR.glob("options_*.xml"), reverse=True)
    if backups:
        latest = backups[0]
        options_path = find_options_xml()
        if options_path:
            shutil.copy2(latest, options_path)
            log(f"Rollback durchgeführt von {latest.name}")

def main():
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    log("Predictive Companion gestartet. Warte auf Flags...")
    
    last_predict_scene = None
    
    while True:
        try:
            # 1. Check Predictive Flag (Priorität 1)
            if PREDICT_FLAG.exists():
                scene = PREDICT_FLAG.read_text().strip()
                if scene != last_predict_scene:
                    log(f"Predictive Switch erkannt: {scene}")
                    if patch_xml(scene):
                        log(">>> PATCH ANGEWENDET <<<")
                    last_predict_scene = scene
                PREDICT_FLAG.unlink()
            
            # 2. Check Reactive Flag (Fallback)
            elif FLAG_FILE.exists():
                mode = FLAG_FILE.read_text().strip()
                log(f"Reactive Flag erkannt: {mode}")
                
                scene_map = {
                    "PERFORMANCE": "AIRPORT_APPROACH",
                    "QUALITY": "CRUISE"
                }
                target_scene = scene_map.get(mode, "CRUISE")
                
                if patch_xml(target_scene):
                    log(">>> PATCH ANGEWENDET <<<")
                
                FLAG_FILE.unlink()
            
            time.sleep(1) # Schnelleres Polling für Predictive

        except Exception as e:
            log(f"Error im Hauptloop: {e}")
            time.sleep(5)

if __name__ == "__main__":
    # Wichtig: Als pythonw.exe starten um kein Fenster zu zeigen
    main()
