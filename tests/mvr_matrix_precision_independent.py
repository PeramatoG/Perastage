"""Independent numeric, hierarchy and full-package checks for precision fixtures."""
import io
import xml.etree.ElementTree as ET
import zipfile

from mvr_symdef_independent_test import definitions, graph, read_package

DEFINITION = 'a1000000-0000-4000-8000-000000000001'
OUTER = '21000000-0000-4000-8000-000000000001'
INNER = '21000000-0000-4000-8000-000000000002'
TRUSS = '31000000-0000-4000-8000-000000000001'


def multiply(a, b):
    """Compose column-vector 4x3 affine matrices without the production code."""
    return tuple(sum(a[k * 3 + axis] * b[column * 3 + k] for k in range(3))
                 + (a[9 + axis] if column == 3 else 0.)
                 for column in range(4) for axis in range(3))


def worlds(owners, transforms):
    def world(identity):
        parent = owners.get(identity)
        local = transforms[identity]
        return multiply(world(parent), local) if parent in transforms else local
    return {identity: world(identity) for identity in transforms}


def check_precision(directory):
    directory = directory / 'precision'
    source, payloads = read_package(directory / 'source.mvr')
    expected_definitions = definitions(source, payloads)
    expected_graph = graph(source)
    assert len(expected_graph[0]) == 19
    assert all(owner == INNER for child, owner in expected_graph[1].items()
               if child.startswith('31000000'))
    canonical, _ = read_package(directory / 'unchanged.mvr')
    canonical_graph = graph(canonical)
    assert canonical_graph[:2] == expected_graph[:2]
    assert expected_graph[2].items() <= canonical_graph[2].items()
    # The existing importer creates an empty default layer; it owns no placement.
    extra = canonical_graph[2].keys() - expected_graph[2].keys()
    assert all(node.tag == 'Layer' and len(node.find('ChildList')) == 0
               for node in canonical.iter() if node.get('uuid') in extra)
    expected_graph = canonical_graph
    expected_worlds = worlds(*expected_graph[1:])
    for name in ('unchanged', 'cycle1', 'cycle2', 'cycle3', 'standalone', 'reopened'):
        scene, resources = read_package(directory / (name + '.mvr'))
        assert definitions(scene, resources) == expected_definitions
        actual = graph(scene)
        assert actual == expected_graph
        assert worlds(*actual[1:]) == expected_worlds
        assert set(resources.values()) == set(payloads.values())
    for name, changed in (('world-edited', TRUSS), ('edited', TRUSS), ('parent-edited', OUTER)):
        scene, resources = read_package(directory / (name + '.mvr'))
        symbols, owners, transforms = graph(scene)
        assert symbols == expected_graph[0] and owners == expected_graph[1]
        assert definitions(scene, resources) == expected_definitions
        assert transforms[changed] != expected_graph[2][changed]
        # All these edits change translation only, without narrowing authored linear placement.
        assert transforms[changed][:9] == expected_graph[2][changed][:9]
        assert {key: value for key, value in transforms.items() if key != changed} == {
            key: value for key, value in expected_graph[2].items() if key != changed}
    edited_root, edited_resources = read_package(directory / 'edited.mvr', True)
    cycle_root, cycle_resources = read_package(directory / 'edited-cycle.mvr', True)
    assert ET.canonicalize(ET.tostring(edited_root)) == ET.canonicalize(ET.tostring(cycle_root))
    assert edited_resources == cycle_resources
    reopened_root, reopened_resources = read_package(directory / 'edited-reopened.mvr', True)
    assert ET.canonicalize(ET.tostring(edited_root)) == ET.canonicalize(ET.tostring(reopened_root))
    assert edited_resources == reopened_resources
    with zipfile.ZipFile(directory / 'edited.pstg') as project:
        embedded, embedded_resources = read_package(io.BytesIO(project.read('scene.mvr')), True)
    assert ET.canonicalize(ET.tostring(embedded)) == ET.canonicalize(ET.tostring(edited_root))
    assert embedded_resources == edited_resources
    for name in ('rotated', 'scaled'):
        changed, resources = read_package(directory / (name + '.mvr'))
        symbols, owners, transforms = graph(changed)
        assert symbols == expected_graph[0] and owners == expected_graph[1]
        assert definitions(changed, resources) == expected_definitions
        assert transforms[TRUSS][:9] != expected_graph[2][TRUSS][:9]
        # A linear edit around the insertion point cannot move that insertion point.
        assert transforms[TRUSS][9:] == expected_graph[2][TRUSS][9:]
        assert {key: value for key, value in transforms.items() if key != TRUSS} == {
            key: value for key, value in expected_graph[2].items() if key != TRUSS}
    edited, resources = read_package(directory / 'definition-edited.mvr')
    assert graph(edited) == expected_graph
    changed_definition = definitions(edited, resources)[DEFINITION]
    actual_matrix = changed_definition[1][1][0]
    source_matrix = expected_definitions[DEFINITION][1][1][0]
    assert actual_matrix[9] == 8765.125
    assert actual_matrix[:9] == source_matrix[:9] and actual_matrix[10:] == source_matrix[10:]
    merged, resources = read_package(directory / 'merged.mvr')
    merged_definitions = definitions(merged, resources)
    assert len(merged_definitions) == 2
    assert merged_definitions[DEFINITION] == expected_definitions[DEFINITION]
    replacement, = merged_definitions.keys() - expected_definitions.keys()
    # Distinct doubles with the same float32 display must remain distinct definitions.
    assert merged_definitions[replacement][1][1][0][9] != expected_definitions[DEFINITION][1][1][0][9]
    for project_name in ('roundtrip.pstg', 'second-cycle.pstg'):
        with zipfile.ZipFile(directory / project_name) as project:
            embedded_root, embedded_resources = read_package(io.BytesIO(project.read('scene.mvr')), True)
        standalone_root, standalone_resources = read_package(directory / 'standalone.mvr', True)
        assert ET.canonicalize(ET.tostring(embedded_root)) == ET.canonicalize(ET.tostring(standalone_root))
        assert embedded_resources == standalone_resources
