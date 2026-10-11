# Layouts 与 PDF

版面将场景视图和辅助信息组织成可打印的页面。

## 准备页面

1. 打开 **View → Layout Views → Layout Mode View**。必要时从 **View** 显示 **Layouts** 面板。
2. 选择版面，或用 **Add** 新建。**Rename** 和 **Delete** 用于管理页面；最后一个版面不能删除。
3. 使用版面的右键菜单选择方向。
4. 从 Layout 工具栏添加元素：

| 操作 | 内容 |
| --- | --- |
| **Add 2D View** | 2D 场景视图 |
| **Add Legend** | 灯具图例 |
| **Add Event Table** | 活动信息 |
| **Add Text** | 注释和标题 |
| **Add Image** | 标志等图像 |

编辑各元素的可用设置，选择内容和显示方式。生成文档前，应在[视图和表格](views.md)中检查场景数据。

## 排列与编辑

拖动边框以移动，拖动边框控制点以调整大小。双击边框以编辑元素。右键提供编辑、删除和堆叠顺序控制。拖动空白区域以平移，滚轮以缩放。**Z** 将版面适配到视图；**Delete** 删除所选版面元素。

保存 `.pstg` 项目以保留版面。**Export template** 创建包含引用图像的便携 `.pslayout` 包。**Import template** 接受 `.pslayout` 和旧版 `.json` 模板。

## 导出 PDF

选择有内容的版面并使用 **File → Print Layout...**。选择可写的 `.pdf` 目标位置。PDF 使用所选版面的纸张尺寸和方向，并包含支持的元素。打印或分享前请检查导出的文件。

视图缺失、输出错误或导出失败时，请参阅[故障排查](troubleshooting.md#版面与-pdf-问题)。
