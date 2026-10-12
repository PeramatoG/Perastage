# 快捷键与命令栏

本页汇总内置帮助中常用的键盘、鼠标和命令栏操作，便于在文档界面中快速查阅。

## 场景编辑

- `Ctrl+X`: 剪切所选灯具、桁架、葫芦和场景对象。
- `Ctrl+C`: 复制所选场景元素。
- `Ctrl+V`: 将复制的场景元素粘贴到当前项目。

这些命令支持混合选择，也可从 **Edit** 菜单使用。编辑文本字段或表格单元格时，这些按键保留原有的文本编辑功能。

## 全局快捷键

| 快捷键 | 操作 |
| --- | --- |
| `Ctrl+N` | 新建项目 |
| `Ctrl+L` | 加载项目 |
| `Ctrl+S` | 保存项目 |
| `Ctrl+Q` | 关闭应用 |
| `Ctrl+Z / Ctrl+Y` | 撤销 / 重做 |
| `Del` | 删除所选对象 |
| `Ctrl+G` | 将当前跨表格选择分组 |
| `Ctrl+U` | 将所选对象移出其直接所属组 |
| `F1` | 打开帮助 |
| `Ctrl+Alt+D` / `Ctrl+Shift+D` | 沿桁架 / 在两点间分布灯具 |
| `P` / `R` | 在可编辑控件之外聚焦 Console 并预填 `pos ` / `rot ` |
| `F` | 聚焦 Console 并预填 `fixture `（可编辑控件之外） |
| `1 / 2 / 3 / 4` | 切换到 Fixtures / Trusses / Hoists / Objects |

## 查看器快捷键

| 快捷键 | 操作 |
| --- | --- |
| `Z` | 适配视图（当前聚焦的 2D/3D 查看器） |
| `NumPad1` | 前视图 |
| `NumPad3` | 侧视图/右视图 |
| `NumPad7` | 俯视图 |
| `NumPad5` | 重置 3D 摄像机朝向 |
| `Arrow keys` (3D) | 环绕摄像机 |
| `Shift + Arrow keys` (3D) | 平移摄像机 |
| `Alt + Arrow keys` (2D/3D) | 放大/缩小 |
| `Arrow keys` (2D) | 平移视图 |

## Layout View 快捷键

| 快捷键 | 操作 |
| --- | --- |
| `Delete` | 删除所选版面元素 |
| `Z` | 适配版面视图 |

## Console 输入快捷键

| 快捷键 | 操作 |
| --- | --- |
| `Esc` | 退出输入提示并重新启用应用快捷键 |
| `Up / Down` | 浏览命令历史 |
| `Home` | 移至输入起始处（提示符之后） |
| `Left / Backspace` | 不能移至提示符之前 |

## 鼠标操作（3D 查看器）

| 操作 | 结果 |
| --- | --- |
| 左键拖动 | 环绕摄像机 |
| Shift + 左键拖动或中键拖动 | 平移摄像机 |
| 鼠标滚轮 | 放大/缩小 |
| 左键单击 | 选择光标下的灯具/桁架/对象 |
| Shift/Ctrl + 左键单击 | 切换选择状态 |
| Ctrl + 左键拖动 | 框选 |
| 双击灯具标签 | 打开灯具地址分配对话框 |

## 命令栏快速操作

1. 聚焦命令栏。
2. 在可编辑控件之外，使用 `P`、`R` 或 `F` 聚焦 Console 并预填命令。
3. 编辑命令文本。
4. 执行并在表格和查看器中核对结果。
5. 重复修改时复用命令历史。

## 实验性 CLI

从 1.7.0 开始，Perastage 包含实验性 `perastage-cli`，用于自动化和高级工作流程。
请在终端中运行：

| 平台 | 示例 |
| --- | --- |
| Windows（PowerShell，默认安装位置） | `& "C:\Program Files\Perastage\perastage-cli.exe" --help` |
| macOS（应用安装在 Applications 中） | `/Applications/Perastage.app/Contents/MacOS/perastage-cli --help` |
| Linux AppImage（请替换为下载的文件名） | `./Perastage-1.7.0-x86_64.AppImage --cli --help` |
| Arch Linux | `perastage-cli --help` |

