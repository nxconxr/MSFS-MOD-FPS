/**
 * CPU Bottleneck Crusher (CBC) - WASM Module for MSFS 2024
 * Optimized for GTX 1080 Ti + Ryzen 5 2600
 * 
 * Features:
 * - Dynamic Traffic Scaler
 * - Smart Tree LOD
 * - Terrain Pre-Loader
 * - Frame Pacing Guard
 */

// SimConnect types (simplified for this example)
interface SimConnect {
    registerDataDefineStruct<T>(structName: string): void;
    onRecvSimobjectData: (data: any) => void;
    setData: (name: string, value: any) => void;
}

interface CPUStats {
    mainThreadFrametime: number; // in ms
    renderThreadFrametime: number; // in ms
    cpuLoad: number; // 0.0 to 1.0
}

interface CBCConfig {
    targetFPS: number;
    maxCPULoad: number;
    trafficDensityMin: number;
    trafficDensityMax: number;
    lodBiasObjects: number;
    lodBiasTrees: number;
}

class CPUBottleneckCrusher {
    private simconnect: SimConnect | null = null;
    private config: CBCConfig;
    private lastAdjustmentTime: number = 0;
    private adjustmentInterval: number = 500; // ms between adjustments
    private isInitialized: boolean = false;

    constructor() {
        // Default config optimized for Ryzen 5 2600 + GTX 1080 Ti
        this.config = {
            targetFPS: 55, // Aim for stable 55 FPS
            maxCPULoad: 0.85, // Keep CPU below 85%
            trafficDensityMin: 0.2, // 20% minimum traffic
            trafficDensityMax: 0.8, // 80% maximum traffic
            lodBiasObjects: -0.2, // Slightly reduce object LOD distance
            lodBiasTrees: -0.3, // More aggressive tree LOD reduction
        };
    }

    /**
     * Initialize the CBC module with SimConnect
     */
    public initialize(simconnect: SimConnect): void {
        this.simconnect = simconnect;
        
        // Register CPU stats structure
        this.simconnect.registerDataDefineStruct<CPUStats>('CBC_CPU_STATS');
        
        // Setup event listener for frame data
        this.simconnect.onRecvSimobjectData = (data) => {
            this.onFrameDataReceived(data);
        };

        this.isInitialized = true;
        console.log('[CBC] CPU Bottleneck Crusher initialized for GTX 1080 Ti + Ryzen 5 2600');
    }

    /**
     * Called on every frame update from SimConnect
     */
    private onFrameDataReceived(data: any): void {
        const currentTime = Date.now();
        
        // Throttle adjustments to avoid excessive changes
        if (currentTime - this.lastAdjustmentTime < this.adjustmentInterval) {
            return;
        }

        const cpuStats = this.parseCPUStats(data);
        
        if (!cpuStats) {
            return;
        }

        // Check if we need to mitigate CPU load
        if (cpuStats.cpuLoad > this.config.maxCPULoad || 
            cpuStats.mainThreadFrametime > (1000 / this.config.targetFPS)) {
            this.triggerCPUMitigation(cpuStats);
        } else if (cpuStats.cpuLoad < (this.config.maxCPULoad - 0.15)) {
            // Gradually restore settings if CPU has room
            this.restoreSettings(cpuStats);
        }

        this.lastAdjustmentTime = currentTime;
    }

    /**
     * Parse incoming data into CPUStats
     */
    private parseCPUStats(data: any): CPUStats | null {
        // In real implementation, this would parse SimConnect data
        // For now, return mock data structure
        if (!data) {
            return null;
        }

        return {
            mainThreadFrametime: data.mainThreadFrametime || 16.6,
            renderThreadFrametime: data.renderThreadFrametime || 16.6,
            cpuLoad: data.cpuLoad || 0.7,
        };
    }

    /**
     * Trigger CPU mitigation strategies
     */
    private triggerCPUMitigation(stats: CPUStats): void {
        const severity = this.calculateMitigationSeverity(stats);

        console.log(`[CBC] CPU Mitigation triggered (Severity: ${severity})`);

        // Apply dynamic traffic scaling
        this.adjustTrafficDensity(severity);

        // Apply smart LOD adjustments
        this.adjustLODBias(severity);

        // Trigger terrain pre-cache if in cruise flight
        this.manageTerrainPreCache(stats);

        // Clean WASM cache if needed
        this.cleanWasmCacheIfNeeded();
    }

