#!/bin/bash
set -e

# macOS Release Package Script
# Usage: ./scripts/package-release-macos.sh [version]

VERSION="${1:-2.0.0}"
BUILD_DIR="build-release"
DIST_DIR="dist"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "Building lightweight HIS for macOS..."
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
cmake --build "$BUILD_DIR" -j$(sysctl -n hw.ncpu)

# Run tests
echo "Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    ARCH_TAG="macos-arm64"
elif [ "$ARCH" = "x86_64" ]; then
    ARCH_TAG="macos-x86_64"
else
    ARCH_TAG="macos-$ARCH"
fi

# Package name
PACKAGE_NAME="lightweight-his-portable-v${VERSION}-${ARCH_TAG}"
STAGE_PATH="$DIST_DIR/$PACKAGE_NAME"
ZIP_PATH="$DIST_DIR/${PACKAGE_NAME}.zip"

echo "Creating package: $PACKAGE_NAME"

# Create staging directory
rm -rf "$STAGE_PATH"
mkdir -p "$STAGE_PATH"

# Copy executables
cp "$BUILD_DIR/his" "$STAGE_PATH/his"
cp "$BUILD_DIR/his_desktop" "$STAGE_PATH/his_desktop"

# Make executables executable
chmod +x "$STAGE_PATH/his"
chmod +x "$STAGE_PATH/his_desktop"

# Copy data and documentation
cp README.md "$STAGE_PATH/"
cp CHANGELOG.md "$STAGE_PATH/"
cp -r data "$STAGE_PATH/"

# Create launcher scripts
cat > "$STAGE_PATH/run_desktop.sh" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
./his_desktop
EOF

cat > "$STAGE_PATH/run_console.sh" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
./his
EOF

chmod +x "$STAGE_PATH/run_desktop.sh"
chmod +x "$STAGE_PATH/run_console.sh"

# Create START_HERE.txt
cat > "$STAGE_PATH/START_HERE.txt" << EOF
Lightweight HIS Portable v${VERSION} for macOS

Start desktop:
  ./run_desktop.sh
  or double-click run_desktop.sh in Finder

Start console:
  ./run_console.sh

Demo accounts:
  ADM0001 / admin123  - Administrator
  CLK0001 / clerk123  - Registration Clerk
  DOC0001 / doctor123 - Doctor
  INP0001 / inpatient123 - Inpatient Registrar
  WRD0001 / ward123 - Ward Manager
  PHA0001 / pharmacy123 - Pharmacy
  PAT0001 / patient123 - Patient

Notes:
  - Keep the data folder next to the executables
  - If macOS blocks the app, go to System Preferences > Security & Privacy
    and click "Open Anyway"
  - You may need to run: xattr -cr . to remove quarantine attributes

Architecture: ${ARCH_TAG}
EOF

# Create zip archive
echo "Creating zip archive..."
cd "$DIST_DIR"
zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"
cd "$PROJECT_ROOT"

echo ""
echo "✅ Portable release ready:"
echo "  Folder: $STAGE_PATH"
echo "  Zip:    $ZIP_PATH"
echo "  Arch:   $ARCH_TAG"
