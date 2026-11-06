#!/usr/bin/env bash
# Fail if a binary references glibc symbol versions newer than a ceiling.
#
#   scripts/check-abi.sh --max 2.33 [--needed] <binary> [binary...]
#
# This is the guard for the one mistake that silently produces an unrunnable
# binary: building with a toolchain whose glibc is newer than the target's.
# glibc >= 2.34 re-versioned __libc_start_main and folded in libpthread, so a
# single GLIBC_2.34 reference is enough to make ld.so refuse the executable.
#
# Each port that needs it sets the ceiling (DXL_PORT_GLIBC_MAX in its
# port.cmake); the build runs this after linking. readelf reads the version
# needs of any ELF, whatever its architecture, so no cross binutils needed.
#
# With --needed, the shared libraries a program needs must be the C
# library's alone (libc, libm, libdl, libpthread, librt, the loader): the
# launcher's programs open everything else at run time, and a library linked
# instead would add its own NEEDED and run nowhere it is not. The engine,
# which the workspace checks with the same script, needs its own.
set -euo pipefail

max="" needed=""
if [ "${1:-}" = "--max" ]; then max="${2:-}"; shift 2; fi
if [ "${1:-}" = "--needed" ]; then needed=1; shift; fi
case "$max" in
    2.[0-9]*) ;;
    *) echo "usage: $0 --max 2.NN <binary> [binary...]" >&2; exit 2 ;;
esac
[ $# -ge 1 ] || { echo "usage: $0 --max 2.NN <binary> [binary...]" >&2; exit 2; }
MAX_MINOR="${max#2.}"

# None at all, a static program's, is no error: under pipefail grep's 1
# would end the script there, unsaid, the files after it unchecked.
versions() {
    readelf -V --wide "$1" 2>/dev/null | grep -oE 'GLIBC_2\.[0-9]+' | sort -u -t. -k2,2n || true
}

needed() {
    readelf -d --wide "$1" 2>/dev/null | grep -oE 'Shared library: \[[^]]+\]' | sed 's/Shared library: \[\(.*\)\]/\1/'
}

rc=0
for bin in "$@"; do
    [ -f "$bin" ] || { echo "MISSING $bin" >&2; rc=1; continue; }
    readelf -h "$bin" > /dev/null 2>&1 || { echo "FAIL $bin is no ELF file readelf reads" >&2; rc=1; continue; }
    have=$(versions "$bin")
    bad=$(printf '%s\n' "$have" | awk -F. -v max="$MAX_MINOR" 'NF == 2 && $2 > max')
    if [ -n "$bad" ]; then
        echo "FAIL $bin references $(echo "$bad" | tr '\n' ' ')(ceiling $max)" >&2
        rc=1
    else
        echo "OK   $bin [$(echo "${have:-no glibc refs}" | tr '\n' ' ')]"
    fi
    if [ -n "$needed" ]; then
        extra=$(needed "$bin" | grep -vE '^(libc|libm|libdl|libpthread|librt)\.so|^ld-linux' || true)
        if [ -n "$extra" ]; then
            echo "FAIL $bin needs $(echo "$extra" | tr '\n' ' '): the C library's alone are allowed" >&2
            rc=1
        fi
    fi
done
exit $rc