使用 `--version` 检查已安装的版本。Windows 和 macOS 不会将 CLI 添加到 PATH。
命令和选项保持英文。`inspect` 只读取文件，不修改文件；`scene` 必须指定单独的输出文件，
绝不会覆盖源文件；`live` 仅通过回环地址连接正在运行的本地 Perastage 实例。
实验性描述的是公开支持和稳定性的成熟程度，现有的验证和原子性规则仍然适用。
MCP 适配器需要单独配置，不包含在安装包中。详细语法请参阅
[Developer CLI](../../../developer/cli.md)。

<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->

## Console 常用命令

### 选择命令

| 命令 | 说明 |
| --- | --- |
| `clear` | 清除灯具、桁架和对象选择 |
| `f ...` | 按 ID 选择灯具 |
| `t ...` | 按单元编号选择桁架（先清除桁架选择） |

选择语法示例：

- 单个 ID： `f 12`
- 范围： `f 1-5`, `f 1 thru 5`, `f 1 t 5`
- 添加/移除： `f + 10 - 3`
- 混合： `f 1 3 5 7-9`

### 位置与旋转命令

| 命令 | 说明 |
| --- | --- |
| `pos x <values>` | 设置 X 位置 |
| `pos y <values>` | 设置 Y 位置 |
| `pos z <values>` | 设置 Z 位置 |
| `pos <x>,<y>,<z>` | 同时设置 X/Y/Z |
| `x <values>` / `y <values>` / `z <values>` | `pos` 的轴快捷命令 |
| `rot x <values>` / `rot y <values>` / `rot z <values>` | 设置滚转/俯仰/偏航角 |
| `rot x\|y\|z <values> --group` 或 `--g` | 将整个选择作为一组旋转 |

说明：

- Console 的位置值和枢轴坐标使用米，旋转值使用度。
- 单个值应用于所有所选元素。
- 两个值将所选元素从起点到终点线性分布；`t` 和 `thru` 可作为范围分隔符，因此 `pos x -7 t 7`、`pos x -7 thru 7` 和 `pos x -7 7` 等效。
- 使用 `++` / `--` 表示相对偏移。数值与符号之间有无空格均可，例如 `pos x ++1.5`、`pos x ++ 1.5`、`pos z --1` 和 `pos z -- 1`。
- 组移动遵循[选择与移动偏好](preferences.md#选择与移动)。
- 组旋转的默认枢轴为所选对象包围盒的中心。
- 可在末尾添加 `x,y,z` 三元组指定枢轴，例如 `rot y ++45 --g -2.5,0,0`。

### 变换空间

命令栏变换默认使用世界坐标轴。相对位置和旋转命令可通过 `--local` 或 `-l` 使用局部轴：

```text
f 1
pos x ++ 1
pos x ++ 1 --local
rot y ++ 45
rot y ++ 45 -l
rot z ++ 30 --group --local
```

变换空间修饰符影响增量操作（`++` 和 `--`）。绝对位置命令仍设置世界坐标，绝对旋转命令保留原有欧拉角行为。工具栏的 **Local Axes** 按钮会保留状态，但只影响视口变换；命令栏历史和脚本保持确定性，不继承该按钮状态。

<!-- PERASTAGE_CONSOLE_HELP_END -->

## 焦点与优先级

- 在可编辑文本字段或单元格中输入时，快捷键被禁用。
- 查看器快捷键作用于当前聚焦的查看器。
- 对话框快捷键仅作用于该对话框。

## Create from text 对话框快捷键

| 快捷键 | 操作 |
| --- | --- |
| `↑` / `↓` | 浏览建议 |
| `Enter` 或 `Tab` | 接受所选建议 |
| `Esc` | 关闭建议但不关闭对话框 |

页面编辑和版面鼠标操作见 [Layouts 与 PDF](layouts.md)。
