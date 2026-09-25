#!/usr/bin/env bash
# Moves the vendored SameBoy submodule to a new upstream ref, rebuilds, runs
# the tests and summarises what changed upstream so it can be ported.
#
#   scripts/sync-sameboy.sh             # newest v* release tag
#   scripts/sync-sameboy.sh master      # tip of upstream master
#   scripts/sync-sameboy.sh <ref>       # any tag, branch or commit
#
# Environment: BUILD_DIR (default: build), SKIP_BUILD=1 to only move the pin.
set -euo pipefail

cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
SUBMODULE=third_party/SameBoy
BUILD_DIR=${BUILD_DIR:-build}

git submodule update --init "$SUBMODULE"
git -C "$SUBMODULE" fetch --tags --force origin

ref=${1:-}
if [[ -z "$ref" ]]; then
    ref=$(git -C "$SUBMODULE" tag --list 'v*' --sort=-version:refname | head -n1)
    echo "Latest upstream release: $ref"
elif [[ "$ref" == "master" ]]; then
    ref=origin/master
fi

old=$(git -C "$SUBMODULE" rev-parse HEAD)
git -C "$SUBMODULE" checkout --quiet --detach "$ref"
new=$(git -C "$SUBMODULE" rev-parse HEAD)

if [[ "$old" == "$new" ]]; then
    echo "Already at $ref ($new)."
else
    echo
    echo "== Upstream commits ${old:0:10}..${new:0:10} =="
    git -C "$SUBMODULE" log --oneline --no-merges "$old..$new"

    echo
    echo "== Public core headers changed (API surface used by src/) =="
    git -C "$SUBMODULE" diff --stat "$old" "$new" -- 'Core/*.h' || true

    echo
    echo "== Cocoa frontend files changed (candidates for porting to the Qt UI) =="
    git -C "$SUBMODULE" diff --stat "$old" "$new" -- Cocoa AppleCommon || true

    echo
    echo "== Other assets we consume (boot ROMs, shaders, symbols, version) =="
    git -C "$SUBMODULE" diff --stat "$old" "$new" -- BootROMs Shaders Misc/registers.sym version.mk || true

    # Theme defaults are transcribed into src/settings/PaletteThemes.cpp.
    if git -C "$SUBMODULE" diff --quiet "$old" "$new" -- Cocoa/GBApp.m; then :; else
        if git -C "$SUBMODULE" diff "$old" "$new" -- Cocoa/GBApp.m | grep -q 'GBThemes\|registerDefaults\|@"GB'; then
            echo
            echo "!! Cocoa/GBApp.m defaults or themes changed: review src/settings/Settings.cpp and PaletteThemes.cpp"
        fi
    fi
fi

# Keep the flake's pinned copy (used by `nix build`) in step with the submodule.
if command -v nix >/dev/null 2>&1 && [[ -f flake.nix ]]; then
    echo
    echo "Updating flake input 'sameboy' to $new"
    nix flake lock --override-input sameboy "github:LIJI32/SameBoy/$new" || \
        echo "warning: could not update flake.lock; run: nix flake lock --override-input sameboy github:LIJI32/SameBoy/$new"
fi

if [[ -z "${SKIP_BUILD:-}" ]]; then
    echo
    echo "== Building and testing =="
    cmake -S . -B "$BUILD_DIR" -G Ninja
    cmake --build "$BUILD_DIR"
    (cd "$BUILD_DIR" && ctest --output-on-failure)
    if [[ -n "$(git -C "$SUBMODULE" status --porcelain)" ]]; then
        echo "error: the build modified files inside $SUBMODULE" >&2
        exit 1
    fi
fi

echo
echo "Submodule now at $(git -C "$SUBMODULE" describe --tags --always). Review, then commit:"
echo "  git add $SUBMODULE flake.lock && git commit -m \"Sync SameBoy to $(git -C "$SUBMODULE" describe --tags --always)\""
