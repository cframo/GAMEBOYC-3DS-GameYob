#!/usr/bin/env bash
set -e

COMMAND="${1:-build}"
PARAM="${2:-}"

case "$COMMAND" in
  build)
    echo "[*] Compilando con dkp-make..."
    ./dkp-make
    ;;
  clean)
    echo "[*] Limpiando artefactos..."
    rm -rf build output
    ./dkp-make
    ;;
  send)
    if [ -z "$PARAM" ]; then
      echo "[-] Error: Debes indicar la IP de la consola. Uso: ./dev.sh send <IP_3DS>"
      exit 1
    fi
    ./dkp-make
    echo "[*] Transfiriendo GameYob.3dsx a $PARAM vía Podman..."
    ./dkp-3dslink -a "$PARAM" output/GameYob.3dsx
    ;;
  *)
    echo "Uso: ./dev.sh [build | clean | send <IP_3DS>]"
    exit 1
    ;;
esac
