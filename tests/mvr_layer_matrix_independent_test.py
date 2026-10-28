"""Independent ZIP/XML checks of authored layer placement and canonical packages."""
import io
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

from mvr_symdef_independent_test import graph, read_package
from mvr_matrix_precision_independent import worlds

LAYER = '11000000-0000-4000-8000-000000000001'
DIRECT = '31000000-0000-4000-8000-000000000001'
NESTED = '31000000-0000-4000-8000-000000000002'
OTHER = '31000000-0000-4000-8000-000000000003'


def payloads(source):
    with zipfile.ZipFile(source) as archive:
        assert archive.testzip() is None
        return [(name, archive.read(name)) for name in archive.namelist()]


def standard_payloads(source):
    entries = payloads(source)
    root = ET.fromstring(entries[0][1])
    # Reimport changes existing Perastage runtime-path metadata. The layer
    # contract compares the complete standard scene and every resource here;
    # same-state standalone/PSTG equality below compares all bytes, including it.
    for data in root.findall('UserData'):
        root.remove(data)
    return ET.canonicalize(ET.tostring(root)), entries[1:]


def main():
    with tempfile.TemporaryDirectory(prefix='mvr-layer-independent-') as temporary:
        directory = Path(temporary)
        subprocess.run([sys.argv[1], str(directory)], check=True)
        source, resources = read_package(directory / 'source.mvr')
        expected = graph(source)
        expected_worlds = worlds(*expected[1:])
        for name in ('unchanged', 'reopened', 'cycle1', 'cycle2', 'cycle3', 'undo', 'group-undo'):
            scene, _ = read_package(directory / (name + '.mvr'))
            for fixture in scene.iter('Fixture'):
                assert fixture.findtext('Focus') in {node.get('uuid') for node in scene.iter('FocusPoint')}
            actual = graph(scene)
            assert actual[0] == expected[0]
            assert expected[1].items() <= actual[1].items()
            assert expected[2].items() <= actual[2].items()
            assert expected_worlds.items() <= worlds(*actual[1:]).items()
        canonical = payloads(directory / 'unchanged.mvr')
        # GDTF archives and model payloads are opaque and remain byte-identical.
        canonical_resources = dict(canonical)
        for fixture in source.iter('Fixture'):
            original = resources[fixture.findtext('GDTFSpec')]
            assert original in canonical_resources.values()
        assert resources['target.glb'] in canonical_resources.values()
        for cycle in range(1, 4):
            assert standard_payloads(directory / f'cycle{cycle}.mvr') == standard_payloads(directory / 'unchanged.mvr')
            with zipfile.ZipFile(directory / f'cycle{cycle}.pstg') as project:
                assert payloads(io.BytesIO(project.read('scene.mvr'))) == payloads(directory / f'cycle{cycle}-standalone.mvr')
        for name in ('edited', 'redo', 'regrouped', 'detached', 'ungrouped', 'layer-moved', 'layer-prepared', 'group-created', 'promoted'):
            scene, _ = read_package(directory / (name + '.mvr'))
            _, owners, transforms = graph(scene)
            # Edits and hierarchy operations must retain the authored layer owner.
            assert transforms[LAYER] == expected[2][LAYER]
            actual_worlds = worlds(owners, transforms)
            for identity in (DIRECT, NESTED, OTHER):
                before = expected_worlds[identity]
                after = actual_worlds[identity]
                if name in ('edited', 'redo') and identity == DIRECT:
                    assert after[:9] == before[:9]
                    assert after[9:] == tuple(a + b for a, b in zip(before[9:], (17, 23, 31)))
                else:
                    assert all(math.isclose(a, b, abs_tol=0.001) for a, b in zip(after, before))
        for name, affected, delta in (
                ('world-edited', {DIRECT, '51000000-0000-4000-8000-000000000001',
                                  '61000000-0000-4000-8000-000000000001',
                                  '61000000-0000-4000-8000-000000000002',
                                  '71000000-0000-4000-8000-000000000001'}, (0, 0, 64)),
                ('group-edited', {NESTED, '21000000-0000-4000-8000-000000000001',
                                  '21000000-0000-4000-8000-000000000002'}, (17, 23, 31))):
            scene, _ = read_package(directory / (name + '.mvr'))
            _, owners, transforms = graph(scene)
            assert expected[1].items() <= owners.items()
            actual = worlds(owners, transforms)
            for identity, before in expected_worlds.items():
                after = actual[identity]
                shift = delta if identity in affected else (0, 0, 0)
                assert after[:9] == before[:9]
                assert after[9:] == tuple(a + b for a, b in zip(before[9:], shift))
        assert standard_payloads(directory / 'group-edited.mvr') == standard_payloads(directory / 'group-edited-cycle.mvr')
        assert payloads(directory / 'regrouped.mvr') == payloads(directory / 'group-redo.mvr')
        assert standard_payloads(directory / 'edited.mvr') == standard_payloads(directory / 'edited-reopened.mvr')
        assert payloads(directory / 'edited.mvr') == payloads(directory / 'redo.mvr')
        with zipfile.ZipFile(directory / 'edited.pstg') as project:
            assert payloads(io.BytesIO(project.read('scene.mvr'))) == payloads(directory / 'edited.mvr')
        merged, _ = read_package(directory / 'merged.mvr')
        _, owners, transforms = graph(merged)
        assert expected[0].items() <= graph(merged)[0].items()
        assert expected[1].items() <= owners.items()
        assert expected[2].items() <= transforms.items()
        incoming_layers = [node for node in merged.iter('Layer')
                           if node.get('name') == 'Elevated' and node.get('uuid') != LAYER]
        assert len(incoming_layers) == 1
        replacement = incoming_layers[0].get('uuid')
        assert transforms[replacement][11] != transforms[LAYER][11]
        assert len(list(merged.iter('Truss'))) == 6
        # Every remapped object remains inside its remapped layer's coordinate system.
        incoming_direct = incoming_layers[0].find('ChildList/Truss')
        assert owners[incoming_direct.get('uuid')] == replacement
        assert transforms[incoming_direct.get('uuid')] == expected[2][DIRECT]
        assert worlds(owners, transforms)[incoming_direct.get('uuid')][11] == (
            transforms[replacement][11] + expected[2][DIRECT][11])
        focus_ids = {node.get('uuid') for node in merged.iter('FocusPoint')}
        for fixture in merged.iter('Fixture'):
            assert fixture.findtext('Focus') in focus_ids
        for node in merged.iter():
            reference = node.findtext('Classing')
            if reference:
                assert any(item.get('uuid') == reference for item in merged.iter('Class'))


if __name__ == '__main__':
    main()
