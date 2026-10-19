#!/usr/bin/env python3
"""Verify identifier semantics and shared PSTG/MVR output without the MVR reader."""
from collections import Counter
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

KINDS = {'Fixture', 'Truss', 'Support', 'SceneObject'}


def package(source):
    with zipfile.ZipFile(source) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert len(names) == len(set(names))
        resources = {name: archive.read(name) for name in names}
    return ET.fromstring(resources.pop('GeneralSceneDescription.xml')), resources


def identifiers(root):
    return {node.attrib['uuid']: (node.tag, node.findtext('FixtureID', ''),
            int(node.findtext('FixtureIDNumeric')),
            int(node.findtext('UnitNumber')) if node.find('UnitNumber') is not None else None)
            for node in root.iter() if node.tag in KINDS}


def main():
    with tempfile.TemporaryDirectory(prefix='mvr-identifiers-') as temporary:
        directory = Path(temporary)
        subprocess.run([sys.argv[1], str(directory)], check=True)
        source, _ = package(directory / 'source.mvr')
        expected = identifiers(source)
        assert len(expected) == 5
        assert all(value[2] == 0 for value in expected.values())
        for name in ('unchanged', 'canonical', 'reopened', 'replaced'):
            root, _ = package(directory / (name + '.mvr'))
            assert identifiers(root) == expected
        canonical, canonical_resources = package(directory / 'canonical.mvr')
        for name, reference in (('roundtrip', 'canonical'), ('second-cycle', 'second-canonical')):
            with zipfile.ZipFile(directory / (name + '.pstg')) as project:
                assert project.testzip() is None
                root, resources = package(io.BytesIO(project.read('scene.mvr')))
            assert identifiers(root) == expected
            same_state, same_state_resources = package(directory / (reference + '.mvr'))
            with zipfile.ZipFile(directory / (name + '.pstg')) as project:
                with zipfile.ZipFile(io.BytesIO(project.read('scene.mvr'))) as embedded:
                    with zipfile.ZipFile(directory / (reference + '.mvr')) as standalone:
                        assert embedded.namelist() == standalone.namelist()
                        assert [(n, embedded.read(n)) for n in embedded.namelist()] == [
                            (n, standalone.read(n)) for n in standalone.namelist()]
            assert ET.canonicalize(ET.tostring(root)) == ET.canonicalize(ET.tostring(same_state))
            assert ET.canonicalize(ET.tostring(root.find('Scene'))) == ET.canonicalize(ET.tostring(canonical.find('Scene')))
            assert resources == same_state_resources
            assert resources == canonical_resources
        merged, _ = package(directory / 'merged.mvr')
        assert Counter(identifiers(merged).values()) == Counter(expected.values()) + Counter(expected.values())
        operations, _ = package(directory / 'operations.mvr')
        actual = identifiers(operations)
        assert len(actual) == 11
        assert expected.items() <= actual.items()
        edited, _ = package(directory / 'edited.mvr')
        edited_values = identifiers(edited)
        selected = '40000000-0000-4000-8000-000000000001'
        assert edited_values.pop(selected) == ('Fixture', '42', 42, 0)
        actual.pop(selected)
        assert edited_values == actual
        numeric, _ = package(directory / 'numeric-edited.mvr')
        numeric_values = identifiers(numeric)
        assert numeric_values.pop(selected) == ('Fixture', '  House & 1  ', 63, 0)
        assert numeric_values == actual
        unit, _ = package(directory / 'unit-edited.mvr')
        unit_values = identifiers(unit)
        assert unit_values.pop(selected) == ('Fixture', '  House & 1  ', 63, 0)
        support = '80000000-0000-4000-8000-000000000001'
        assert unit_values.pop(support) == ('Support', 'Hoist tag', 0, 5)
        actual.pop(support)
        assert unit_values == actual
        for name in ('units', 'stale', 'positive', 'negative', 'empty', 'empty-containers'):
            authored, _ = package(directory / (name + '-source.mvr'))
            expected_variant = identifiers(authored)
            if name == 'positive':
                assert any(node.text == '+0007' for node in authored.iter('UnitNumber'))
            if name == 'empty-containers':
                for node in authored.iter('Protocols'):
                    assert not node.attrib and not list(node)
                    assert node.text and not node.text.strip()
            for suffix in ('', '-reopened'):
                output, _ = package(directory / (name + suffix + '.mvr'))
                assert identifiers(output) == expected_variant
                if name == 'positive':
                    assert all(node.text == '7' for node in output.iter('UnitNumber'))
                if name == 'empty-containers':
                    assert output.find('.//Protocols') is None
            with zipfile.ZipFile(directory / (name + '.pstg')) as project:
                embedded, _ = package(io.BytesIO(project.read('scene.mvr')))
                assert identifiers(embedded) == expected_variant


if __name__ == '__main__':
    main()
