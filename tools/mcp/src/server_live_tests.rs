use super::tests::server;
use super::*;
use crate::arguments::{ObjectKind, SelectionObjectReference, SelectionOperation};
use serde_json::{Value, json};

#[test]
fn discovery_queries_map_to_exact_cli_argv() {
    let (server, calls) = server(true);
    server
        .live_objects_list(Parameters(LiveArgs { port: None }))
        .unwrap();
    server
        .live_layers_list(Parameters(LiveArgs { port: Some(5002) }))
        .unwrap();
    server
        .live_groups_list(Parameters(LiveArgs { port: None }))
        .unwrap();
    server
        .live_current_selection(Parameters(LiveArgs { port: Some(5003) }))
        .unwrap();
    assert_eq!(
        *calls.lock().unwrap(),
        [
            args(&["live", "query", "scene.objects.list"]),
            args(&["live", "query", "scene.layers.list", "--port", "5002"]),
            args(&["live", "query", "scene.groups.list"]),
            args(&["live", "query", "scene.selection.get", "--port", "5003"]),
        ]
    );
}

#[test]
fn object_lookup_maps_all_kinds_and_uuids_to_exact_structured_cli_argv() {
    let (server, calls) = server(true);
    for (kind, token) in [
        (ObjectKind::Fixture, "fixture"),
        (ObjectKind::Truss, "truss"),
        (ObjectKind::Support, "support"),
        (ObjectKind::SceneObject, "scene_object"),
        (ObjectKind::Group, "group"),
    ] {
        server
            .live_object_get(Parameters(ObjectGetArgs {
                kind,
                uuid: "object-uuid".into(),
                port: Some(5004),
            }))
            .unwrap();
        assert_eq!(
            calls.lock().unwrap().last().unwrap(),
            &args(&[
                "live",
                "query",
                "scene.object.get",
                "--args",
                &format!(r#"{{"kind":"{token}","uuid":"object-uuid"}}"#),
                "--port",
                "5004",
            ])
        );
    }
    server
        .live_object_get(Parameters(ObjectGetArgs {
            kind: ObjectKind::Fixture,
            uuid: "quote\"\\\n$(object)".into(),
            port: None,
        }))
        .unwrap();
    assert_eq!(
        calls.lock().unwrap().last().unwrap(),
        &args(&[
            "live",
            "query",
            "scene.object.get",
            "--args",
            r#"{"kind":"fixture","uuid":"quote\"\\\n$(object)"}"#,
        ])
    );
}

#[test]
fn selection_replace_add_and_remove_map_all_categories_to_exact_cli_requests() {
    let (server, calls) = server(true);
    for kind in ["fixture", "truss", "support", "scene_object"] {
        for (preserve_existing, operation) in [(false, "add"), (true, "add"), (true, "remove")] {
            let input: SelectionUpdateArgs = serde_json::from_value(json!({
                "target_kind": kind,
                "preserve_existing": preserve_existing,
                "operations": [{"kind": operation, "objects": [{"kind": kind, "uuid": "object-uuid"}]}],
                "port": 5005
            }))
            .unwrap();
            server.live_selection_update(Parameters(input)).unwrap();
            let serialized = format!(
                r#"{{"target_kind":"{kind}","preserve_existing":{preserve_existing},"operation_kinds":["{operation}"],"object_kinds":["{kind}"],"object_uuids":["object-uuid"]}}"#
            );
            assert_eq!(
                calls.lock().unwrap().last().unwrap(),
                &args(&[
                    "live",
                    "execute",
                    "scene.selection.update",
                    "--args",
                    &serialized,
                    "--port",
                    "5005",
                ])
            );
        }
    }
    assert_eq!(calls.lock().unwrap().len(), 12);
}

#[test]
fn selection_mapping_preserves_operation_order_and_empty_category_updates() {
    let (server, calls) = server(true);
    let input = serde_json::from_value(json!({
        "target_kind": "support",
        "preserve_existing": false,
        "operations": [
            {"kind": "add", "objects": [{"kind": "support", "uuid": "a"}, {"kind": "support", "uuid": "b"}]},
            {"kind": "remove", "objects": [{"kind": "support", "uuid": "a"}]},
            {"kind": "add", "objects": []},
            {"kind": "add", "objects": [{"kind": "support", "uuid": "a"}]}
        ]
    }))
    .unwrap();
    server.live_selection_update(Parameters(input)).unwrap();
    server
        .live_selection_update(Parameters(SelectionUpdateArgs {
            target_kind: SelectionObjectKind::SceneObject,
            preserve_existing: false,
            operations: Vec::new(),
            port: None,
        }))
        .unwrap();
    assert_eq!(
        *calls.lock().unwrap(),
        [
            args(&[
                "live",
                "execute",
                "scene.selection.update",
                "--args",
                r#"{"target_kind":"support","preserve_existing":false,"operation_kinds":["add","add","remove","add"],"object_kinds":["support","support","support","support"],"object_uuids":["a","b","a","a"]}"#,
            ]),
            args(&[
                "live",
                "execute",
                "scene.selection.update",
                "--args",
                r#"{"target_kind":"scene_object","preserve_existing":false,"operation_kinds":[],"object_kinds":[],"object_uuids":[]}"#,
            ]),
        ]
    );
}

fn schema(name: &str) -> Value {
    let tool = PerastageMcp::tool_router()
        .list_all()
        .into_iter()
        .find(|tool| tool.name == name)
        .unwrap();
    serde_json::to_value(tool.input_schema).unwrap()
}

#[test]
fn discovery_and_selection_schemas_require_explicit_typed_inputs() {
    for name in ["live_objects_list", "live_layers_list", "live_groups_list"] {
        let schema = schema(name);
        assert_eq!(schema["additionalProperties"], false);
        let properties = schema["properties"].as_object().unwrap();
        assert_eq!(properties.len(), 1);
        assert!(properties.contains_key("port"));
    }
    let lookup = schema("live_object_get");
    assert_eq!(lookup["required"], json!(["kind", "uuid"]));
    assert_eq!(lookup["properties"]["uuid"]["minLength"], 1);
    assert_eq!(
        lookup["$defs"]["ObjectKind"]["enum"],
        json!(["fixture", "truss", "support", "scene_object", "group"])
    );
    let update = schema("live_selection_update");
    assert_eq!(
        update["required"],
        json!(["target_kind", "preserve_existing", "operations"])
    );
    assert_eq!(update["additionalProperties"], false);
    let definitions = &update["$defs"];
    assert_eq!(
        definitions["SelectionObjectKind"]["enum"],
        json!(["fixture", "truss", "support", "scene_object"])
    );
    assert_eq!(
        definitions["SelectionOperationKind"]["enum"],
        json!(["add", "remove"])
    );
    assert_eq!(
        definitions["SelectionOperation"]["required"],
        json!(["kind", "objects"])
    );
    assert_eq!(
        definitions["SelectionOperation"]["additionalProperties"],
        false
    );
    assert_eq!(
        definitions["SelectionObjectReference"]["required"],
        json!(["kind", "uuid"])
    );
    assert_eq!(
        definitions["SelectionObjectReference"]["properties"]["uuid"]["minLength"],
        1
    );
    assert_eq!(
        definitions["SelectionObjectReference"]["additionalProperties"],
        false
    );
}

#[test]
fn malformed_selection_inputs_and_empty_uuids_never_invoke_the_cli() {
    let (server, calls) = server(true);
    for value in [
        json!({"target_kind": "group", "preserve_existing": false, "operations": []}),
        json!({"target_kind": "fixture", "preserve_existing": false}),
        json!({"target_kind": "fixture", "preserve_existing": false, "operations": [{"kind": "toggle", "objects": []}]}),
        json!({"target_kind": "fixture", "preserve_existing": false, "operations": [{"kind": "add", "objects": [{"kind": "fixture"}]}]}),
        json!({"target_kind": "fixture", "preserve_existing": false, "operations": [], "command": "clear"}),
        json!({"target_kind": "fixture", "preserve_existing": false, "operations": [{"kind": "add", "objects": [{"kind": "fixture", "uuid": "a", "name": "Fixture"}]}]}),
    ] {
        assert!(serde_json::from_value::<SelectionUpdateArgs>(value).is_err());
    }
    assert!(serde_json::from_value::<ObjectGetArgs>(json!({"kind": "fixture"})).is_err());
    assert!(
        serde_json::from_value::<ObjectGetArgs>(json!({"kind": "layer", "uuid": "a"})).is_err()
    );
    assert!(serde_json::from_value::<LiveArgs>(json!({"filter": "blinder"})).is_err());
    assert!(
        server
            .live_object_get(Parameters(ObjectGetArgs {
                kind: ObjectKind::Fixture,
                uuid: String::new(),
                port: None,
            }))
            .is_err()
    );
    assert!(
        server
            .live_selection_update(Parameters(SelectionUpdateArgs {
                target_kind: SelectionObjectKind::Fixture,
                preserve_existing: false,
                operations: vec![SelectionOperation {
                    kind: SelectionOperationKind::Add,
                    objects: vec![
                        SelectionObjectReference {
                            kind: SelectionObjectKind::Fixture,
                            uuid: "valid".into()
                        },
                        SelectionObjectReference {
                            kind: SelectionObjectKind::Fixture,
                            uuid: String::new()
                        },
                    ],
                }],
                port: None,
            }))
            .is_err()
    );
    assert!(calls.lock().unwrap().is_empty());
}

#[test]
fn missing_objects_and_kind_mismatches_are_validated_by_the_cli() {
    let (server, calls) = server(false);
    let input = serde_json::from_value(json!({
        "target_kind": "fixture",
        "preserve_existing": false,
        "operations": [{"kind": "add", "objects": [{"kind": "truss", "uuid": "missing"}]}]
    }))
    .unwrap();
    let result = server.live_selection_update(Parameters(input)).unwrap();
    assert_eq!(result.is_error, Some(true));
    assert_eq!(
        result.structured_content.unwrap()["result"]["diagnostics"][0]["code"],
        "test"
    );
    assert_eq!(
        *calls.lock().unwrap(),
        [args(&[
            "live",
            "execute",
            "scene.selection.update",
            "--args",
            r#"{"target_kind":"fixture","preserve_existing":false,"operation_kinds":["add"],"object_kinds":["truss"],"object_uuids":["missing"]}"#,
        ])]
    );
}
