#!/usr/bin/env bash
# Fail if a binary needs more than its target has: on Linux, glibc symbol
# versions newer than a ceiling; on Windows, a DLL Windows does not have
# itself, or a Windows older than the floor in its PE headers.
#
#   scripts/check-abi.sh --max 2.33 <binary> [binary...]
#   scripts/check-abi.sh --windows 10.0.18362 <program.exe|library.dll> [...]
#
# This is the guard for the one mistake that silently produces an unrunnable
# binary: building with a toolchain whose glibc is newer than the target's.
# glibc >= 2.34 re-versioned __libc_start_main and folded in libpthread, so a
# single GLIBC_2.34 reference is enough to make ld.so refuse the executable.
# On Windows, a toolchain's own DLL (libc++, libunwind, winpthread) left
# unlinked statically is the same mistake: the program does not start on a
# machine without it.
#
# Each port that needs it sets the ceiling (DXL_PORT_GLIBC_MAX in its
# port.cmake) or the floor (DXL_PORT_WINDOWS_MIN); the build runs this after
# linking. readelf reads the version needs of any ELF, whatever its
# architecture, and objdump the imports of any PE (DXL_OBJDUMP another), so
# no cross binutils needed.
set -euo pipefail

# Windows 10's own DLLs, and the app's own beside its programs: a PE may
# import these and no others. The UCRT's api-ms-win-crt-* and the
# api-ms-win-core-* sets are Windows 10's too.
WINDOWS_DLLS="advapi32 bcrypt cfgmgr32 comctl32 comdlg32 d3d11 d3dcompiler_47 dbghelp dwmapi dxgi gdi32 hid
imm32 iphlpapi kernel32 ntdll ole32 oleaut32 opengl32 psapi setupapi shcore shell32 shlwapi user32 userenv
uxtheme version winmm winspool ws2_32
openal32"

windows() {
    local floor="$1"; shift
    local major="${floor%%.*}" rc=0 bin
    for bin in "$@"; do
        [ -f "$bin" ] || { echo "MISSING $bin" >&2; rc=1; continue; }
        local head
        head=$(${DXL_OBJDUMP:-objdump} -p "$bin" 2>/dev/null) || { echo "FAIL $bin is no PE file objdump reads" >&2; rc=1; continue; }
        if ! grep -q 'file format pei-x86-64' <<< "$head"; then
            echo "FAIL $bin is not an x86_64 PE" >&2; rc=1; continue
        fi
        local os sub bad="" dll dlls=""
        os=$(awk '$1 == "MajorOSystemVersion" { print $2 }' <<< "$head")
        sub=$(awk '$1 == "MajorSubsystemVersion" { print $2 }' <<< "$head")
        while read -r dll; do
            local name; name=$(printf '%s' "${dll%.*}" | tr 'A-Z' 'a-z')
            dlls="$dlls $name"
            case "$name" in api-ms-win-crt-*|api-ms-win-core-*) continue ;; esac
            case " $(echo $WINDOWS_DLLS) " in *" $name "*) ;; *) bad="$bad $dll" ;; esac
        done < <(sed -n 's/^[[:space:]]*DLL Name: //p' <<< "$head")
        if [ -n "$bad" ]; then
            echo "FAIL $bin imports DLLs Windows does not have:$bad" >&2; rc=1
        elif [ "${os:-0}" -lt "$major" ] || [ "${sub:-0}" -lt "$major" ]; then
            echo "FAIL $bin says Windows $os (subsystem $sub), below the floor $floor" >&2; rc=1
        else
            local own=0 d
            for d in $dlls; do case "$d" in api-ms-win-*) ;; *) own=$((own + 1)) ;; esac; done
            echo "OK   $bin [Windows $os; $own DLLs of Windows' and the UCRT]"
        fi
    done
    return $rc
}

if [ "${1:-}" = "--windows" ]; then
    floor="${2:-}"
    case "$floor" in
        [0-9]*.[0-9]*) ;;
        *) echo "usage: $0 --windows 10.0.NNNNN <program> [program...]" >&2; exit 2 ;;
    esac
    shift 2
    [ $# -ge 1 ] || { echo "usage: $0 --windows 10.0.NNNNN <program> [program...]" >&2; exit 2; }
    windows "$floor" "$@"
    exit $?
fi

max=""
if [ "${1:-}" = "--max" ]; then max="${2:-}"; shift 2; fi
case "$max" in
    2.[0-9]*) ;;
    *) echo "usage: $0 --max 2.NN <binary> [binary...]" >&2; exit 2 ;;
esac
[ $# -ge 1 ] || { echo "usage: $0 --max 2.NN <binary> [binary...]" >&2; exit 2; }
MAX_MINOR="${max#2.}"

versions() {
    readelf -V --wide "$1" 2>/dev/null | grep -oE 'GLIBC_2\.[0-9]+' | sort -u -t. -k2,2n
}

rc=0
for bin in "$@"; do
    [ -f "$bin" ] || { echo "MISSING $bin" >&2; rc=1; continue; }
    have=$(versions "$bin")
    bad=$(printf '%s\n' "$have" | awk -F. -v max="$MAX_MINOR" 'NF == 2 && $2 > max')
    if [ -n "$bad" ]; then
        echo "FAIL $bin references $(echo "$bad" | tr '\n' ' ')(ceiling $max)" >&2
        rc=1
    else
        echo "OK   $bin [$(echo "${have:-no glibc refs}" | tr '\n' ' ')]"
    fi
done
exit $rc
