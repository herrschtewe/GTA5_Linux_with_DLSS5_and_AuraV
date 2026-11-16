#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DLL="${SCRIPT_DIR}/cabinet.dll"

# Locate Wine's x86_64-windows/libkernel32.a
KERNEL32_LIB="${1:-}"
if [[ -z "${KERNEL32_LIB}" ]]; then
    KERNEL32_LIB="$(find \
        "$HOME/.local/share/Steam/compatibilitytools.d" \
        "$HOME/.local/share/lutris/runners/wine" \
        /usr/lib/wine /usr/lib64/wine /opt \
        -path "*/x86_64-windows/libkernel32.a" 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "${KERNEL32_LIB}" || ! -f "${KERNEL32_LIB}" ]]; then
    echo "ERROR: Could not locate x86_64-windows/libkernel32.a."
    echo "Usage: $0 [/path/to/x86_64-windows/libkernel32.a]"
    exit 1
fi

echo "Using libkernel32.a: ${KERNEL32_LIB}"

CFLAGS=(-c -fno-builtin -fno-stack-protector -nostdlib -mabi=ms -mcmodel=small -fPIC -O2)
if [[ "${DEBUG_LOG:-0}" == "1" ]]; then
    CFLAGS+=(-DENABLE_PROXY_LOG)
fi

gcc "${CFLAGS[@]}" -DNDEBUG=1 -I"${SCRIPT_DIR}" "${SCRIPT_DIR}/pyxca_xpress.c" -o "${SCRIPT_DIR}/pyxca_xpress_win.o"
gcc "${CFLAGS[@]}" "${SCRIPT_DIR}/my_cabinet_ms.c" -o "${SCRIPT_DIR}/my_cabinet_ms.o"

ld -m i386pep --shared -e DllMain \
    "${SCRIPT_DIR}/cabinet.def" \
    "${SCRIPT_DIR}/my_cabinet_ms.o" \
    "${SCRIPT_DIR}/pyxca_xpress_win.o" \
    "${KERNEL32_LIB}" \
    -o "${OUT_DLL}"

objcopy -R .comment "${OUT_DLL}" 2>/dev/null || true
rm -f "${SCRIPT_DIR}/pyxca_xpress_win.o" "${SCRIPT_DIR}/my_cabinet_ms.o"

echo "Successfully built: ${OUT_DLL}"
