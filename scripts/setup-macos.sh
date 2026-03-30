#!/bin/bash
# setup-macos.sh - macOS 安全配置脚本
# 用于首次运行前配置应用

set -e

echo "🔧 配置 Lightweight HIS for macOS..."
echo ""

# 检查是否在正确的目录
if [ ! -f "his_desktop" ]; then
    echo "❌ 错误：请在应用目录中运行此脚本"
    echo "   应该包含 his_desktop 文件"
    exit 1
fi

# 移除隔离属性
echo "📝 步骤 1/3: 移除隔离属性..."
if xattr -cr . 2>/dev/null; then
    echo "   ✅ 隔离属性已移除"
else
    echo "   ⚠️  无法移除隔离属性（可能已经移除）"
fi

# 添加执行权限
echo "🔑 步骤 2/3: 添加执行权限..."
chmod +x run_desktop.sh run_console.sh his his_desktop 2>/dev/null || true
echo "   ✅ 执行权限已添加"

# 验证
echo "✅ 步骤 3/3: 验证配置..."
if [ -x "his_desktop" ]; then
    echo "   ✅ 配置完成！"
else
    echo "   ⚠️  配置可能不完整，请手动运行："
    echo "      chmod +x his_desktop"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🎉 配置成功！现在可以运行："
echo ""
echo "  ./run_desktop.sh    # 桌面版（推荐）"
echo "  ./run_console.sh    # 控制台版"
echo ""
echo "演示账号："
echo "  PAT0001 / patient123  - 患者"
echo "  DOC0001 / doctor123   - 医生"
echo "  ADM0001 / admin123    - 管理员"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# 询问是否立即运行
read -p "是否立即运行桌面版？(y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🚀 启动桌面版..."
    ./run_desktop.sh
fi
