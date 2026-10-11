# Create from text

使用 **Tools → Create from text** 将演出技术需求清单中的文本转换为初始场景。用作演出文档前，请检查生成结果。

## 输入与操作流程

支持粘贴文本及 `.txt`、`.pdf` 文件；PDF 会先提取文本，再进行解析。

1. 打开 **Tools → Create from text**，粘贴文本或加载技术需求清单。
2. 选择 **Apply filter** 清理候选行，然后检查并编辑筛选后的文本。
3. 选择 **Create**。
4. 若灯具定义需要匹配，出现 **Resolve fixture types** 时，检查建议的匹配结果，选择 **Resolve and create** 确认方案并开始所选下载。

## 灯具类型匹配

已知类型使用当前灯具字典。对于未匹配的类型，可查看缓存的 GDTF Share 建议，用 **Use suggested** 接受可靠匹配，或用 **Search...** 选择配置文件。可在表格中修正类型，并取消勾选 **Create** 以跳过该行的灯具，同时保留清单中的其他元素。

打开检查界面无需登录。首次获取目录或下载时可能需要登录。确认后，接受的映射才会被记住并用于后续清单。创建前取消不会改变场景内容和字典映射。

### 通用替代方案

**Use generic** 提供仅适用于本次导入的替代方案，不保存占位映射。目录缺失、取消登录或网络故障时仍可使用 Generic 创建。可恢复的下载、验证、模式、认证或字典错误仅使受影响的类型使用替代方案；其余导入继续进行。

### DMX 模式

配置文件有多个模式时，应明确选择所需模式。双击 **Mode** 单元格查看选项。未选择模式的建议会使用 Generic 替代方案。确认的字典模式更改会在 **Resolve and create** 后保存。

## 生成内容与常用解析模式

根据文本内容，流程可创建灯具、桁架、葫芦/电机及屏幕等场景对象。屏幕或视频控制条目不会被当作灯具。

常用模式包括数量（`12 Spot`）、组合行（`8 Wash + 4 Beam`）、`LX1`、`FLOOR`、`SIDES` 等位置，以及桁架、吊杆或葫芦条目。Rider Import 的[偏好设置](preferences.md#导入)控制自动地址分配、图层组织和默认 LX 布置。

## 自动补全与验证

编辑器会建议常用关键词和本地字典术语。使用 Up/Down 浏览、Enter 或 Tab 接受、Esc 关闭建议。

创建后检查 Fixtures、Trusses、Hoists 和 Objects 表格、DMX 模式及地址分配。在 [2D 和 3D](views.md) 中核对位置，保存为 PSTG，并按需[导出 MVR](opening-mvr-files.md#导出-mvr)。

详细的解析和布置契约见[文本生成场景规则](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/text_to_scene_rules.md)。
