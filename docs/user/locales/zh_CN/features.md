# 功能概览

Perastage 是开源桌面工具，用于灯光和吊装场景设计及技术文档制作。

## MVR 项目流程

[打开、合并和导出 MVR](opening-mvr-files.md)，检查冲突，并将进行中的工作保存为 PSTG 项目。

## GDTF 与 GDTF Share

使用灯具定义和本地字典，[搜索和下载 GDTF](gdtf-download.md)，离线浏览缓存目录并检查修订和 DMX 模式。灯具编辑器按 Model 检查并保留原始 GDTF 资源。

## MVR / GDTF Inspector

[检查](inspector.md)项目快照和外部包而不编辑。浏览场景树、包树、灯具定义、XML、预览和已报告问题。

## 2D 和 3D 显示

[检查平面图和空间关系](views.md)，选择渲染风格，使用移动、轴和吸附控制。大型场景可调整模型细节。

## 场景编辑、表格、地址分配和分组

编辑灯具、桁架、葫芦和对象，检查元数据和地址分配，组织图层和组。使用表格编辑、连续放置、Undo/Redo 和内置 [Console](shortcuts-and-command-bar.md)。

## Create from text

从粘贴文本或 TXT/PDF 技术需求清单[生成初始场景](create-from-text.md)，检查灯具类型匹配，选择模式，或使用 Generic 替代方案继续。

## Layouts 与 PDF 文档

使用 2D 视图、图例、活动表格、文本和图像[制作打印页面](layouts.md)。分享模板并将版面导出为 PDF。

## 实验性本地集成

OSC 和 [MCP](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/mcp_adapter.md) 提供实验性本地集成，用于单独配置的自动化。技术契约见[开发者文档](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/index.md)。

## 平台与源码

Perastage 支持 Windows、macOS 和 Linux，采用 GNU GPL 第 3 版或更高版本许可。软件包见[安装](installation.md)，源码和贡献说明见[代码仓库](https://github.com/PeramatoG/Perastage)。

## 手动 DMX 配接

手动编辑单个灯具时，只要 Universe/Channel 起始地址有效（通道为 1–512），就会保留该地址，即使主 GDTF 通道占用超出当前宇宙。红色 Channel 单元格的悬停提示会分别说明溢出、未知通道占用或重叠。例如，1.400 起始的 180 通道在假设连续 Auto Span 路由下会占用 1.400–1.512 和 2.1–2.67。这些范围仅供参考：标准 MVR 保存起始地址，并不保证该路由，接收或控制应用必须自行支持。次级 DMX breaks 保持不变。AutoPatch 和多个灯具的顺序分配仍采用原有的跨宇宙分配规则。

## DMX Patch 查看器

通过 **视图 → DMX Patch** 在当前 2D/3D 视口下方查看任意 Universe 的 512 个通道。紧凑单元格和列数会自动适应面板宽度，也可输入编号查看空 Universe。颜色遵循灯具显示颜色，标签显示 GDTF 物理通道属性；悬停提示列出所有占用灯具和 Break。斜线单元格表示重叠。诊断列表说明缺失定义、分配和溢出；超过 512 的通道不会自动延续到下一个 Universe。此查看器为只读。关闭后可恢复视口空间，并会记住可见状态和高度。在 Layout 和 Inspector 模式中会暂时隐藏。 可通过标题栏按钮关闭或最大化面板。警告图标会打开可滚动的诊断列表，替代冗长的悬停提示。

每个灯具名称仅显示在首行；居中的通道编号、首通道边框和圆角标识其起止位置。对于有歧义的定义，默认启用 **声明偏移**，相对于主起始地址预览暂定字节，以虚线边框、问号及独立计数区分已确认的占用和冲突。此选项不会修复 Break、改写文件或将通道延续到其他 Universe。 选择和悬停轮廓与灯具表格及视图保持同步，沿用表格的青色选择和绿色悬停颜色，不改变灯具填充颜色。
