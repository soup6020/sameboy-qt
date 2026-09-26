#!/usr/bin/env bash
# clang-format / clang-tidy for our sources (never third_party/). Run inside
# `nix develop`, which provides both via clang-tools.
#
#   scripts/lint.sh              # check formatting + run clang-tidy
#   scripts/lint.sh format       # reformat in place
#   scripts/lint.sh format-check # formatting only, non-zero exit on diffs
#   scripts/lint.sh tidy [files] # clang-tidy only (all sources by default)
#
# clang-tidy needs build/compile_commands.json: run `cmake -S . -B build` first
# (BUILD_DIR overrides the directory).
set -euo pipefail

cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
BUILD_DIR=${BUILD_DIR:-build}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}

sources() {
    git ls-files --cached --others --exclude-standard -- 'src/*.cpp' 'src/*.h' 'tests/*.cpp'
}

format() {
    sources | xargs clang-format -i
}

format_check() {
    sources | xargs clang-format --dry-run --Werror
}

tidy() {
    if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
        echo "error: $BUILD_DIR/compile_commands.json not found; run: cmake -S . -B $BUILD_DIR" >&2
        exit 1
    fi
    local files=("$@")
    if [[ ${#files[@]} -eq 0 ]]; then
        mapfile -t files < <(sources | grep '\.cpp$')
    fi
    # The compile database comes from GCC, so tolerate GCC-only warning flags.
    printf '%s\n' "${files[@]}" |
        xargs -P "$JOBS" -n 1 clang-tidy -p "$BUILD_DIR" --quiet --warnings-as-errors='*' \
            --extra-arg=-Wno-unknown-warning-option
}

case "${1:-all}" in
    format) format ;;
    format-check) format_check ;;
    tidy) shift; tidy "$@" ;;
    all) format_check && tidy ;;
    *) echo "usage: $0 [format|format-check|tidy [files...]]" >&2; exit 2 ;;
esac
