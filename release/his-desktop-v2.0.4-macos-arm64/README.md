# HIS Desktop v2.0.4 - macOS ARM64

轻量级医院信息系统桌面版

## 版本更新

### v2.0.4 (2026-03-30)

**修复**
- 修复中文字符显示为乱码（????）的问题
- 修复 macOS Retina 显示屏上鼠标指针漂移问题
- 改善按钮点击响应的准确性

## 系统要求

- macOS 11.0 (Big Sur) 或更高版本
- Apple Silicon (M1/M2/M3) 处理器

## 安装说明

1. 解压下载的压缩包
2. 双击运行 `his_desktop` 可执行文件
3. 如果遇到"无法打开，因为它来自身份不明的开发者"提示：
   - 打开"系统偏好设置" > "安全性与隐私"
   - 点击"仍要打开"按钮
   - 或者在终端中运行：`xattr -cr his_desktop`

## 使用说明

应用程序会自动加载 `data` 目录中的数据文件。首次运行时可以使用以下测试账号登录：

- 患者账号：P001 / password
- 医生账号：D001 / password
- 管理员账号：admin / admin

## 数据文件

`data` 目录包含以下数据文件：
- users.txt - 用户账号
- patients.txt - 患者信息
- doctors.txt - 医生信息
- departments.txt - 科室信息
- registrations.txt - 挂号记录
- visits.txt - 看诊记录
- medicines.txt - 药品信息
- dispense_records.txt - 发药记录
- beds.txt - 床位信息
- wards.txt - 病区信息
- admissions.txt - 住院记录

## 技术支持

如有问题，请访问：https://github.com/ymylive/his/issues
