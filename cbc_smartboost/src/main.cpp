/*
 * CBC SmartBoost v1.0 - Main WASM Module
 * CPU Bottleneck Crusher für MSFS 2024
 * 
 * Ziel: Adaptive Performance-Optimierung für GTX 1080 Ti + Ryzen 5 2600
 */

#include <stdint.h>
#include <string.h>
#include "cbc_simconnect.h"

// Version
static const char* VERSION = "1.0.0";

// Current Stats
static CBCStats currentStats = {0};
static CBCProfile currentProfile = PROFILE_AUTO;

// Configuration Thresholds
#define FPS_TARGET 58.0f
#define CPU_THRESHOLD 0.85f
#define GPU_THRESHOLD 0.90f
#define TRAFFIC_MIN 0.3f
#define TRAFFIC_MAX 0.8f
#define LOD_BIAS_MIN -0.3f
#define LOD_BIAS_MAX 0.1f
#define CACHE_INTERVAL_SEC 1800  // 30 Minuten

// Timing
static uint32_t lastCacheCheck = 0;
static uint32_t startTime = 0;

// Externe SimConnect-Funktionen (werden von MSFS bereitgestellt)
// In echter Implementierung: über SimConnect API aufrufen
extern float simvar_get_fps();
extern float simvar_get_main_thread_frametime();
extern float simvar_get_render_thread_frametime();
extern float simvar_get_cpu_usage();
extern float simvar_get_gpu_usage();
extern float simvar_get_altitude();
extern float simvar_get_airspeed();
extern void simvar_set_traffic_density(float density);
extern void simvar_set_lod_bias(const char* category, float bias);

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

void cbc_init() {
    startTime = 0;  // In echter Implementierung: Zeitstempel setzen
    memset(&currentStats, 0, sizeof(CBCStats));
    currentProfile = PROFILE_AUTO;
    
    // Init-Log (in echter Implementierung: Debug-Output)
    // log_info("[CBC] SmartBoost v%s initialized", VERSION);
}

void cbc_update(CBCStats* stats) {
    if (!stats) return;
    
    // Stats kopieren
    memcpy(&currentStats, stats, sizeof(CBCStats));
    
    // Bottleneck-Analyse
    if (cbc_isCPUBottleneck(stats)) {
        // CPU-Bottleneck: Traffic reduzieren, LOD anpassen
        float trafficDensity = cbc_getTrafficDensity(stats);
        simvar_set_traffic_density(trafficDensity);
        
        float lodBias = cbc_getLODBias(stats, "OBJECTS");
        simvar_set_lod_bias("OBJECTS", lodBias);
        
        lodBias = cbc_getLODBias(stats, "TREES");
        simvar_set_lod_bias("TREES", lodBias);
    } else if (cbc_isGPUBottleneck(stats)) {
        // GPU-Bottleneck: Weniger aggressiv optimieren (1080 Ti hat genug VRAM)
        float lodBias = cbc_getLODBias(stats, "TERRAIN");
        simvar_set_lod_bias("TERRAIN", lodBias);
    }
    
    // Cache-Management
    if (cbc_shouldClearCache(stats)) {
        // In echter Implementierung: Cache-Cleanup triggern
        // log_info("[CBC] Cache cleanup triggered");
        lastCacheCheck = 0;  // Reset timer
    }
}

CBCStats cbc_getStats() {
    return currentStats;
}

void cbc_setProfile(CBCProfile profile) {
    currentProfile = profile;
    
    // Profile-spezifische Anpassungen
    switch (profile) {
        case PROFILE_PERFORMANCE:
            // Aggressive Optimierung
            simvar_set_traffic_density(TRAFFIC_MIN);
            simvar_set_lod_bias("OBJECTS", LOD_BIAS_MIN);
            break;
        case PROFILE_QUALITY:
            // Maximale Qualität
            simvar_set_traffic_density(TRAFFIC_MAX);
            simvar_set_lod_bias("OBJECTS", LOD_BIAS_MAX);
            break;
        case PROFILE_CUSTOM:
            // Benutzerdefinierte Einstellungen (wird extern konfiguriert)
            break;
        case PROFILE_AUTO:
        default:
            // Automatische Anpassung (Standard)
            break;
    }
}

const char* cbc_getVersion() {
    return VERSION;
}

// ============================================================================
// BOTTLENECK DETECTION
// ============================================================================

