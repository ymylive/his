#!/bin/bash
set -euo pipefail

# Linux Release Package Script
# Usage: ./scripts/package-release-linux.sh [version]

VERSION="${1:-2.0.0}"
NORMALIZED_VERSION="${VERSION#v}"
BUILD_DIR="build-release"
DIST_DIR="dist"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "Building lightweight HIS for Linux..."
echo "Version: $VERSION"
echo "Project root: $PROJECT_ROOT"

# Clean and create directories
cd "$PROJECT_ROOT"
rm -rf "$BUILD_DIR" "$DIST_DIR"
mkdir -p "$DIST_DIR"

# Configure and build
echo "Configuring CMake..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

# Run tests
echo "Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

# Detect architecture
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)        ARCH_TAG="linux-x86_64" ;;
    aarch64|arm64) ARCH_TAG="linux-arm64" ;;
    *)             ARCH_TAG="linux-$ARCH" ;;
esac

# Package name
PACKAGE_NAME="lightweight-his-portable-v${NORMALIZED_VERSION}-${ARCH_TAG}"
STAGE_PATH="$DIST_DIR/$PACKAGE_NAME"
ZIP_PATH="$DIST_DIR/${PACKAGE_NAME}.zip"

echo "Creating package: $PACKAGE_NAME"

# Create staging directory
rm -rf "$STAGE_PATH"
mkdir -p "$STAGE_PATH"

# Copy executable
cp "$BUILD_DIR/his" "$STAGE_PATH/his"
chmod +x "$STAGE_PATH/his"

if [ -f "$BUILD_DIR/his_desktop" ]; then
    cp "$BUILD_DIR/his_desktop" "$STAGE_PATH/his_desktop"
    chmod +x "$STAGE_PATH/his_desktop"
fi

# Copy data and documentation
cp README.md "$STAGE_PATH/"
cp CHANGELOG.md "$STAGE_PATH/"
cp -r data "$STAGE_PATH/"

# Create launcher script
cat > "$STAGE_PATH/run.sh" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
./his "$@"
EOF
chmod +x "$STAGE_PATH/run.sh"

# Create START_HERE.txt
cat > "$STAGE_PATH/START_HERE.txt" << EOF
Lightweight HIS Portable v${NORMALIZED_VERSION} for Linux

启动方式:

  ./run.sh
  # 或直接
  ./his

Demo accounts:
  ADM0001 / admin123  - Administrator
  DOC0001 / doctor123 - Doctor
  INP0001 / inpatient123 - Inpatient Manager
  PHA0001 / pharmacy123 - Pharmacy
  PAT0001 / patient123 - Patient

注意:
  - 请保持 data/ 文件夹与可执行文件在同一目录
  - 需要支持 UTF-8 和 ANSI 的终端（大多数现代终端均支持）

Architecture: ${ARCH_TAG}
EOF

# Create tar.gz archive (primary for Linux — no extra tools needed)
TAR_PATH="$DIST_DIR/${PACKAGE_NAME}.tar.gz"
echo "Creating tar.gz archive..."
cd "$DIST_DIR"
tar czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME"

# Also create zip for consistency
ZIP_PATH="$DIST_DIR/${PACKAGE_NAME}.zip"
echo "Creating zip archive..."
zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"
cd "$PROJECT_ROOT"

echo ""
echo "Portable release ready:"
echo "  Folder: $STAGE_PATH"
echo "  tar.gz: $TAR_PATH"
echo "  zip:    $ZIP_PATH"
echo "  Arch:   $ARCH_TAG"
