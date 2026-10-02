use crate::arguments::{InspectionArgs, LiveArgs, PositionArgs, RotationArgs, TransformSpace};
use crate::backend::{CliBackend, args};
use rmcp::handler::server::wrapper::Parameters;
use rmcp::model::CallToolResult;
use rmcp::{ErrorData as McpError, tool, tool_router};
use serde_json::to_value;
use std::ffi::OsString;
use std::sync::Arc;

#[derive(Clone)]
pub struct PerastageMcp {
    backend: Arc<dyn CliBackend>,
}

impl PerastageMcp {
    pub fn new(backend: impl CliBackend) -> Self {
        Self {
            backend: Arc::new(backend),
        }
    }

    fn run(&self, arguments: Vec<OsString>) -> Result<CallToolResult, McpError> {
        let response = self.backend.invoke(&arguments);
        let structured = to_value(&response)
            .map_err(|error| McpError::internal_error(error.to_string(), None))?;
        Ok(if response.success {
            CallToolResult::structured(structured)
        } else {
            CallToolResult::structured_error(structured)
        })
    }

    fn live(
        &self,
        operation: &str,
        value: &str,
        port: Option<u16>,
    ) -> Result<CallToolResult, McpError> {
        let mut arguments = args(&["live", operation, value]);
        append_port(&mut arguments, port);
        self.run(arguments)
    }

