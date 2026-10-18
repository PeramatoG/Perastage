#!/usr/bin/env python3
"""Read generated round-trips with independent ZIP/XML implementations."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import uuid
import xml.etree.ElementTree as ET
import zipfile


def read_package(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        assert len(names) == len(set(names)) == len({n.lower() for n in names})
        assert all('/' not in name and '\\' not in name for name in names)
        assert all(info.compress_type in (0, 8) and not info.flag_bits & 1
                   for info in archive.infolist())
        assert archive.testzip() is None
        root = ET.fromstring(archive.read('GeneralSceneDescription.xml').decode('utf-8'))
        payloads = {name: hashlib.sha256(archive.read(name)).hexdigest() for name in names}
    return root, payloads


def standard_objects(root):
    return {node.attrib['uuid']: node for node in root.find('Scene').iter() if 'uuid' in node.attrib}


def owners(root):
    return {child.attrib['uuid']: parent.attrib['uuid']
            for parent in root.find('Scene').iter() if 'uuid' in parent.attrib
            for children in parent.findall('ChildList') for child in children
            if 'uuid' in child.attrib}


def signature(node, payloads):
    """Compare source semantics while allowing XML formatting and resource renames."""
    attributes = dict(node.attrib)
    if node.tag == 'Geometry3D':
        attributes['fileName'] = payloads[attributes['fileName']]
    text = (node.text or '').strip()
    if node.tag == 'GDTFSpec':
        text = payloads[text]
    if node.tag == 'Matrix':
        text = tuple(float(component) for component in text.replace('{', '').replace('}', ',').strip(',').split(','))
    return node.tag, tuple(sorted(attributes.items())), text, tuple(signature(child, payloads) for child in node)


def assert_references(root, expected_focus, expected_fixtures):
    nodes = list(root.find('Scene').iter())
    identities = [node.attrib['uuid'] for node in nodes if 'uuid' in node.attrib]
    assert len(identities) == len(set(identities))
    assert all(uuid.UUID(identity).int for identity in identities)
    focus = {node.attrib['uuid'] for node in nodes if node.tag == 'FocusPoint'}
    fixtures = [node for node in nodes if node.tag == 'Fixture']
    assert len(focus) == expected_focus and len(fixtures) == expected_fixtures
    assert all(fixture.findtext('Focus') in focus for fixture in fixtures)
    assert all(node.find('Geometries') is not None for node in nodes if node.tag == 'FocusPoint')


def main():
    with tempfile.TemporaryDirectory(prefix='mvr-independent-') as temporary:
        directory = Path(temporary)
        subprocess.run([sys.argv[1], str(directory)], check=True)
        original, original_payloads = read_package(directory / 'source.mvr')
        source_objects = standard_objects(original)
        for name in ('unchanged', 'edited', 'reopened', 'repeated'):
            root, payloads = read_package(directory / (name + '.mvr'))
            assert_references(root, 2, 2)
            objects = standard_objects(root)
            assert source_objects.keys() <= objects.keys()
            assert owners(original).items() <= owners(root).items()
            # Focus subtrees have no canonical additions; all authored values,
            # local matrices and referenced resource bytes must match exactly.
            for identity, node in source_objects.items():
                exported = objects[identity]
                if node.tag == 'FocusPoint':
                    assert signature(node, original_payloads) == signature(exported, payloads)
                if node.tag == 'GroupObject':
                    assert signature(node.find('Matrix'), original_payloads) == signature(exported.find('Matrix'), payloads)
                if node.tag == 'Fixture':
                    for field in ('Focus', 'Function', 'DMXInvertPan', 'DMXInvertTilt',
                                  'Position', 'FixtureID', 'FixtureIDNumeric', 'UnitNumber',
                                  'CustomId', 'CustomIdType', 'Color', 'Addresses', 'GDTFSpec', 'GDTFMode'):
                        assert signature(node.find(field), original_payloads) == signature(exported.find(field), payloads)
            assert signature(original.find("UserData/Data[@provider='Foreign']"), original_payloads) == signature(root.find("UserData/Data[@provider='Foreign']"), payloads)
        for name, focuses, fixtures in (('merged', 4, 4), ('copied', 3, 3), ('grouped', 3, 3), ('legacy', 2, 0), ('empty', 1, 0), ('transformed', 4, 4)):
            root, _ = read_package(directory / (name + '.mvr'))
            assert_references(root, focuses, fixtures)
        with zipfile.ZipFile(directory / 'roundtrip.pstg') as project:
            assert 'scene.mvr' in project.namelist()
        print('Independent Python ZIP/XML checks passed: identities, references, metadata, matrices and payload hashes')


if __name__ == '__main__':
    main()
