#!/usr/bin/env python3
"""
CBC SmartBoost Profiling Script v1.0
Misst Frametimes, erkennt Bottlenecks und generiert Optimierungs-Empfehlungen

Voraussetzung: RTSS (RivaTuner Statistics Server) muss laufen mit Logging aktiviert
"""

import csv
import json
import sys
from datetime import datetime
from pathlib import Path

# Konfiguration
RTSS_LOG_PATH = "C:/Program Files (x86)/RTSS/Logs/MSFS2024.csv"
OUTPUT_DIR = Path("./cbc_profiles")
FPS_TARGET = 58
CPU_THRESHOLD = 85
GPU_THRESHOLD = 90

class CBCProfiler:
    def __init__(self):
        self.frames = []
        self.bottleneck_counts = {"cpu": 0, "gpu": 0, "none": 0}
        
    def load_rtss_log(self, filepath):
        if not Path(filepath).exists():
            print(f"Log-Datei nicht gefunden: {filepath}")
            return False
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f, delimiter=';')
            for row in reader:
                try:
                    frame = {
                        'timestamp': row.get('Timestamp', 0),
                        'fps': float(row.get('Framerate', 0)),
                        'frametime': float(row.get('Frametime', 0)),
                        'cpu_usage': float(row.get('CPU Usage', 0)),
                        'gpu_usage': float(row.get('GPU Usage', 0)),
                    }
                    self.frames.append(frame)
                except (ValueError, KeyError):
                    continue
        print(f"{len(self.frames)} Frames geladen")
        return len(self.frames) > 0
    
    def analyze_bottleneck(self):
        for frame in self.frames:
            fps = frame['fps']
            cpu = frame['cpu_usage']
            gpu = frame['gpu_usage']
            if fps < FPS_TARGET:
                if cpu > CPU_THRESHOLD and cpu > gpu:
                    self.bottleneck_counts["cpu"] += 1
                    frame['bottleneck'] = 'cpu'
                elif gpu > GPU_THRESHOLD and gpu > cpu:
                    self.bottleneck_counts["gpu"] += 1
                    frame['bottleneck'] = 'gpu'
                else:
                    self.bottleneck_counts["none"] += 1
                    frame['bottleneck'] = 'mixed'
            else:
                frame['bottleneck'] = 'none'
                
    def generate_stats(self):
        if not self.frames:
            return {}
        fps_values = [f['fps'] for f in self.frames]
        frametime_values = [f['frametime'] for f in self.frames]
        stats = {
            'total_frames': len(self.frames),
            'avg_fps': sum(fps_values) / len(fps_values),
            'min_fps': min(fps_values),
            'max_fps': max(fps_values),
            'avg_frametime': sum(frametime_values) / len(frametime_values),
            'fps_1pct_low': sorted(fps_values)[int(len(fps_values) * 0.01)],
            'bottleneck_distribution': self.bottleneck_counts,
            'avg_cpu': sum(f['cpu_usage'] for f in self.frames) / len(self.frames),
            'avg_gpu': sum(f['gpu_usage'] for f in self.frames) / len(self.frames),
        }
        avg_ft = stats['avg_frametime']
        variance = sum((ft - avg_ft) ** 2 for ft in frametime_values) / len(frametime_values)
        stats['frametime_variance'] = variance ** 0.5
        return stats
    
    def generate_recommendations(self, stats):
        recommendations = []
        total = max(1, sum(self.bottleneck_counts.values()))
        cpu_pct = self.bottleneck_counts["cpu"] / total * 100
        gpu_pct = self.bottleneck_counts["gpu"] / total * 100
        
        if cpu_pct > 50:
            recommendations.append({'type': 'CPU_BOTTLENECK', 'severity': 'HIGH' if cpu_pct > 70 else 'MEDIUM',
                'action': 'Traffic Density auf 30-50% reduzieren', 'details': f'CPU limitiert in {cpu_pct:.1f}% der Frames'})
            recommendations.append({'type': 'CPU_BOTTLENECK', 'severity': 'MEDIUM',
                'action': 'Trees LOD auf MEDIUM setzen', 'details': 'Baeume sind CPU-intensiv'})
        if gpu_pct > 50:
            recommendations.append({'type': 'GPU_BOTTLENECK', 'severity': 'HIGH' if gpu_pct > 70 else 'MEDIUM',
                'action': 'Volumetric Clouds von ULTRA auf HIGH setzen', 'details': f'GPU limitiert in {gpu_pct:.1f}% der Frames'})
            recommendations.append({'type': 'GPU_BOTTLENECK', 'severity': 'MEDIUM',
                'action': 'FSR 3 Quality Mode aktivieren', 'details': 'Bringt ~26% FPS bei guter Qualitaet'})
        if stats.get('frametime_variance', 0) > 5:
            recommendations.append({'type': 'STUTTER', 'severity': 'HIGH',
                'action': 'Rolling Cache auf NVMe SSD verlegen', 'details': f'Hohe Frametime-Variance ({stats["frametime_variance"]:.2f}ms)'})
        if stats.get('avg_fps', 0) < 40:
            recommendations.append({'type': 'LOW_FPS', 'severity': 'HIGH',
                'action': 'CBC Profile PERFORMANCE aktivieren', 'details': f'Durchschnittlich nur {stats["avg_fps"]:.1f} FPS'})
        return recommendations
    
    def export_profile(self, output_path):
        stats = self.generate_stats()
        recommendations = self.generate_recommendations(stats)
        profile = {
            'generated_at': datetime.now().isoformat(),
            'stats': stats,
            'recommendations': recommendations,
            'suggested_settings': {
                'traffic_density': 0.3 if self.bottleneck_counts["cpu"] > 50 else 0.6,
                'lod_bias_objects': -0.2 if self.bottleneck_counts["cpu"] > 50 else 0.0,
                'lod_bias_trees': -0.3 if self.bottleneck_counts["cpu"] > 50 else -0.1,
                'volumetric_clouds': 'HIGH' if self.bottleneck_counts["gpu"] > 50 else 'ULTRA',
            }
        }
        with open(output_path, 'w') as f:
            json.dump(profile, f, indent=2)
        print(f"Profil exportiert: {output_path}")
        return profile
    
    def print_report(self):
        stats = self.generate_stats()
        recommendations = self.generate_recommendations(stats)
        print("\n" + "="*60)
        print("CBC SMARTBOOST PROFILING REPORT")
        print("="*60)
        print(f"\nFRAMERATE:")
        print(f"   Durchschnitt: {stats.get('avg_fps', 0):.1f} FPS")
        print(f"   Minimum:      {stats.get('min_fps', 0):.1f} FPS")
        print(f"   1% Lows:      {stats.get('fps_1pct_low', 0):.1f} FPS")
        print(f"\nFRAMETIME:")
        print(f"   Durchschnitt: {stats.get('avg_frametime', 0):.2f} ms")
        print(f"   Variance:     {stats.get('frametime_variance', 0):.2f} ms")
        print(f"\nBOTTLENECK VERTEILUNG:")
        total = sum(self.bottleneck_counts.values())
        for key, value in self.bottleneck_counts.items():
            pct = value / max(1, total) * 100
            print(f"   {key.upper()}: {pct:.1f}% ({value} Frames)")
        if recommendations:
            print(f"\nEMPFEHLUNGEN ({len(recommendations)}):")
            for i, rec in enumerate(recommendations, 1):
                sev = {'HIGH': '[!]', 'MEDIUM': '[!]', 'LOW': '[OK]'}[rec['severity']]
                print(f"   {i}. {sev} [{rec['type']}] {rec['action']}")
        print("\n" + "="*60)

def main():
    print("CBC SmartBoost Profiler v1.0")
    profiler = CBCProfiler()
    log_path = sys.argv[1] if len(sys.argv) > 1 else RTSS_LOG_PATH
    if not profiler.load_rtss_log(log_path):
        print("Bitte RTSS Log-Pfad als Argument angeben:")
        print(f"  python cbc_profiler.py C:\\Pfad\\zum\\Log.csv")
        sys.exit(1)
    print("Analysiere Bottlenecks...")
    profiler.analyze_bottleneck()
    profiler.print_report()
    OUTPUT_DIR.mkdir(exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    profile_path = OUTPUT_DIR / f"cbc_profile_{timestamp}.json"
    profiler.export_profile(profile_path)

if __name__ == "__main__":
    main()
