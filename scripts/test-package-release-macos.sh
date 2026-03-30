#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_INPUT="${1:-v9.9.9}"

ARCH="$(uname -m)"
if [ "$ARCH" = "arm64" ]; then
    ARCH_TAG="macos-arm64"
elif [ "$ARCH" = "x86_64" ]; then
    ARCH_TAG="macos-x86_64"
else
    ARCH_TAG="macos-$ARCH"
fi

NORMALIZED_VERSION="${VERSION_INPUT#v}"
EXPECTED_ZIP="$PROJECT_ROOT/dist/lightweight-his-portable-v${NORMALIZED_VERSION}-${ARCH_TAG}.zip"
UNEXPECTED_ZIP="$PROJECT_ROOT/dist/lightweight-his-portable-v${VERSION_INPUT}-${ARCH_TAG}.zip"

"$PROJECT_ROOT/scripts/package-release-macos.sh" "$VERSION_INPUT"

if [ ! -f "$EXPECTED_ZIP" ]; then
    echo "Expected package missing: $EXPECTED_ZIP" >&2
    exit 1
fi

if [ -f "$UNEXPECTED_ZIP" ] && [ "$UNEXPECTED_ZIP" != "$EXPECTED_ZIP" ]; then
    echo "Unexpected package name present: $UNEXPECTED_ZIP" >&2
    exit 1
fi

echo "macOS release package naming is correct: $(basename "$EXPECTED_ZIP")"
