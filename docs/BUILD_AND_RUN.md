# 构建与运行指南

## 直接构建

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 运行

控制台版：

```powershell
.\build\his.exe
```

桌面版：

```powershell
.\build\his_desktop.exe
```

桌面 smoke：

```powershell
.\build\his_desktop.exe --smoke
```

## Docker

构建镜像：

```bash
docker build -t lightweight-his:latest .
```

运行控制台版：

```bash
docker run --rm -it lightweight-his:latest
```

仅做容器构建验证：

```bash
docker build --target build -t lightweight-his-build:latest .
```

说明：Docker 主要用于构建、测试和控制台演示，不作为桌面 GUI 的默认运行方式。

## 便携版打包

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

## 演示账号

- `ADM0001 / admin123`
- `CLK0001 / clerk123`
- `DOC0001 / doctor123`
- `INP0001 / inpatient123`
- `WRD0001 / ward123`
- `PHA0001 / pharmacy123`
- `PAT0001 / patient123`
