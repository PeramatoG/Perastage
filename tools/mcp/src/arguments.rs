use schemars::JsonSchema;
use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct InspectionArgs {
    /// Absolute or working-directory-relative path to the package to inspect.
    pub path: String,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct LiveArgs {
    /// Optional loopback live endpoint port. Omit to use the Perastage default.
    #[serde(default)]
    pub port: Option<u16>,
}

#[derive(Clone, Copy, Debug, Deserialize, JsonSchema, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum ObjectKind {
    Fixture,
    Truss,
    Support,
    SceneObject,
    Group,
}

#[derive(Clone, Copy, Debug, Deserialize, JsonSchema, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum SelectionObjectKind {
    Fixture,
    Truss,
    Support,
    SceneObject,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct ObjectGetArgs {
    /// Explicit kind of the scene object to read.
    pub kind: ObjectKind,
    /// Stable object UUID returned by live_objects_list or live_groups_list.
    #[schemars(length(min = 1))]
    pub uuid: String,
    /// Optional loopback live endpoint port.
    #[serde(default)]
    pub port: Option<u16>,
}

#[derive(Clone, Copy, Debug, Deserialize, JsonSchema, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum SelectionOperationKind {
    Add,
    Remove,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct SelectionObjectReference {
    /// Explicit selectable object kind, matching target_kind.
    pub kind: SelectionObjectKind,
    /// Stable object UUID returned by live_objects_list.
    #[schemars(length(min = 1))]
    pub uuid: String,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct SelectionOperation {
    /// Add or remove these explicit object references.
    pub kind: SelectionOperationKind,
    /// Ordered typed references. Repeated adds retain the existing order.
    pub objects: Vec<SelectionObjectReference>,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct SelectionUpdateArgs {
    /// Selection category to update. Other categories retain their selection.
    pub target_kind: SelectionObjectKind,
    /// False replaces this category before operations; true retains it.
    pub preserve_existing: bool,
    /// Ordered add/remove operations. Empty operations with false clear the category.
    pub operations: Vec<SelectionOperation>,
    /// Optional loopback live endpoint port.
    #[serde(default)]
    pub port: Option<u16>,
}

#[derive(Debug, Deserialize, JsonSchema, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Axis {
    X,
    Y,
    Z,
}

impl Axis {
    pub fn token(&self) -> &'static str {
        match self {
            Self::X => "x",
            Self::Y => "y",
            Self::Z => "z",
        }
    }
}

#[derive(Debug, Deserialize, JsonSchema, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum TransformSpace {
    World,
    Local,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct PositionArgs {
    /// Axis to transform.
    pub axis: Axis,
    /// Position value in millimetres.
    pub millimeters: f64,
    /// Whether the value is a delta instead of an absolute coordinate.
    pub relative: bool,
    /// World or object-local transform space.
    pub space: TransformSpace,
    /// Whether grouped targets transform around the group behavior.
    pub group: bool,
    /// Optional loopback live endpoint port.
    #[serde(default)]
    pub port: Option<u16>,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct RotationArgs {
    /// Axis to transform.
    pub axis: Axis,
    /// Rotation value in degrees.
    pub degrees: f64,
    /// Whether the value is a delta instead of an absolute angle.
    pub relative: bool,
    /// World or object-local transform space.
    pub space: TransformSpace,
    /// Whether grouped targets rotate as a group.
    pub group: bool,
    /// Optional loopback live endpoint port.
    #[serde(default)]
    pub port: Option<u16>,
}

#[derive(Clone, Copy, Debug, Deserialize, JsonSchema, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum TransformComponentKind {
    Position,
    Rotation,
}

#[derive(Clone, Copy, Debug, Deserialize, JsonSchema, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum TransformMode {
    Absolute,
    Relative,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct TransformComponent {
    /// Position or rotation component to apply in request order.
    pub kind: TransformComponentKind,
    /// Explicit component axis.
    pub axis: Axis,
    /// Position in millimetres or rotation in degrees. Must be finite.
    pub value: f64,
    /// Absolute component or relative delta.
    pub mode: TransformMode,
    /// World or object-local axes; local axes affect relative operations.
    pub space: TransformSpace,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct TransformTarget {
    /// Explicit transformable kind. Children are never promoted to a group.
    pub kind: ObjectKind,
    /// Stable UUID from live_objects_list or live_groups_list.
    #[schemars(length(min = 1))]
    pub uuid: String,
    /// Non-empty ordered components for this exact target.
    #[schemars(length(min = 1))]
    pub components: Vec<TransformComponent>,
}

#[derive(Debug, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct BatchTransformArgs {
    /// Non-empty ordered targets. Repeated UUIDs retain request order.
    #[schemars(length(min = 1))]
    pub targets: Vec<TransformTarget>,
    /// Optional loopback live endpoint port.
    #[serde(default)]
    pub port: Option<u16>,
}
