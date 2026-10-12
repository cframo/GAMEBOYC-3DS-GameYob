#!/usr/bin/env bash
set -eo pipefail

echo "========================================="
echo "[*] INICIANDO SMOKE TEST: GameYob 3DS"
echo "========================================="

# 1. Limpieza de artefactos previos
echo "[1/3] Limpiando entorno..."
rm -rf build output

# 2. Compilación
echo "[2/3] Compilando con toolchain devkitARM..."
./dkp-make

# 3. Validación de integridad de binarios
echo "[3/3] Validando artefactos de salida..."
if [ ! -f "output/GameYob.3dsx" ]; then
    echo "[-] ERROR FATAL: output/GameYob.3dsx no fue generado."
    exit 1
fi

SIZE=$(stat -c%s "output/GameYob.3dsx")
if [ "$SIZE" -lt 100000 ]; then
    echo "[-] ERROR: El binario generado es sospechosamente pequeño ($SIZE bytes)."
    exit 1
fi

echo "========================================="
echo "[+] COMPILACIÓN EXITOSA ($SIZE bytes)"
echo "[+] Binario listo en output/GameYob.3dsx"
echo "========================================="
