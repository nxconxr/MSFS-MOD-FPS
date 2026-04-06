#include <SimConnect.h>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// --- KONFIGURATION ---
const float FPS_TARGET = 55.0f;
const float CPU_THRESHOLD_MS = 18.0f; 
const int HYSTERESIS_SECONDS = 10;
const char* FLAG_DIR = "C:/Users/%USERNAME%/AppData/Local/CBC_Logs/";
const char* FLAG_FILE = "cbc_trigger_reload.flag";
const char* PREDICT_FLAG = "cbc_predictive_switch.flag";

// --- KALMAN FILTER FÜR FRAMETIME ---
struct KalmanFilter {
    float P = 0.1;  // Prozessunsicherheit
    float K = 0.5;  // Kalman Gain
    float x = 16.0f; // Geschätzter Wert (Frametime)
    float R = 0.05;  // Messrauschen
    
    float update(float measurement) {
        P = P + R;
        K = P / (P + 0.1f);
        x = x + K * (measurement - x);
        P = (1 - K) * P;
        return x;
    }
    
    void reset() {
        P = 0.1; K = 0.5; x = 16.0f; R = 0.05;
    }
};

// --- STATE ---
struct SimData {
    double frameRate;
    double mainThreadFPS;
    double groundSpeed; // Knoten
    double latitude;
    double longitude;
    double altitude;
};

static HANDLE hSimConnect = nullptr;
static bool isPerformanceMode = false;
static bool isPredictiveMode = false;
static std::chrono::steady_clock::time_point lastModeSwitchTime;
static KalmanFilter kalmanFt;
static KalmanFilter kalmanCpu;

// --- HILFSFUNKTIONEN ---

// Entfernung berechnen (Haversine Formel)
double calcDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 3440.0; // Erdradius in Nautischen Meilen
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dLat/2) * sin(dLat/2) + 
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) * 
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}

// Flag schreiben
void WriteFlag(const char* mode) {
    char path[MAX_PATH];
    ExpandEnvironmentStringsA(FLAG_DIR, path, MAX_PATH);
    
    fs::path p(path);
    fs::create_directories(p);
    
    std::string fullPath = std::string(path) + FLAG_FILE;
    FILE* f = fopen(fullPath.c_str(), "w");
    if (f) {
        fprintf(f, "%s\n", mode);
        fclose(f);
        printf("[CBC] Flag geschrieben: %s\n", mode);
    }
}

// Prädiktive Flag schreiben
void WritePredictFlag(const char* scene) {
    char path[MAX_PATH];
    ExpandEnvironmentStringsA(FLAG_DIR, path, MAX_PATH);
    fs::create_directories(path);
    
    std::string fullPath = std::string(path) + PREDICT_FLAG;
    FILE* f = fopen(fullPath.c_str(), "w");
    if (f) {
        fprintf(f, "%s\n", scene);
        fclose(f);
        printf("[CBC] Predictive Flag: %s\n", scene);
    }
}

// Bekannte Hotspots (Beispiel: EDDF, KJFK, EGLL)
struct Hotspot {
    const char* name;
    double lat;
    double lon;
    double triggerDistNm; // Distanz für Pre-Scaling
};

Hotspot knownHotspots[] = {
    {"EDDF", 50.0379, 8.5622, 25.0},   // Frankfurt
    {"KJFK", 40.6413, -73.7781, 30.0}, // New York JFK
    {"EGLL", 51.4700, -0.4543, 25.0},  // London Heathrow
    {"KSFO", 37.6213, -122.3790, 25.0},// San Francisco
    {"RJTT", 35.5494, 139.7798, 25.0}  // Tokyo Haneda
};

