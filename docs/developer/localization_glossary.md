# Localization glossary

This glossary records preferred terminology for recurring Perastage UI strings. It guides translators and reviewers; gettext catalogs remain the source of translated application text.

| English term | Spanish guidance | Notes |
| --- | --- | --- |
| MVR | MVR | Official standard name; do not translate. |
| GDTF | GDTF | Official standard name; do not translate. |
| DMX | DMX | Protocol name; do not translate. |
| Art-Net | Art-Net | Protocol name; do not translate. |
| UUID | UUID | Technical identifier; do not translate. |
| XML | XML | Technical format; do not translate. |
| Geometry3D | Geometry3D | GDTF-defined name; do not translate. |
| Symbol | Symbol | Preserve when referring to official MVR/GDTF concepts; use `símbolo` only for generic UI prose. |
| Symdef | Symdef | Specification-defined term; do not translate. |
| Gobo | Gobo | Standard lighting term in Spanish workflows. |
| Pan | Pan | Common lighting-control term; do not translate in channel/function names. |
| Tilt | Tilt | Common lighting-control term; do not translate in channel/function names. |
| Truss | truss | Common entertainment-rigging term in this project. |
| Rigging | rigging | Keep as professional stage-rigging term unless prose clearly calls for `aparejo`. |
| Patch | patch | Keep for DMX patching workflows; use `autopatchear` for Auto patch. |
| Fixture | aparato | Preferred UI term for lighting fixtures. |
| Hoist | motor | Preferred UI term for chain hoists where context is rigging. |
| Layout | layout | Retained because it is already established in the Perastage UI. |
| Viewport | visor | Preferred UI term for interactive viewports. |

## Simplified Chinese guidance

| English term | Simplified Chinese guidance | Notes |
| --- | --- | --- |
| MVR / GDTF / DMX / Art-Net / UUID / XML | Preserve in English | Official protocol, format, or identifier names. |
| Symbol / Symdef | 符号 / Symdef | Preserve `Symdef` when it is the schema concept. |
| Gobo | 图案片 | Keep `Gobo` unchanged where it is an official name or imported value. |
| Pan / Tilt | 水平旋转 / 垂直旋转 | Preserve English in channel and function identifiers. |
| Truss | 桁架 | Entertainment-rigging structure. |
| Rigging | 吊挂 | Use for the general workflow; preserve imported names. |
| Patch / Auto patch | 配接 / 自动配接 | DMX address assignment UI. |
| Fixture | 灯具 | Lighting fixture. |
| Hoist | 葫芦 | Chain-hoist UI; never alter object names. |
| Layout | 布局 | Drawing/layout presentation. |
| Viewport | 视口 | Interactive 2D or 3D viewport. |

## French and German guidance

Use these terms in translated presentation and manual prose. Literal English UI
labels in manuals remain unchanged under the existing manual policy; imported
object names, filenames, config keys, channel/function names and machine syntax
always remain exact. The FR/DE translations are machine-assisted, with optional
native-speaker review welcome, particularly for rigging terminology.

| English term | French | German | Notes |
| --- | --- | --- | --- |
| Fixture | projecteur | Scheinwerfer | Lighting fixture; not an imported type name. |
| Truss | structure | Traverse | Entertainment-rigging structure. |
| Rigging | accroche | Rigging | Professional stage-rigging workflow. |
| Hoist | palan | Kettenzug | Chain hoist; preserve imported motor names. |
| Patch / Auto patch | patch / patch automatique | Patch / automatischer Patch | DMX address assignment. |
| Layout | mise en page | Layout | Printable page presentation. |
| Viewport | vue | Ansicht | Interactive scene viewport. |
| Symbol / Symdef | symbole / Symdef | Symbol / Symdef | Preserve official Symbol/Symdef schema names. |
| Gobo | gobo | Gobo | Preserve official or imported wheel/slot names. |
| Pan / Tilt | Pan / Tilt | Pan / Tilt | Keep channel/function identifiers; translate explanatory motion prose. |
| Inspector | Inspecteur | Inspektor | Read-only package viewer. |
| Preferences | Préférences | Einstellungen | UI presentation. |
| Model | modèle | Modell | Preserve Model when naming the GDTF schema concept. |
| Geometry | géométrie | Geometrie | Preserve Geometry/Geometry3D for schema references. |
| Dictionary | dictionnaire | Verzeichnis | Perastage's editable profile mapping collection. |

MVR, GDTF, DMX, Art-Net, sACN, OSC, MCP, UUID, XML, PSTG and GDTF Share
remain unchanged. Preserve schema names including FixtureType, PrimitiveType,
Revision, UserData, Symbol, Symdef and Geometry3D when used as identifiers.
