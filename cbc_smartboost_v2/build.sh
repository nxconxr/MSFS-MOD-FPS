#!/bin/bash
# CBC SmartBoost v2.0 - Build Script für WASM
# Voraussetzung: Emscripten muss installiert und aktiviert sein

echo "========================================"
echo "CBC SmartBoost v2.0 - Build Script"
echo "========================================"

# Prüfen ob emcc verfügbar ist
if ! command -v emcc &> /dev/null; then
    echo "FEHLER: Emscripten (emcc) nicht gefunden!"
    echo ""
    echo "Installation:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo "Emscripten gefunden: $(emcc --version | head -1)"
echo ""

# Output Verzeichnis erstellen
mkdir -p package

# Kompilieren
echo "Kompiliere main.cpp zu cbc_core.wasm..."
emcc src/main.cpp -O2 \
  -s EXPORTED_FUNCTIONS='["_MSFS_Init","_MSFS_Update","_MSFS_Deinit"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -o package/cbc_core.wasm \
  --no-entry \
  -Wall

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build erfolgreich!"
    echo "  Output: package/cbc_core.wasm"
    echo "  Größe: $(ls -lh package/cbc_core.wasm | awk '{print $5}')"
    echo ""
    echo "Nächste Schritte:"
    echo "  1. Companion installieren: pip install -r companion/requirements.txt"
    echo "  2. package/ Ordner nach MSFS Community kopieren"
    echo "  3. pythonw companion/predictive_loader.py starten"
else
    echo ""
    echo "✗ Build fehlgeschlagen!"
    exit 1
fi
