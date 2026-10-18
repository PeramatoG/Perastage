#!/usr/bin/env python3
"""Check class graphs, ownership and resources with independent ZIP/XML readers."""
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import uuid
import xml.etree.ElementTree as ET
import zipfile


def read_package(source):
    with zipfile.ZipFile(source) as archive:
        names = archive.namelist()
        assert len(names) == len(set(names)) == len({name.lower() for name in names})
        assert all('/' not in name and '\\' not in name for name in names)
        assert all(item.compress_type in (0, 8) and not item.flag_bits & 1
                   for item in archive.infolist())
        assert archive.testzip() is None
        root = ET.fromstring(archive.read('GeneralSceneDescription.xml'))
        payloads = {name: archive.read(name) for name in names}
    return root.find('Scene'), payloads


def classes(scene):
    return {node.attrib['uuid']: dict(node.attrib) for node in scene.findall('AUXData/Class')}


def assignments(scene):
    return {node.attrib['uuid']: node.findtext('Classing') for node in scene.iter()
            if node.find('Classing') is not None}


def owners(scene):
    return {child.attrib['uuid']: parent.attrib['uuid'] for parent in scene.iter()
            if 'uuid' in parent.attrib for children in parent.findall('ChildList')
            for child in children if 'uuid' in child.attrib}


def check_graph(scene):
    ids = [node.attrib['uuid'] for node in scene.iter() if 'uuid' in node.attrib]
    assert len(ids) == len(set(ids))
    assert all(str(uuid.UUID(value)).upper() == value.upper() for value in ids)
    assert set(assignments(scene).values()) <= classes(scene).keys()
    focus = {node.attrib['uuid'] for node in scene.iter('FocusPoint')}
    assert all(node.findtext('Focus') in focus for node in scene.iter('Fixture'))


def main():
    with tempfile.TemporaryDirectory(prefix='mvr-classes-independent-') as temporary:
        directory = Path(temporary)
        subprocess.run([sys.argv[1], str(directory)], check=True)
        source, payloads = read_package(directory / 'source.mvr')
        for name in ('unchanged', 'edited', 'reopened', 'repeated'):
            scene, resources = read_package(directory / (name + '.mvr'))
            check_graph(scene)
            assert classes(scene) == classes(source)
            assert assignments(scene) == assignments(source)
            assert owners(source).items() <= owners(scene).items()
            for geometry in scene.iter('Geometry3D'):
                assert resources[geometry.attrib['fileName']] == payloads['target.glb']
        for name in ('shared', 'equivalent', 'conflict', 'same-name', 'cross-kind', 'object-collision', 'copied-grouped'):
            scene, _ = read_package(directory / (name + '.mvr'))
            check_graph(scene)
        with zipfile.ZipFile(directory / 'roundtrip.pstg') as project:
            assert project.testzip() is None
            embedded, _ = read_package(io.BytesIO(project.read('scene.mvr')))
            assert classes(embedded) == classes(source)
            assert assignments(embedded) == assignments(source)


if __name__ == '__main__':
    main()
