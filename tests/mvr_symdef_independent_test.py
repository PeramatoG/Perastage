#!/usr/bin/env python3
"""Verify canonical definition/instance graphs with independent ZIP/XML readers."""
import io
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import uuid
import xml.etree.ElementTree as ET
import zipfile

IDENTITY = (1., 0., 0., 0., 1., 0., 0., 0., 1., 0., 0., 0.)
DEFINITION = 'a0000000-0000-4000-8000-000000000001'


def matrix(node):
    text = node.findtext('Matrix')
    return tuple(map(float, re.findall(r'[-+\d.eE]+', text))) if text else IDENTITY


def read_package(source, full_document=False):
    with zipfile.ZipFile(source) as archive:
        names = archive.namelist()
        assert len(names) == len(set(names)) == len({name.lower() for name in names})
        assert all('/' not in name and '\\' not in name for name in names)
        assert all(item.compress_type in (0, 8) and not item.flag_bits & 1
                   for item in archive.infolist())
        assert archive.testzip() is None
        resources = {name: archive.read(name) for name in names}
    root = ET.fromstring(resources.pop('GeneralSceneDescription.xml'))
    return (root if full_document else root.find('Scene')), resources


def definitions(scene, resources):
    return {node.attrib['uuid']: (node.get('name'), tuple(
        (matrix(geometry), resources[geometry.attrib['fileName']])
        for geometry in node.find('ChildList')))
        for node in scene.findall('AUXData/Symdef')}


def graph(scene):
    identities = [node.attrib['uuid'] for node in scene.iter() if 'uuid' in node.attrib]
    assert len(identities) == len(set(identities))
    assert all(str(uuid.UUID(value)) == value.lower() and uuid.UUID(value).int
               for value in identities)
    defs = {node.attrib['uuid'] for node in scene.findall('AUXData/Symdef')}
    symbols = {node.attrib['uuid']: (owner.attrib['uuid'], node.attrib['symdef'], matrix(node))
               for owner in scene.iter() for entries in owner.findall('Geometries')
               for node in entries.findall('Symbol')}
    assert all(target in defs for _, target, _ in symbols.values())
    owners = {child.attrib['uuid']: parent.attrib['uuid'] for parent in scene.iter()
              if 'uuid' in parent.attrib for children in parent.findall('ChildList')
              for child in children if 'uuid' in child.attrib}
    transforms = {node.attrib['uuid']: matrix(node) for node in scene.iter()
                  if 'uuid' in node.attrib}
    return symbols, owners, transforms


def check_models(resources):
    for name, data in resources.items():
        if name.endswith('.glb'):
            assert struct.unpack('<III', data[:12]) == (0x46546C67, 2, len(data))
            assert struct.unpack('<I', data[16:20])[0] == 0x4E4F534A


def main():
    with tempfile.TemporaryDirectory(prefix='mvr-symdef-independent-') as temporary:
        directory = Path(temporary)
        subprocess.run([sys.argv[1], str(directory)], check=True)
        source, payloads = read_package(directory / 'source.mvr')
        expected_defs = definitions(source, payloads)
        expected_graph = graph(source)
        assert len(expected_defs) == 3
        assert len(expected_graph[0]) == 2
        assert {target for _, target, _ in expected_graph[0].values()} == {DEFINITION}
        named_source, named_resources = read_package(directory / 'named-source.mvr')
        named_defs = definitions(named_source, named_resources)
        for name in ('unchanged', 'named', 'edited', 'hidden', 'reopened', 'repeated'):
            scene, resources = read_package(directory / (name + '.mvr'))
            check_models(resources)
            assert definitions(scene, resources) == (expected_defs if name == 'unchanged' else named_defs)
            actual = graph(scene)
            assert actual[0] == expected_graph[0]
            assert expected_graph[1].items() <= actual[1].items()
            assert expected_graph[2].items() <= actual[2].items()
        for name, count, instances in (('shared', 3, 4), ('conflict', 4, 4),
                                       ('cross-kind', 3, 2), ('copied-grouped', 3, 3)):
            scene, resources = read_package(directory / (name + '.mvr'))
            defs = definitions(scene, resources)
            symbols, _, _ = graph(scene)
            assert len(defs) == count and len(symbols) == instances
            if name == 'cross-kind':
                assert set(named_defs.values()) == set(defs.values())
                assert all(target != DEFINITION for _, target, _ in symbols.values())
            else:
                assert named_defs.items() <= defs.items()
            if name == 'conflict':
                replacement, = defs.keys() - named_defs.keys()
                assert defs[replacement][1][0][1] != defs[DEFINITION][1][0][1]
                assert sum(target == replacement for _, target, _ in symbols.values()) == 2
                assert sum(target == DEFINITION for _, target, _ in symbols.values()) == 2
        with zipfile.ZipFile(directory / 'roundtrip.pstg') as project:
            assert project.testzip() is None
            embedded, resources = read_package(io.BytesIO(project.read('scene.mvr')))
            assert definitions(embedded, resources) == named_defs
            assert graph(embedded)[0] == expected_graph[0]
            standalone, standalone_resources = read_package(directory / 'hidden.mvr')
            # Equivalent options use the same complete standard scene and assets.
            assert ET.canonicalize(ET.tostring(embedded)) == ET.canonicalize(ET.tostring(standalone))
            embedded_root, _ = read_package(io.BytesIO(project.read('scene.mvr')), True)
            standalone_root, _ = read_package(directory / 'hidden.mvr', True)
            assert ET.canonicalize(ET.tostring(embedded_root)) == ET.canonicalize(ET.tostring(standalone_root))
            assert resources == standalone_resources


if __name__ == '__main__':
    main()
