# 依赖安装指南

## 人类开发者

### Windows

推荐环境：`MSYS2 MinGW32`。

```bash
pacman -Syu
pacman -S --needed \
  mingw-w64-i686-gcc \
  mingw-w64-i686-cmake \
  mingw-w64-i686-ninja \
  git
```

可选 `PATH`：

```text
C:\msys64\mingw32\bin
C:\Program Files\Git\cmd
```

### Linux

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  git \
  libasound2-dev \
  libgl1-mesa-dev \
  libwayland-dev \
  libx11-dev \
  libxcursor-dev \
  libxi-dev \
  libxinerama-dev \
  libxkbcommon-dev \
  libxrandr-dev
```

### 图形依赖

- `raylib` 已随仓库 vendored
- `raygui.h` 已随仓库 vendored
- 不需要单独安装 `raylib` / `raygui`

### 中文字体

桌面端会优先尝试以下 Windows 字体：

- `NotoSansSC-VF.ttf`
- `msyh.ttc`
- `simhei.ttf`
- `simsun.ttc`

## AI / Agent 环境

最低要求：

- C11 编译器
- `cmake`
- `ninja` 或等价生成器
- `git`

推荐验证：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
