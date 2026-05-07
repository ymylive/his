# HIS 发版与 CI 构建操作指南

## 标准发版流程

### 1. 确认代码就绪

```bash
cd /Users/cornna/project/his/his

# 确认工作区干净
git status

# 本地构建测试通过
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . && ctest --output-on-failure
```

### 2. 提交代码

```bash
cd /Users/cornna/project/his/his

# 暂存修改的文件（指定文件名，避免 git add -A 误提交）
git add src/xxx.c include/xxx.h

# 提交（使用 HEREDOC 保证多行消息格式正确）
git commit -m "$(cat <<'EOF'
fix: 修复某某问题

详细描述...
EOF
)"
```

### 3. 更新版本号

```bash
# 修改版本号（遵循语义化版本: 主版本.次版本.修订号）
# - 修订号 (patch): bug修复 (5.1.0 -> 5.1.1)
# - 次版本 (minor): 新功能 (5.1.0 -> 5.2.0)
# - 主版本 (major): 重大变更 (5.1.0 -> 6.0.0)
sed -i '' 's/HIS_VERSION "旧版本"/HIS_VERSION "新版本"/' include/common/UpdateChecker.h

# 提交版本号变更
git add include/common/UpdateChecker.h
git commit -m "chore: bump version to v新版本"
```

### 4. 推送到 GitHub

```bash
git push origin main
```

此步骤会自动触发 **CI 工作流**（三平台构建+测试），但不会发布 Release。

### 5. 打标签触发 Release 构建

```bash
# 创建带注释的标签
git tag -a v新版本 -m "v新版本 变更说明"

# 推送标签到远程
git push origin v新版本
```

推送 `v*` 标签会自动触发以下工作流：

| 工作流 | 触发条件 | 构建内容 |
|--------|---------|---------|
| **CI** | push to main | Linux/macOS/Windows 构建+测试 |
| **Multi-Platform Release** | push tag `v*` | 三平台打包 + GitHub Release 发布 |
| **Portable Release** | push tag `v*` | Windows 便携包 |

### 6. 监控 CI 状态

```bash
# 查看所有运行中的工作流
gh run list --limit 5

# 实时监控特定工作流（会阻塞直到完成）
gh run watch <run_id> --exit-status

# 查看失败日志
gh run view <run_id> --log-failed
```

### 7. 确认 Release

```bash
# 查看发布的 Release
gh release view v新版本

# 应看到 4 个产物:
# - lightweight-his-portable-v新版本-linux-x86_64.tar.gz
# - lightweight-his-portable-v新版本-linux-x86_64.zip
# - lightweight-his-portable-v新版本-macos-arm64.zip
# - lightweight-his-portable-v新版本-win64.zip
```

---

## 一键发版命令（复制即用）

```bash
# 替换 VERSION 和 MESSAGE
VERSION="5.2.0"
MESSAGE="新功能描述"

sed -i '' "s/HIS_VERSION \"[^\"]*\"/HIS_VERSION \"$VERSION\"/" include/common/UpdateChecker.h \
  && git add -u \
  && git commit -m "chore: bump version to v$VERSION" \
  && git push origin main \
  && git tag -a "v$VERSION" -m "v$VERSION $MESSAGE" \
  && git push origin "v$VERSION" \
  && echo "Done! Watch: gh run list --limit 3"
```

---

## 故障处理

### CI 失败后重新发版

```bash
# 修复问题后提交推送
git add ... && git commit -m "fix: ..." && git push origin main

# 删除旧标签
git tag -d v旧版本
git push origin :refs/tags/v旧版本

# 重新打标签
git tag -a v旧版本 -m "v旧版本 说明"
git push origin v旧版本
```

### 查看特定平台构建日志

```bash
# 查看 Windows 构建失败详情
gh run view <run_id> --log-failed | grep -A 20 "Windows"
```

---

## 演示数据重新生成

如果修改了密码哈希迭代次数等影响认证的参数：

```bash
cd tools
cc -o regen_user_fixture regen_user_fixture.c
./regen_user_fixture > ../data/demo_seed/users.txt
./regen_user_fixture > ../data/users.txt
```
