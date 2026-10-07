use super::tests::server;
use super::*;
use serde_json::{Value, json};

fn batch_input(value: Value) -> BatchTransformArgs {
    serde_json::from_value(value).unwrap()
}

fn component() -> Value {
    json!({"kind": "position", "axis": "x", "value": 1250.0,
           "mode": "absolute", "space": "world"})
}

fn request() -> Value {
    json!({"targets": [{"kind": "fixture", "uuid": "fixture-a", "components": [component()]}]})
}

#[test]
fn batch_mapping_flattens_mixed_components_and_repeated_targets_in_order() {
    let (server, calls) = server(true);
    let input = batch_input(json!({
        "targets": [
            {"kind": "fixture", "uuid": "a", "components": [
                {"kind": "position", "axis": "x", "value": 1000.0, "mode": "absolute", "space": "world"},
                {"kind": "position", "axis": "y", "value": -250.0, "mode": "relative", "space": "local"},
                {"kind": "rotation", "axis": "z", "value": 45.0, "mode": "relative", "space": "world"}
            ]},
            {"kind": "group", "uuid": "g", "components": [
                {"kind": "rotation", "axis": "y", "value": -90.0, "mode": "absolute", "space": "local"}
            ]},
            {"kind": "fixture", "uuid": "a", "components": [
                {"kind": "position", "axis": "z", "value": 4000.0, "mode": "absolute", "space": "world"}
            ]}
        ],
        "port": 5010
    }));
    server.live_batch_transform(Parameters(input)).unwrap();
    assert_eq!(
        *calls.lock().unwrap(),
        [args(&[
            "live",
            "execute",
            "scene.transform.batch",
            "--args",
            r#"{"target_kinds":["fixture","fixture","fixture","group","fixture"],"target_uuids":["a","a","a","g","a"],"component_kinds":["position","position","rotation","rotation","position"],"axes":["x","y","z","y","z"],"values":[1000.0,-250.0,45.0,-90.0,4000.0],"modes":["absolute","relative","relative","absolute","absolute"],"spaces":["world","local","world","local","world"]}"#,
            "--port",
            "5010",
        ])]
    );
}

#[test]
fn batch_mapping_accepts_all_explicit_kinds_and_keeps_uuid_text_in_one_argv() {
    let (server, calls) = server(true);
    for kind in ["fixture", "truss", "support", "scene_object", "group"] {
        let input = batch_input(json!({
            "targets": [{"kind": kind, "uuid": "quote\"\\\n$(object)", "components": [component()]}]
        }));
        server.live_batch_transform(Parameters(input)).unwrap();
        assert_eq!(
            calls.lock().unwrap().last().unwrap(),
            &args(&[
                "live",
                "execute",
                "scene.transform.batch",
                "--args",
                &format!(
                    r#"{{"target_kinds":["{kind}"],"target_uuids":["quote\"\\\n$(object)"],"component_kinds":["position"],"axes":["x"],"values":[1250.0],"modes":["absolute"],"spaces":["world"]}}"#,
                ),
            ])
        );
    }
    assert_eq!(calls.lock().unwrap().len(), 5);
}

#[test]
fn batch_schema_requires_explicit_targets_and_components() {
    let tool = PerastageMcp::tool_router()
        .list_all()
        .into_iter()
        .find(|tool| tool.name == "live_batch_transform")
        .unwrap();
    let schema = serde_json::to_value(tool.input_schema).unwrap();
    assert_eq!(schema["additionalProperties"], false);
    assert_eq!(schema["required"], json!(["targets"]));
    assert_eq!(schema["properties"]["targets"]["minItems"], 1);
    let definitions = &schema["$defs"];
    assert_eq!(
        definitions["TransformTarget"]["required"],
        json!(["kind", "uuid", "components"])
    );
    assert_eq!(
        definitions["TransformTarget"]["additionalProperties"],
        false
    );
    assert_eq!(
        definitions["TransformTarget"]["properties"]["uuid"]["minLength"],
        1
    );
    assert_eq!(
        definitions["TransformTarget"]["properties"]["components"]["minItems"],
        1
    );
    assert_eq!(
        definitions["TransformComponent"]["required"],
        json!(["kind", "axis", "value", "mode", "space"])
    );
    assert_eq!(
        definitions["TransformComponent"]["additionalProperties"],
        false
    );
    for (name, choices) in [
        (
            "ObjectKind",
            json!(["fixture", "truss", "support", "scene_object", "group"]),
        ),
        ("TransformComponentKind", json!(["position", "rotation"])),
        ("Axis", json!(["x", "y", "z"])),
        ("TransformMode", json!(["absolute", "relative"])),
        ("TransformSpace", json!(["world", "local"])),
    ] {
        assert_eq!(definitions[name]["enum"], choices);
    }
}

