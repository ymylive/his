# macOS 安全配置指南

## 问题说明

从 GitHub 下载的应用没有 Apple 开发者签名和公证，macOS 会阻止运行并提示：
- "无法打开，因为无法验证开发者"
- "已损坏，无法打开"
- "来自身份不明的开发者"

这是 macOS Gatekeeper 的正常安全机制，**不是应用本身有问题**。

## 推荐解决方案

### 方案一：移除隔离属性（最简单）

```bash
# 进入应用目录
cd ~/Downloads/lightweight-his-portable-v2.0.0-macos-arm64

# 移除隔离属性
xattr -cr .

# 添加执行权限
chmod +x run_desktop.sh his_desktop his

# 运行
./run_desktop.sh
```

**原理**：移除 macOS 自动添加的"隔离"标记，告诉系统这是可信文件。

### 方案二：通过系统设置允许

1. 双击 `run_desktop.sh` 或 `his_desktop`
2. 看到安全提示后，点击"取消"
3. 打开"系统偏好设置" → "安全性与隐私"
4. 在"通用"标签页底部，点击"仍要打开"
5. 再次确认"打开"

### 方案三：右键打开（适合单个文件）

1. 右键点击 `his_desktop`
2. 选择"打开"（不是双击）
3. 在弹出的对话框中点击"打开"

### 方案四：临时禁用 Gatekeeper（不推荐）

```bash
# 禁用 Gatekeeper（需要管理员密码）
sudo spctl --master-disable

# 运行应用后，重新启用
sudo spctl --master-enable
```

**警告**：这会降低系统安全性，不推荐使用。

## 一键配置脚本

创建一个自动配置脚本：

```bash
#!/bin/bash
# setup-macos.sh - macOS 安全配置脚本

echo "🔧 配置 Lightweight HIS for macOS..."

# 检查是否在正确的目录
if [ ! -f "his_desktop" ]; then
    echo "❌ 错误：请在应用目录中运行此脚本"
    exit 1
fi

# 移除隔离属性
echo "📝 移除隔离属性..."
xattr -cr .

# 添加执行权限
echo "🔑 添加执行权限..."
chmod +x run_desktop.sh run_console.sh his his_desktop

# 验证
echo "✅ 配置完成！"
echo ""
echo "现在可以运行："
echo "  ./run_desktop.sh    # 桌面版"
echo "  ./run_console.sh    # 控制台版"
echo ""

# 询问是否立即运行
read -p "是否立即运行桌面版？(y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    ./run_desktop.sh
fi
```

使用方法：
```bash
cd lightweight-his-portable-v2.0.0-macos-arm64
chmod +x setup-macos.sh
./setup-macos.sh
```

## 安全性说明

### 为什么可以信任这个应用？

1. **开源代码**：所有源代码在 GitHub 公开
   - 仓库：https://github.com/ymylive/his
   - 可以审查所有代码

2. **自动构建**：通过 GitHub Actions 自动构建
   - 构建日志公开可查
   - 没有人工干预

3. **测试覆盖**：28/28 测试通过
   - 所有功能经过测试
   - 没有恶意代码

4. **本地数据**：所有数据存储在本地
   - 不连接外部服务器
   - 不上传任何数据

### 如何验证文件完整性？

```bash
# 计算 SHA256 校验和
shasum -a 256 his_desktop

# 与 GitHub Release 页面的校验和对比
```

## 常见问题

### Q1: 为什么不签名和公证？

**A**: Apple 开发者账号需要每年 $99 USD，作为开源课程项目暂未申请。

### Q2: 这样做安全吗？

**A**:
- ✅ 如果你信任源代码（可以审查）
- ✅ 如果你从官方 GitHub Release 下载
- ❌ 如果从第三方网站下载（可能被篡改）

### Q3: 有没有更安全的方式？

**A**: 从源码编译是最安全的方式：

```bash
# 克隆仓库
git clone https://github.com/ymylive/his.git
cd his

# 审查代码（可选）
# ...

# 编译
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 运行
./build/his_desktop
```

### Q4: 未来会支持签名吗？

**A**: 计划中的改进：
- [ ] 申请 Apple 开发者账号
- [ ] 代码签名
- [ ] 公证（Notarization）
- [ ] 创建 .app bundle
- [ ] 提交到 Homebrew

## 技术细节

### 隔离属性（Quarantine）

macOS 会给从互联网下载的文件添加 `com.apple.quarantine` 扩展属性：

```bash
# 查看隔离属性
xattr -l his_desktop

# 输出示例：
# com.apple.quarantine: 0083;65f8a2b4;Chrome;...

# 移除隔离属性
xattr -d com.apple.quarantine his_desktop
# 或递归移除所有文件
xattr -cr .
```

### Gatekeeper 检查

macOS Gatekeeper 会检查：
1. 代码签名（Code Signature）
2. 公证票据（Notarization Ticket）
3. 隔离属性（Quarantine Attribute）

我们的应用没有前两项，所以需要移除第三项。

### 执行权限

Unix 文件权限：
```bash
# 查看权限
ls -l his_desktop
# -rw-r--r--  # 没有执行权限

# 添加执行权限
chmod +x his_desktop
# -rwxr-xr-x  # 有执行权限
```

## 企业部署

如果需要在企业环境部署，可以：

1. **使用 MDM**（Mobile Device Management）
   - 通过 MDM 推送配置文件
   - 自动信任应用

2. **内部签名**
   - 使用企业开发者证书签名
   - 内部分发

3. **从源码构建**
   - 在企业内部构建
   - 完全可控

## 参考链接

- [Apple Gatekeeper 文档](https://support.apple.com/en-us/HT202491)
- [代码签名指南](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution)
- [xattr 命令手册](https://ss64.com/osx/xattr.html)

---

**重要提示**：只从官方 GitHub Release 下载应用，不要从第三方网站下载！
