# 故障排查

## MVR 打开与导入问题

确认扩展名为 `.mvr`，尝试其他文件以定位问题，并查看 Console 中的导入消息。使用 [Inspector](inspector.md) 可检查外部包而不导入。合并冲突见[打开 MVR 文件](opening-mvr-files.md)。

## GDTF 灯具缺失或错误

检查灯具定义和所选 DMX 模式。使用 **Tools → Download GDTF** 获取配置文件，使用 **Tools → Edit dictionaries** 管理本地映射。修正缺失配置文件后，重新打开并检查场景。 在灯具编辑器中检查所选 Model；项目符号单独保存到 PSTG。

## 用户库

使用 **Tools → Open user library folder**；Windows 默认路径为 `%APPDATA%\Perastage\library\`。确认用户配置目录可写，替换文件前备份自定义内容。应编辑用户库文件，而不是安装的默认文件。

## 版面与 PDF 问题

选择有内容的版面，检查边框和方向，在 **File → Print Layout...** 前核对场景和表格内容。选择可写目标位置和非空文件名。检查导出的 PDF；流程见 [Layouts 与 PDF](layouts.md)。

## 项目与运行问题

尽可能另存一份项目，重启 Perastage 并重新打开。核对 2D、3D 和表格中的关键数据。 在灯具编辑器中检查所选 Model；项目符号单独保存到 PSTG。与小型项目比较，判断是否为特定场景的问题。

## GDTF Share 凭据

在线加载和下载需要账户和网络连接；[缓存目录浏览](gdtf-download.md#缓存与离线浏览)仍可离线使用。

如果 Perastage 提示运行时安全凭据存储不可用，密码可用于当前操作，但不会保留。检查操作系统的凭据存储是否可用；Linux 需要 GNOME Keyring 或 KWallet 等运行中的 Secret Service 提供程序。如果发布包中仍出现警告，请在报告中说明。不要提供密码。

## 日志与诊断报告

使用 **Help → Open Logs Folder** 查找 `perastage.log`、上次启动的 `perastage.previous.log` 和 `crash_reports` 子目录。

默认日志位置：

- Windows：`%LOCALAPPDATA%\Perastage\logs\`
- macOS：`~/Library/Logs/Perastage/`
- Linux：`${XDG_STATE_HOME}/perastage/logs/`；未设置该变量时为 `~/.local/state/perastage/logs/`。

使用 **Help → Export Diagnostic Report...** 生成文本报告，包含构建和平台详情、可用 OpenGL 信息及最近的日志行。报告保留在本地，不会自动上传。分享前请检查报告，并尽可能附上复现步骤和小型示例文件。

源码构建、依赖和工具链问题见[开发者构建指南](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md)。