    fn inspect(&self, path: String, extension: &str) -> Result<CallToolResult, McpError> {
        if !path.to_ascii_lowercase().ends_with(extension) {
            return Err(McpError::invalid_params(
                format!("Expected a {extension} package path."),
                None,
            ));
        }
        self.run(vec!["inspect".into(), path.into(), "--json".into()])
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::arguments::{Axis, TransformSpace};
    use crate::backend::CliResponse;
    use serde_json::json;
    use std::sync::{Arc, Mutex};

    #[derive(Clone)]
    struct RecordingBackend {
        calls: Arc<Mutex<Vec<Vec<OsString>>>>,
        response: CliResponse,
    }

    impl CliBackend for RecordingBackend {
        fn invoke(&self, arguments: &[OsString]) -> CliResponse {
            self.calls.lock().unwrap().push(arguments.to_vec());
            CliResponse {
                success: self.response.success,
                exit_code: self.response.exit_code,
                result: self.response.result.clone(),
                stderr: self.response.stderr.clone(),
            }
        }
    }

    fn server(success: bool) -> (PerastageMcp, Arc<Mutex<Vec<Vec<OsString>>>>) {
        let calls = Arc::new(Mutex::new(Vec::new()));
        let backend = RecordingBackend {
            calls: calls.clone(),
            response: CliResponse {
                success,
                exit_code: Some(if success { 0 } else { 4 }),
                result: json!({"ok": success, "diagnostics": [{"code": "test"}]}),
                stderr: if success {
                    String::new()
                } else {
                    "live unavailable".into()
                },
            },
        };
        (PerastageMcp::new(backend), calls)
    }

    #[test]
    fn discovery_lists_only_the_initial_typed_surface() {
        let (_server, _) = server(true);
        let tools = PerastageMcp::tool_router().list_all();
        let names: Vec<_> = tools.iter().map(|tool| tool.name.as_ref()).collect();
        assert_eq!(
            names,
            [
                "discover_capabilities",
                "inspect_gdtf",
                "inspect_mvr",
                "live_current_selection",
                "live_position_transform",
                "live_rotation_transform",
                "live_scene_summary",
                "live_selection_clear",
            ]
        );
        for tool in tools {
            let (read_only, destructive, idempotent) = match tool.name.as_ref() {
                "discover_capabilities"
                | "inspect_gdtf"
                | "inspect_mvr"
                | "live_current_selection"
                | "live_scene_summary" => (true, false, true),
                "live_selection_clear" => (false, true, true),
                "live_position_transform" | "live_rotation_transform" => (false, true, false),
                unexpected => panic!("unexpected MCP tool: {unexpected}"),
            };
            assert_eq!(
                tool.annotations.as_ref().unwrap().read_only_hint,
                Some(read_only)
            );
            assert_eq!(
                tool.annotations.as_ref().unwrap().destructive_hint,
                Some(destructive)
            );
            assert_eq!(
                tool.annotations.as_ref().unwrap().idempotent_hint,
                Some(idempotent)
            );
            assert_eq!(
                tool.annotations.as_ref().unwrap().open_world_hint,
                Some(false)
            );
            assert!(!tool.input_schema.contains_key("command"));
        }
        let position = PerastageMcp::tool_router()
            .list_all()
            .into_iter()
            .find(|tool| tool.name == "live_position_transform")
            .unwrap();
        let schema = serde_json::to_value(position.input_schema).unwrap();
        let required = schema["required"].as_array().unwrap();
        for field in ["axis", "millimeters", "relative", "space", "group"] {
            assert!(required.iter().any(|value| value == field));
        }
    }

    #[test]
    fn typed_requests_map_to_exact_cli_argv_without_a_shell() {
        let (server, calls) = server(true);
        server.discover_capabilities().unwrap();
        server
            .live_scene_summary(Parameters(LiveArgs { port: Some(5001) }))
            .unwrap();
        server
            .live_position_transform(Parameters(PositionArgs {
                axis: Axis::Y,
                millimeters: 1250.0,
                relative: true,
                space: TransformSpace::Local,
                group: true,
                port: None,
            }))
            .unwrap();
        server
            .live_rotation_transform(Parameters(RotationArgs {
                axis: Axis::Z,
                degrees: -45.0,
                relative: false,
                space: TransformSpace::World,
                group: false,
                port: None,
            }))
            .unwrap();
        assert_eq!(
            *calls.lock().unwrap(),
            [
                args(&["capabilities", "--json"]),
                args(&["live", "query", "scene.summary", "--port", "5001"]),
                args(&["live", "command", "pos y ++1.25 --local --group"]),
                args(&["live", "command", "rot z -45"]),
            ]
        );
    }

    #[test]
    fn structured_cli_failures_are_preserved_as_tool_errors() {
        let (server, _) = server(false);
        let result = server
            .live_current_selection(Parameters(LiveArgs { port: None }))
            .unwrap();
        assert_eq!(result.is_error, Some(true));
        let structured = result.structured_content.unwrap();
        assert_eq!(structured["exit_code"], 4);
        assert_eq!(structured["stderr"], "live unavailable");
        assert_eq!(structured["result"]["diagnostics"][0]["code"], "test");
    }

    #[test]
    fn structured_cli_success_is_preserved_as_a_successful_tool_result() {
        let (server, _) = server(true);
        let result = server
            .live_selection_clear(Parameters(LiveArgs { port: None }))
            .unwrap();
        assert_eq!(result.is_error, Some(false));
        let structured = result.structured_content.unwrap();
        assert_eq!(structured["success"], true);
        assert_eq!(structured["exit_code"], 0);
        assert_eq!(structured["result"]["ok"], true);
    }

    #[test]
    fn inspection_tools_reject_mismatched_package_types_before_execution() {
        let (server, calls) = server(true);
        assert!(server.inspect("scene.gdtf".into(), ".mvr").is_err());
        assert!(calls.lock().unwrap().is_empty());
        server.inspect("SCENE.MVR".into(), ".mvr").unwrap();
        assert_eq!(
            calls.lock().unwrap()[0],
            args(&["inspect", "SCENE.MVR", "--json"])
        );
    }
}

fn append_port(arguments: &mut Vec<OsString>, port: Option<u16>) {
    if let Some(port) = port {
        arguments.push("--port".into());
        arguments.push(port.to_string().into());
    }
}

fn transform_command(
    keyword: &str,
    axis: &str,
    value: f64,
    relative: bool,
    space: TransformSpace,
    group: bool,
) -> Result<String, McpError> {
    if !value.is_finite() {
        return Err(McpError::invalid_params(
            "Transform value must be finite.",
            None,
        ));
    }
    let mut command = format!(
        "{keyword} {axis} {}{value}",
        if relative { "++" } else { "" }
    );
    if matches!(space, TransformSpace::Local) {
        command.push_str(" --local");
    }
    if group {
        command.push_str(" --group");
    }
    Ok(command)
}

#[tool_router(server_handler)]
impl PerastageMcp {
    #[tool(
        description = "Discover Perastage semantic capabilities and frontend exposure.",
        annotations(
            read_only_hint = true,
            destructive_hint = false,
            idempotent_hint = true,
            open_world_hint = false
        )
    )]
    fn discover_capabilities(&self) -> Result<CallToolResult, McpError> {
        self.run(args(&["capabilities", "--json"]))
    }

    #[tool(
        description = "Inspect one MVR package and return its complete structured report.",
        annotations(
            read_only_hint = true,
            destructive_hint = false,
            idempotent_hint = true,
            open_world_hint = false
        )
    )]
    fn inspect_mvr(
        &self,
        Parameters(input): Parameters<InspectionArgs>,
    ) -> Result<CallToolResult, McpError> {
        self.inspect(input.path, ".mvr")
    }

    #[tool(
        description = "Inspect one GDTF package and return its complete structured report.",
        annotations(
            read_only_hint = true,
            destructive_hint = false,
            idempotent_hint = true,
            open_world_hint = false
        )
    )]
    fn inspect_gdtf(
        &self,
        Parameters(input): Parameters<InspectionArgs>,
    ) -> Result<CallToolResult, McpError> {
        self.inspect(input.path, ".gdtf")
    }

    #[tool(
        description = "Read the summary of the scene open in the local Perastage application.",
        annotations(
            read_only_hint = true,
            destructive_hint = false,
            idempotent_hint = true,
            open_world_hint = false
        )
    )]
    fn live_scene_summary(
        &self,
        Parameters(input): Parameters<LiveArgs>,
    ) -> Result<CallToolResult, McpError> {
        self.live("query", "scene.summary", input.port)
    }

    #[tool(
        description = "Read the current selection from the local Perastage application.",
        annotations(
            read_only_hint = true,
            destructive_hint = false,
            idempotent_hint = true,
            open_world_hint = false
        )
    )]
    fn live_current_selection(
        &self,
        Parameters(input): Parameters<LiveArgs>,
    ) -> Result<CallToolResult, McpError> {
        self.live("query", "scene.selection.get", input.port)
    }

    #[tool(
        description = "Clear the supported current selection categories in the local Perastage application.",
        annotations(
            read_only_hint = false,
            destructive_hint = true,
            idempotent_hint = true,
            open_world_hint = false
        )
    )]
    fn live_selection_clear(
        &self,
        Parameters(input): Parameters<LiveArgs>,
    ) -> Result<CallToolResult, McpError> {
        self.live("command", "clear", input.port)
    }

    #[tool(
        description = "Apply one explicit position component to the current live selection.",
        annotations(
            read_only_hint = false,
            destructive_hint = true,
            idempotent_hint = false,
            open_world_hint = false
        )
    )]
    fn live_position_transform(
        &self,
        Parameters(input): Parameters<PositionArgs>,
    ) -> Result<CallToolResult, McpError> {
        let axis = input.axis.token();
        let command = transform_command(
            "pos",
            axis,
            input.millimeters / 1000.0,
            input.relative,
            input.space,
            input.group,
        )?;
        self.live("command", &command, input.port)
    }

    #[tool(
        description = "Apply one explicit rotation component to the current live selection.",
        annotations(
            read_only_hint = false,
            destructive_hint = true,
            idempotent_hint = false,
            open_world_hint = false
        )
    )]
    fn live_rotation_transform(
        &self,
        Parameters(input): Parameters<RotationArgs>,
    ) -> Result<CallToolResult, McpError> {
        let axis = input.axis.token();
        let command = transform_command(
            "rot",
            axis,
            input.degrees,
            input.relative,
            input.space,
            input.group,
        )?;
        self.live("command", &command, input.port)
    }
}