#[test]
fn malformed_batch_shapes_and_enum_tokens_are_rejected() {
    let (server, calls) = server(true);
    for invalid in [
        json!({}),
        json!({"targets": {}, "command": "clear"}),
        json!({"targets": [], "command": "clear"}),
        json!({"targets": [{"kind": "layer", "uuid": "a", "components": [component()]}]}),
        json!({"targets": [{"kind": "fixture", "components": [component()]}]}),
        json!({"targets": [{"kind": "fixture", "uuid": "a"}]}),
        json!({"targets": [{"kind": "fixture", "uuid": "a", "components": [component()], "group": true}]}),
    ] {
        assert!(serde_json::from_value::<BatchTransformArgs>(invalid).is_err());
    }
    for (field, invalid) in [
        ("kind", json!("scale")),
        ("axis", json!("w")),
        ("value", json!("1000")),
        ("mode", json!("incremental")),
        ("space", json!("viewport")),
        ("command", json!("clear")),
    ] {
        let mut value = request();
        value["targets"][0]["components"][0][field] = invalid;
        assert!(serde_json::from_value::<BatchTransformArgs>(value).is_err());
    }
    for field in ["kind", "axis", "value", "mode", "space"] {
        let mut value = request();
        value["targets"][0]["components"][0]
            .as_object_mut()
            .unwrap()
            .remove(field);
        assert!(serde_json::from_value::<BatchTransformArgs>(value).is_err());
    }
    assert!(calls.lock().unwrap().is_empty());
    server
        .live_batch_transform(Parameters(batch_input(request())))
        .unwrap();
    assert_eq!(calls.lock().unwrap().len(), 1);
}

#[test]
fn empty_batches_targets_and_nonfinite_values_never_invoke_the_cli() {
    let (server, calls) = server(true);
    for invalid in [
        json!({"targets": []}),
        json!({"targets": [{"kind": "fixture", "uuid": "", "components": [component()]}]}),
        json!({"targets": [{"kind": "fixture", "uuid": "a", "components": []}]}),
        json!({"targets": [
            {"kind": "fixture", "uuid": "a", "components": [component()]},
            {"kind": "truss", "uuid": "", "components": [component()]}
        ]}),
    ] {
        assert!(
            server
                .live_batch_transform(Parameters(batch_input(invalid)))
                .is_err()
        );
    }
    for value in [f64::NAN, f64::INFINITY, f64::NEG_INFINITY] {
        let mut input = batch_input(request());
        input.targets[0]
            .components
            .push(serde_json::from_value(component()).unwrap());
        input.targets[0].components[1].value = value;
        assert!(server.live_batch_transform(Parameters(input)).is_err());
    }
    assert!(calls.lock().unwrap().is_empty());
}

#[test]
fn batch_cli_diagnostics_and_results_are_preserved() {
    for success in [true, false] {
        let (server, calls) = server(success);
        let result = server
            .live_batch_transform(Parameters(batch_input(json!({
                "targets": [{"kind": "truss", "uuid": "missing-or-mismatched", "components": [component()]}]
            }))))
            .unwrap();
        assert_eq!(result.is_error, Some(!success));
        let structured = result.structured_content.unwrap();
        assert_eq!(structured["success"], success);
        assert_eq!(structured["result"]["ok"], success);
        assert_eq!(structured["result"]["diagnostics"][0]["code"], "test");
        assert_eq!(structured["exit_code"], if success { 0 } else { 4 });
        assert_eq!(
            structured["stderr"],
            if success { "" } else { "live unavailable" }
        );
        assert_eq!(calls.lock().unwrap().len(), 1);
    }
}
