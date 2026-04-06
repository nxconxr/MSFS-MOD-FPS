/*
 * CBC SmartBoost v1.0 - SimConnect Header Definitions
 * Minimalistische Definitionen für MSFS 2024 WASM Module
 */

#ifndef CBC_SIMCONNECT_H
#define CBC_SIMCONNECT_H

#ifdef __cplusplus
extern "C" {
#endif

// SimConnect Data Structures
typedef struct {
    float fps;
    float mainThreadFrametime;
    float renderThreadFrametime;
    float cpuUsage;
    float gpuUsage;
    float altitude;
    float airspeed;
    float latitude;
    float longitude;
} CBCStats;

typedef enum {
    PROFILE_AUTO = 0,
    PROFILE_PERFORMANCE = 1,
    PROFILE_QUALITY = 2,
    PROFILE_CUSTOM = 3
} CBCProfile;

// Core Functions (werden von main.cpp implementiert)
void cbc_init();
void cbc_update(CBCStats* stats);
CBCStats cbc_getStats();
void cbc_setProfile(CBCProfile profile);
const char* cbc_getVersion();

// Bottleneck Detection
int cbc_isCPUBottleneck(const CBCStats* stats);
int cbc_isGPUBottleneck(const CBCStats* stats);

// Dynamic Adjustments
float cbc_getTrafficDensity(const CBCStats* stats);
float cbc_getLODBias(const CBCStats* stats, const char* category);
int cbc_shouldClearCache(const CBCStats* stats);

#ifdef __cplusplus
}
#endif

#endif // CBC_SIMCONNECT_H
