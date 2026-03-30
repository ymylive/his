# README User Guide Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite `README.md` so first-time users can find all demo accounts, download the correct build, start the app, and follow a complete demo path without reading developer docs first.

**Architecture:** Keep the repository root `README.md` as a user-first guide. Move developer-heavy content out of the main narrative and replace it with short links to the detailed docs that already exist. Preserve accurate platform-specific startup steps and current release naming.

**Tech Stack:** Markdown, existing repository docs, GitHub Releases naming

---

### Task 1: Restructure README For End Users

**Files:**
- Modify: `README.md`
- Reference: `docs/MACOS_SUPPORT.md`
- Reference: `docs/MACOS_SECURITY.md`
- Reference: `docs/BUILD_AND_RUN.md`
- Reference: `docs/INSTALL.md`

- [ ] **Step 1: Rewrite the README outline**

Replace the mixed developer/user structure with this order:

```md
# 项目标题
一句话项目说明

## 演示账号
完整 7 个账号

## 如何开始
Windows / macOS / Linux

## 推荐演示路线
3 条闭环路线

## 功能总览
7 个角色

## 常见问题
macOS、安全配置、中文显示、数据目录

## 更多文档
docs/ 链接
```

- [ ] **Step 2: Put all demo accounts near the top**

Use this exact account list:

```md
- `ADM0001 / admin123` - 管理员
- `CLK0001 / clerk123` - 挂号员
- `DOC0001 / doctor123` - 医生
- `INP0001 / inpatient123` - 住院登记员
- `WRD0001 / ward123` - 病区管理员
- `PHA0001 / pharmacy123` - 药房
- `PAT0001 / patient123` - 患者
```

- [ ] **Step 3: Rewrite startup instructions for users**

Keep only end-user startup steps in the main flow:

```md
### Windows
1. 打开最新 Release
2. 下载 `lightweight-his-portable-v*-win64.zip`
3. 解压并运行 `run_desktop.bat`

### macOS
1. 下载 `lightweight-his-portable-v*-macos-arm64.zip`
2. 解压后进入目录
3. 运行 `./setup-macos.sh`
4. 运行 `./run_desktop.sh`

### Linux
说明当前主要通过源码构建运行，并引导到 `docs/BUILD_AND_RUN.md`
```

- [ ] **Step 4: Add recommended demo flows**

Add these 3 user-facing flows:

```md
1. 患者挂号 -> 医生接诊 -> 药房发药 -> 患者查看发药记录
2. 患者挂号 -> 医生开检查 -> 检查结果回写 -> 患者查看检查摘要
3. 患者挂号 -> 医生建议住院 -> 住院登记员办理入院 -> 病区管理员转床/检查 -> 办理出院
```

- [ ] **Step 5: Compress developer content**

Do not keep long command blocks for source build, packaging, or Docker in the main body. Replace them with short links:

```md
- 安装依赖：`docs/INSTALL.md`
- 构建与运行：`docs/BUILD_AND_RUN.md`
- macOS 支持：`docs/MACOS_SUPPORT.md`
- macOS 安全配置：`docs/MACOS_SECURITY.md`
- 字体加载说明：`docs/FONT_LOADING.md`
```

- [ ] **Step 6: Verify the rewritten README**

Run:

```bash
sed -n '1,260p' README.md
```

Expected:
- demo accounts appear before startup steps
- startup instructions match current release assets
- no duplicated “快速开始” or duplicated account sections remain

- [ ] **Step 7: Record the docs-only status**

State explicitly in the final report that this is a docs-only change and that no code tests were required beyond content verification.
