# 安装

从[官方网站](https://perastage.luismaperamato.com/#download)下载各平台的软件包并查看发行说明。如果元数据不可用，请访问 [GitHub 最新版本](https://github.com/PeramatoG/Perastage/releases/latest)。各版本提供的软件包可能不同。

## Windows

1. 下载官方 x64 安装程序并运行。
2. SmartScreen 可能提示应用无法识别。请核实文件来自官方发行版。若信任该文件，可在选项可用时选择 **More info → Run anyway**。
3. 完成安装，通过快捷方式启动 Perastage。

请勿禁用 Windows Defender 或 SmartScreen。

## macOS

1. macOS 15 用户选择 **macOS 15 — Apple Silicon (Compatibility Build)**；macOS 26 用户选择 **macOS 26 — Apple Silicon**。两者均为 ARM64，不支持 Intel Mac。
2. 打开 DMG，将 Perastage 拖入 Applications。
3. 尝试启动 Perastage。
4. 当前版本没有 Developer ID 签名，也未经 Apple 公证。如果 Gatekeeper 因无法验证开发者而阻止启动，且您信任官方下载，可在选项可用时使用 **System Settings → Privacy & Security → Open Anyway**。

参阅 [Apple 的安全打开应用说明](https://support.apple.com/en-us/102445)。请保持 Gatekeeper 启用。

### macOS 高级故障排除

某些 macOS 版本可能提示应用已损坏。首先从官方发行版重新下载。仅在核实官方来源并信任文件后，高级用户才可运行：

```bash
xattr -dr com.apple.quarantine /Applications/Perastage.app
```

此命令递归移除此应用的隔离属性，绕过针对它的基于隔离属性的 Gatekeeper 检查。它不会验证应用或修复损坏。不要对不可信下载使用此命令，也不要全局移除隔离属性。

## Linux AppImage

1. 下载 x86_64 AppImage。
2. 在下载目录中赋予执行权限：

```bash
chmod +x Perastage-*.AppImage
```

3. 打开 AppImage 启动 Perastage。通常无需安装。

## Arch Linux（实验性）

原生 x86_64 软件包仍属实验性。在下载的软件包所在目录中安装：

```bash
sudo pacman -U ./Perastage-*-arch-x86_64.pkg.tar.zst
```

然后从应用菜单启动 Perastage。

## 可编辑的用户库

使用 **Tools → Open user library folder** 查找可写的字典、灯具配置文件和其他库内容。Windows 的默认路径为 `%APPDATA%\Perastage\library\`。

请备份自定义内容。随软件提供的默认文件只补充用户库中缺失的文件，不覆盖已有文件；应编辑用户副本，而不是安装目录中的文件。

遇到启动、导入或内容缺失问题时，请参阅[故障排查](troubleshooting.md)。源码构建说明见[开发者构建指南](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md)。