// --- SIMCONNECT CALLBACK ---
void CALLBACK DispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
    switch (pData->dwID) {
        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
            SIMCONNECT_RECV_SIMOBJECT_DATA* pObj = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;
            if (pObj->dwRequestID == 1) {
                SimData* data = (SimData*)&pObj->dwData;
                
                float currentFt = 1000.0f / static_cast<float>(data->frameRate);
                float currentCpuMs = 1000.0f / static_cast<float>(data->mainThreadFPS);
                
                // Kalman Filter anwenden
                float smoothFt = kalmanFt.update(currentFt);
                float smoothCpu = kalmanCpu.update(currentCpuMs);
                
                auto now = std::chrono::steady_clock::now();
                auto timeSinceSwitch = std::chrono::duration_cast<std::chrono::seconds>(now - lastModeSwitchTime).count();

                // --- PRÄDIKTIVE LOGIK ---
                bool nearHotspot = false;
                for (const auto& hs : knownHotspots) {
                    double dist = calcDistance(data->latitude, data->longitude, hs.lat, hs.lon);
                    if (dist < hs.triggerDistNm) {
                        nearHotspot = true;
                        // Wenn noch nicht im Perf-Mode und nah genug -> voraktivieren
                        if (!isPerformanceMode && !isPredictiveMode && dist < (hs.triggerDistNm * 0.6)) {
                            if (timeSinceSwitch > 5) { // Kürzere Hysterese für Predictive
                                isPredictiveMode = true;
                                WritePredictFlag("AIRPORT_APPROACH");
                                printf("[CBC] Predictive: Approaching %s (%.1f NM)\n", hs.name, dist);
                            }
                        }
                        break;
                    }
                }
                
                // Reset Predictive wenn weit weg
                if (!nearHotspot && isPredictiveMode && timeSinceSwitch > 10) {
                    isPredictiveMode = false;
                    WritePredictFlag("CRUISE");
                }

                // --- REAKTIVE LOGIK (Fallback) ---
                bool isBottleneck = (smoothFt > (1000.0f/FPS_TARGET)) || (smoothCpu > CPU_THRESHOLD_MS);
                
                if (!isPerformanceMode && !isPredictiveMode && isBottleneck) {
                    if (timeSinceSwitch > HYSTERESIS_SECONDS) {
                        isPerformanceMode = true;
                        lastModeSwitchTime = now;
                        WriteFlag("PERFORMANCE");
                    }
                }
                else if (isPerformanceMode && !isPredictiveMode && 
                         (smoothFt < (1000.0f/(FPS_TARGET+5))) && (smoothCpu < (CPU_THRESHOLD_MS - 2))) {
                    if (timeSinceSwitch > HYSTERESIS_SECONDS) {
                        isPerformanceMode = false;
                        lastModeSwitchTime = now;
                        WriteFlag("QUALITY");
                    }
                }
            }
            break;
        }
    }
}

// --- INIT & LOOP ---
extern "C" __declspec(dllexport) void MSFS_Init() {
    HRESULT hr = SimConnect_Open(&hSimConnect, "CBC_SmartBoost_v2", nullptr, 0, nullptr, 0);
    if (SUCCEEDED(hr)) {
        // Data Definition
        SimConnect_AddToDataDefinition(hSimConnect, 1, "SIMULATOR FRAME RATE", "number", SIMCONNECT_DATATYPE_FLOAT64);
        SimConnect_AddToDataDefinition(hSimConnect, 1, "SIMULATOR MAIN THREAD FPS", "number", SIMCONNECT_DATATYPE_FLOAT64);
        SimConnect_AddToDataDefinition(hSimConnect, 1, "GROUND SPEED", "knots", SIMCONNECT_DATATYPE_FLOAT64);
        SimConnect_AddToDataDefinition(hSimConnect, 1, "PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64);
        SimConnect_AddToDataDefinition(hSimConnect, 1, "PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64);
        SimConnect_AddToDataDefinition(hSimConnect, 1, "INDICATED ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64);
        
        // Request Data jede Sekunde
        SimConnect_RequestDataOnSimObject(hSimConnect, 1, 1, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND, 0, 0, 0);
        
        lastModeSwitchTime = std::chrono::steady_clock::now();
        printf("[CBC v2] Initialisiert mit Kalman Filter + Predictive Logic\n");
    } else {
        printf("[CBC] SimConnect Open failed!\n");
    }
}

extern "C" __declspec(dllexport) void MSFS_Update() {
    if (hSimConnect) {
        SimConnect_CallDispatch(hSimConnect, DispatchProc, nullptr);
    }
}

extern "C" __declspec(dllexport) void MSFS_Deinit() {
    if (hSimConnect) SimConnect_Close(hSimConnect);
}