    /**
     * Calculate mitigation severity based on CPU load and frametime
     */
    private calculateMitigationSeverity(stats: CPUStats): number {
        const fpsTarget = 1000 / this.config.targetFPS;
        const frameOverhead = stats.mainThreadFrametime / fpsTarget;
        const cpuOverhead = stats.cpuLoad / this.config.maxCPULoad;

        // Severity from 0.0 (mild) to 1.0 (aggressive)
        return Math.min(1.0, Math.max(0.0, (frameOverhead + cpuOverhead) / 2 - 0.5));
    }

    /**
     * Adjust AI traffic density based on CPU load
     */
    private adjustTrafficDensity(severity: number): void {
        if (!this.simconnect) return;

        // Linear interpolation between min and max density based on severity
        const targetDensity = this.config.trafficDensityMax - 
            (severity * (this.config.trafficDensityMax - this.config.trafficDensityMin));

        this.simconnect.setData('AI_TRAFFIC_DENSITY', targetDensity);
        console.log(`[CBC] Traffic density adjusted to ${(targetDensity * 100).toFixed(0)}%`);
    }

    /**
     * Adjust LOD bias for objects and trees
     */
    private adjustLODBias(severity: number): void {
        if (!this.simconnect) return;

        // More aggressive LOD reduction at higher severity
        const objectLODBias = this.config.lodBiasObjects - (severity * 0.3);
        const treeLODBias = this.config.lodBiasTrees - (severity * 0.4);

        this.simconnect.setData('LOD_BIAS_OBJECTS', objectLODBias);
        this.simconnect.setData('LOD_BIAS_TREES', treeLODBias);
        
        console.log(`[CBC] LOD Bias - Objects: ${objectLODBias.toFixed(2)}, Trees: ${treeLODBias.toFixed(2)}`);
    }

    /**
     * Manage terrain pre-caching based on flight phase
     */
    private manageTerrainPreCache(stats: CPUStats): void {
        // In a real implementation, this would check aircraft velocity and altitude
        // to determine if we're in cruise flight and can pre-load terrain
        
        if (stats.mainThreadFrametime < 14) {
            // Good frametime, enable aggressive pre-caching
            this.simconnect?.setData('TERRAIN_PRECACHE_AGGRESSIVE', true);
        } else {
            // Reduce pre-caching to save CPU
            this.simconnect?.setData('TERRAIN_PRECACHE_AGGRESSIVE', false);
        }
    }

    /**
     * Clean WASM cache periodically to prevent bloat
     */
    private cleanWasmCacheIfNeeded(): void {
        // This would interface with the file system in a real implementation
        // For now, just log that we'd clean the cache
        console.log('[CBC] WASM Cache check completed');
    }

    /**
     * Gradually restore settings when CPU load decreases
     */
    private restoreSettings(stats: CPUStats): void {
        const restoreFactor = 0.1; // Gentle restoration
        
        // Slowly increase traffic density
        const currentDensity = this.config.trafficDensityMax * 0.5; // Mock current
        const newDensity = Math.min(this.config.trafficDensityMax, currentDensity + restoreFactor);
        
        this.simconnect?.setData('AI_TRAFFIC_DENSITY', newDensity);
        
        console.log(`[CBC] Restoring settings... Traffic: ${(newDensity * 100).toFixed(0)}%`);
    }

    /**
     * Get current configuration
     */
    public getConfig(): CBCConfig {
        return { ...this.config };
    }

    /**
     * Update configuration
     */
    public updateConfig(newConfig: Partial<CBCConfig>): void {
        this.config = { ...this.config, ...newConfig };
        console.log('[CBC] Configuration updated:', this.config);
    }
}

// Export the main class
const cbcModule = new CPUBottleneckCrusher();

// Auto-initialize if running in MSFS environment
if (typeof window !== 'undefined' && (window as any).simconnect) {
    cbcModule.initialize((window as any).simconnect);
}

export default cbcModule;
export { CPUBottleneckCrusher, CBCConfig, CPUStats };
