# 偏好设置

打开 **Edit → Preferences**。切换类别会保留待保存的修改。**Apply** 保存但不关闭，**OK** 保存并关闭，**Cancel** 放弃上次 Apply 之后的修改。

## 常规

- **Language:**选择界面语言，重启 Perastage 后生效。
- **Units:**选择距离和重量单位。单位更改立即生效。
- **Updates:** **Check on startup (recommended)** 最多每 24 小时检查一次。可关闭特定版本的提醒。**Manual only** 禁用启动检查；仍可通过 **Help → Check for Updates** 手动检查。

### 缓存

**常规 → 缓存** 显示只读的缓存位置和当前磁盘占用。
**打开缓存文件夹** 使用系统文件管理器打开文件夹。
**清除缓存...** 要求确认后立即删除 Perastage 可重新生成的缓存，不受应用、确定或取消按钮影响。
几何数据在需要时重新生成；联网后 GDTF 目录会重新下载。
配置、日志、凭据、库、GDTF 文件、字典和项目均会保留。
如果部分文件无法删除，Perastage 会报告失败并更新显示的大小。
持久缓存位于用户应用数据目录下的 `cache/` 中，不再写入源文件旁边。

## 导入

**Rider Import** 控制 **Auto patch after import**、按位置或灯具类型自动创建图层，以及默认 LX 高度、位置和边距。距离字段使用所选单位。参阅 [Create from text](create-from-text.md)。

## 查看器

### 3D Viewer

选择渲染显示方式及 **Performance → Model detail**：

- **Standard**（默认）兼顾细节和性能。
- **Low** 对复杂模型进行更大程度的简化。
- **High** 显示原始模型的全部可用细节。

**Use a simplified proxy while navigating** 是独立选项，默认关闭。快速导航时会暂时简化未选中的灯具，随后恢复所选细节级别。更改在下一次重绘时生效，无需重新打开项目。源几何体、GDTF 资源、2D 符号和导出几何体保持不变。

### 选择与移动

选择哪些对象类型在鼠标、Console 和 Magnet 变换时移动其所属的根组。已分组桁架默认移动整个组；灯具、支撑/葫芦和场景对象独立移动。直接选择组会移动整个组。表格编辑只影响被编辑的对象。

**Show anchor references while moving or inserting elements** 在移动或插入时显示兼容的 Magnet 锚点。这些辅助标记不属于场景或导出内容。更改移动偏好不会改变项目层级，也不会将项目标记为已编辑。

## 格式

### GDTF 资源

GDTF 偏好设置管理 Share 登录凭据。Perastage 自动将整灯的
Top/Front/Side/Bottom 符号保存到 PSTG。保存项目即可保留生成的符号和用户覆盖；
这些符号不会成为 GDTF 或 MVR 资源。灯具编辑器按 Model 检查原始 SVG，
并保留其字节和插入偏移。仍可明确编辑物理属性。

### MVR 导入 / 导出

**Truss geometry export mode** 提供：

- **Standard MVR representation**（默认）：尽可能保留导入的符号引用。
- **Direct Geometry3D for truss symbols:**为需要直接几何体的接收应用展开桁架符号引用。

两者都生成兼容 MVR 1.6 的输出。项目保存使用标准表示方式。导入选项见[打开 MVR 文件](opening-mvr-files.md)。
