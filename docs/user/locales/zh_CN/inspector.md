# MVR / GDTF Inspector

打开 **View → Layout Views → MVR / GDTF Inspector**，检查当前项目的快照。检查为只读操作：不修改项目或被检查的文件，不改变项目选择，也不将项目标记为已编辑。

## 选择检查对象

- 当前项目视图是快照，不会持续更新。编辑后使用 **Refresh** 获取最新项目状态。
- 使用 **Open external...** 检查外部 `.mvr` 或 `.gdtf`，无需导入。

## 浏览 MVR 和 GDTF 内容

对于 MVR，可在包树中检查归档条目，或在场景树中查看场景对象及其名称、类型和 UUID。激活嵌入的 GDTF 可打开其灯具定义，再返回上级 MVR。

对于 GDTF，可检查 FixtureType 标识、物理信息和修订版本。选择 DMX 模式可查看通道、功能和通道功能集。在详情页中检查色轮、槽位、滤镜和资源引用。

## 源文本与预览

选择包资源以查看源文本或预览。XML 和纯文本显示在 **Source** 中；SVG 提供 XML 和渲染预览。支持的图像及 GLB/3DS 模型显示在 **Preview** 中。不支持或过大的资源仍会列出，并显示预览状态。

使用搜索、折叠和复制控件查看 XML。大型文档可能先显示有限内容，再提供明确的操作以加载完整文本。拖动窗格分隔线，为导航、源文本、详情或预览分配更多空间。

## 问题与诊断

**Issues** 汇总已报告的问题；**Diagnostics** 提供调查所需的更多细节。预览受限并不意味着文件无效。Inspector 不修复包，也不下载缺失资源。请通过常规编辑流程修正，或在报告问题时[导出诊断报告](troubleshooting.md#日志与诊断报告)。

## 灯具编辑器预览

Preview 默认显示 **All fixture**。选择 Geometry 可在 Solo 模式查看其子树；
同一 Model 的重复实例仍是独立选项。下方缩略图支持 GDTF 根目录中的标准 PNG 和 SVG。

在 Symbols 中选择 **Standard GDTF** 的 Model，查看其准确的 Top/Front/Side
资源、尺寸和 PrimitiveType。提示显示路径、插入偏移和诊断。资源存在不代表支持预览。
独立的 **Perastage project (PSTG)** 组显示整灯的 Top/Front/Side/Bottom，
并区分用户覆盖的来源。检查预览及生成项目符号均保留原始 GDTF 资源。
