use crate::arguments::{
    Axis, BatchTransformArgs, ObjectKind, TransformComponentKind, TransformMode, TransformSpace,
};
use rmcp::ErrorData as McpError;
use serde::Serialize;

#[derive(Serialize)]
pub(crate) struct BatchTransformRequest<'a> {
    target_kinds: Vec<ObjectKind>,
    target_uuids: Vec<&'a str>,
    component_kinds: Vec<TransformComponentKind>,
    axes: Vec<&'a Axis>,
    values: Vec<f64>,
    modes: Vec<TransformMode>,
    spaces: Vec<&'a TransformSpace>,
}

pub(crate) fn flatten(input: &BatchTransformArgs) -> Result<BatchTransformRequest<'_>, McpError> {
    if input.targets.is_empty() {
        return Err(McpError::invalid_params(
            "Batch transforms require at least one target.",
            None,
        ));
    }
    let mut request = BatchTransformRequest {
        target_kinds: Vec::new(),
        target_uuids: Vec::new(),
        component_kinds: Vec::new(),
        axes: Vec::new(),
        values: Vec::new(),
        modes: Vec::new(),
        spaces: Vec::new(),
    };
    for target in &input.targets {
        if target.uuid.is_empty() {
            return Err(McpError::invalid_params(
                "Object UUID must not be empty.",
                None,
            ));
        }
        if target.components.is_empty() {
            return Err(McpError::invalid_params(
                "Every batch target requires at least one transform component.",
                None,
            ));
        }
        for component in &target.components {
            if !component.value.is_finite() {
                return Err(McpError::invalid_params(
                    "Transform value must be finite.",
                    None,
                ));
            }
            request.target_kinds.push(target.kind);
            request.target_uuids.push(&target.uuid);
            request.component_kinds.push(component.kind);
            request.axes.push(&component.axis);
            request.values.push(component.value);
            request.modes.push(component.mode);
            request.spaces.push(&component.space);
        }
    }
    Ok(request)
}
