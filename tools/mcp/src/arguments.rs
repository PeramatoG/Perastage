use schemars::JsonSchema;
use serde::Deserialize;

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

#[derive(Debug, Deserialize, JsonSchema)]
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

#[derive(Debug, Deserialize, JsonSchema)]
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
