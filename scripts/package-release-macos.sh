#!/bin/bash
set -euo pipefail

# macOS Release Package Script
# Usage: ./scripts/package-release-macos.sh [version]

VERSION="${1:-2.0.0}"
NORMALIZED_VERSION="${VERSION#v}"
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
PACKAGE_NAME="lightweight-his-portable-v${NORMALIZED_VERSION}-${ARCH_TAG}"
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

# Copy setup script
cp scripts/setup-macos.sh "$STAGE_PATH/"
chmod +x "$STAGE_PATH/setup-macos.sh"

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
Lightweight HIS Portable v${NORMALIZED_VERSION} for macOS

⚠️  首次运行前必读 ⚠️

由于应用未经 Apple 签名，macOS 会阻止运行。请先运行配置脚本：

  ./setup-macos.sh

或手动配置：

  xattr -cr .
  chmod +x run_desktop.sh his_desktop

然后启动：

  ./run_desktop.sh    # 桌面版（推荐）
  ./run_console.sh    # 控制台版

Demo accounts:
  ADM0001 / admin123  - Administrator
  DOC0001 / doctor123 - Doctor
  INP0001 / inpatient123 - Inpatient Manager
  PHA0001 / pharmacy123 - Pharmacy
  PAT0001 / patient123 - Patient

详细说明：
  - 安全配置：docs/MACOS_SECURITY.md
  - 完整文档：docs/MACOS_SUPPORT.md

Notes:
  - Keep the data folder next to the executables
  - For security details, see docs/MACOS_SECURITY.md

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