int cbc_isCPUBottleneck(const CBCStats* stats) {
    if (!stats) return 0;
    
    // CPU-Bottleneck wenn:
    // 1. Main Thread Frametime > Render Thread Frametime
    // 2. FPS unter Zielwert
    // 3. CPU-Auslastung hoch
    
    if (stats->fps < FPS_TARGET && 
        stats->mainThreadFrametime > stats->renderThreadFrametime &&
        stats->cpuUsage > CPU_THRESHOLD) {
        return 1;
    }
    
    // Profil-basierte Override
    if (currentProfile == PROFILE_PERFORMANCE) {
        return stats->fps < FPS_TARGET;
    }
    
    return 0;
}

int cbc_isGPUBottleneck(const CBCStats* stats) {
    if (!stats) return 0;
    
    // GPU-Bottleneck wenn:
    // 1. Render Thread Frametime > Main Thread Frametime
    // 2. FPS unter Zielwert
    // 3. GPU-Auslastung hoch
    
    if (stats->fps < FPS_TARGET && 
        stats->renderThreadFrametime > stats->mainThreadFrametime &&
        stats->gpuUsage > GPU_THRESHOLD) {
        return 1;
    }
    
    return 0;
}

// ============================================================================
// DYNAMIC ADJUSTMENTS
// ============================================================================

float cbc_getTrafficDensity(const CBCStats* stats) {
    if (!stats) return TRAFFIC_MAX;
    
    // Dynamische Berechnung basierend auf CPU-Last
    float cpuLoad = stats->cpuUsage;
    
    if (cpuLoad > 0.95f) {
        return TRAFFIC_MIN;  // Sehr aggressive Reduktion
    } else if (cpuLoad > 0.90f) {
        return TRAFFIC_MIN + 0.1f;
    } else if (cpuLoad > 0.85f) {
        return TRAFFIC_MIN + 0.2f;
    } else if (cpuLoad > 0.75f) {
        return TRAFFIC_MIN + 0.3f;
    } else {
        return TRAFFIC_MAX;  // Normale Dichte
    }
}

float cbc_getLODBias(const CBCStats* stats, const char* category) {
    if (!stats || !category) return 0.0f;
    
    float bias = 0.0f;
    float cpuLoad = stats->cpuUsage;
    float speed = stats->airspeed;  // Knoten
    
    // Kategorie-spezifische Logik
    if (strcmp(category, "OBJECTS") == 0) {
        // Objekte: Bei hoher Geschwindigkeit weniger Details
        if (speed > 200.0f) {
            bias = -0.2f;  // Schnellflug: reduzierte Details
        } else if (cpuLoad > 0.90f) {
            bias = -0.25f;
        } else if (cpuLoad > 0.85f) {
            bias = -0.15f;
        } else {
            bias = 0.05f;  // Leichter Bonus bei guter Performance
        }
    } else if (strcmp(category, "TREES") == 0) {
        // Bäume: Immer konservativ (großer CPU-Killer)
        if (cpuLoad > 0.85f) {
            bias = -0.3f;
        } else if (cpuLoad > 0.75f) {
            bias = -0.15f;
        } else {
            bias = 0.0f;
        }
    } else if (strcmp(category, "TERRAIN") == 0) {
        // Terrain: Bei Reiseflug mehr Vorab-Laden
        if (stats->altitude > 30000.0f && speed > 400.0f) {
            bias = 0.1f;  // Mehr Details im Voraus laden
        } else if (cpuLoad > 0.90f) {
            bias = -0.1f;
        } else {
            bias = 0.0f;
        }
    }
    
    // Clamp Werte
    if (bias < LOD_BIAS_MIN) bias = LOD_BIAS_MIN;
    if (bias > LOD_BIAS_MAX) bias = LOD_BIAS_MAX;
    
    return bias;
}

int cbc_shouldClearCache(const CBCStats* stats) {
    // In echter Implementierung: Zeitbasierte Prüfung
    // Hier: Simuliert alle 30 Minuten
    
    // Einfache Heuristik: Wenn VRAM fast voll (>10.2 GB von 11 GB)
    // In echter Implementierung: VRAM-Wert aus SimConnect holen
    if (stats && stats->gpuUsage > 0.95f) {
        return 1;
    }
    
    return 0;
}

// ============================================================================
// ENTRY POINT (für WASM)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

int main() {
    cbc_init();
    return 0;
}

// Update-Funktion wird von MSFS aufgerufen
void update() {
    // In echter Implementierung: Stats von SimConnect holen
    CBCStats stats;
    stats.fps = simvar_get_fps();
    stats.mainThreadFrametime = simvar_get_main_thread_frametime();
    stats.renderThreadFrametime = simvar_get_render_thread_frametime();
    stats.cpuUsage = simvar_get_cpu_usage();
    stats.gpuUsage = simvar_get_gpu_usage();
    stats.altitude = simvar_get_altitude();
    stats.airspeed = simvar_get_airspeed();
    
    cbc_update(&stats);
}

#ifdef __cplusplus
}
#endif
