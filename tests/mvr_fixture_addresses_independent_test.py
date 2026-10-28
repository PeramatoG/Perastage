"""Independent standard Address semantics and same-state ZIP payload checks."""
import collections
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import uuid
import xml.etree.ElementTree as ET
import zipfile

SINGLE = '51000000-0000-4000-8000-000000000001'
MULTI = '51000000-0000-4000-8000-000000000002'
NONZERO = '51000000-0000-4000-8000-000000000003'


def package(source):
    with zipfile.ZipFile(source) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert len(names) == len(set(names)) == len({name.lower() for name in names})
        assert names[0] == 'GeneralSceneDescription.xml'
        return [(name, archive.read(name)) for name in names]


def root(source):
    return ET.fromstring(package(source)[0][1])


def assignments(scene):
    scene = scene.find('Scene')
    assert scene is not None
    result = {}
    identities = [node.get('uuid') for node in scene.iter() if node.get('uuid')]
    assert len(identities) == len(set(identities))
    assert all(uuid.UUID(identity).int for identity in identities)
    for fixture in scene.iter('Fixture'):
        containers = fixture.findall('Addresses')
        assert len(containers) <= 1
        entries = None if not containers else tuple(
            (entry.get('break'), entry.text or '') for entry in containers[0])
        assert all(entry.tag == 'Address' for container in containers for entry in container)
        if entries:
            breaks = [int(identifier or '0') for identifier, _ in entries]
            assert len(breaks) == len(set(breaks))
            for _, value in entries:
                if '.' in value:
                    universe, channel = map(int, value.split('.'))
                    assert universe > 0 and 1 <= channel <= 512
                else:
                    assert int(value) > 0
        result[fixture.get('uuid')] = entries
    return result


def standard(source):
    entries = package(source)
    scene = ET.fromstring(entries[0][1])
    # Reimport changes existing extraction-path metadata, independently of DMX.
    # Complete same-state equality below retains and compares that metadata.
    for data in scene.findall('UserData'):
        scene.remove(data)
    return ET.canonicalize(ET.tostring(scene)), entries[1:]


def main():
    with tempfile.TemporaryDirectory(prefix='mvr-address-independent-') as temporary:
        directory = Path(temporary)
        subprocess.run([sys.argv[1], str(directory)], check=True)
        source = assignments(root(directory / 'source.mvr'))
        assert source[MULTI] == (('0', '1025'), ('7', '2049'), ('19', '4097'))
        for name in ('unchanged', 'undo', 'hierarchy', 'cycle1', 'cycle2', 'cycle3'):
            assert assignments(root(directory / f'{name}.mvr')) == source
        assert package(directory / 'standalone.mvr') == package(directory / 'unchanged.mvr')
        original_resources = dict(package(directory / 'source.mvr'))
        for cycle in range(1, 4):
            with zipfile.ZipFile(directory / f'cycle{cycle}.pstg') as project:
                embedded = io.BytesIO(project.read('scene.mvr'))
                assert package(embedded) == package(directory / f'cycle{cycle}-standalone.mvr')
            assert standard(directory / f'cycle{cycle}.mvr') == standard(directory / 'unchanged.mvr')
            entries = dict(package(directory / f'cycle{cycle}.mvr'))
            for fixture in root(directory / f'cycle{cycle}.mvr').iter('Fixture'):
                assert entries[fixture.findtext('GDTFSpec')] == original_resources['fixture.gdtf']
        for name, primary in (('edited', '2065'), ('redo', '2065'), ('edited-reopened', '2065'),
                              ('legacy-edited', '2681')):
            actual = assignments(root(directory / f'{name}.mvr'))
            assert actual[SINGLE] == source[SINGLE] and actual[NONZERO] == source[NONZERO]
            assert actual[MULTI] == (('0', primary),) + source[MULTI][1:]
        assert package(directory / 'edited.mvr') == package(directory / 'redo.mvr')
        assert standard(directory / 'edited.mvr') == standard(directory / 'edited-reopened.mvr')
        with zipfile.ZipFile(directory / 'edited.pstg') as project:
            assert package(io.BytesIO(project.read('scene.mvr'))) == package(directory / 'edited.mvr')
        for name in ('cleared', 'cleared-reopened'):
            clear = assignments(root(directory / f'{name}.mvr'))
            assert clear[MULTI] == source[MULTI][1:]
            assert clear[SINGLE] == source[SINGLE] and clear[NONZERO] == source[NONZERO]
        with zipfile.ZipFile(directory / 'cleared.pstg') as project:
            assert package(io.BytesIO(project.read('scene.mvr'))) == package(directory / 'cleared.mvr')
        assert assignments(root(directory / 'auto-gui.mvr')) == assignments(root(directory / 'auto-patched.mvr'))
        auto = assignments(root(directory / 'auto-patched.mvr'))
        assert auto[MULTI][1:] == source[MULTI][1:]
        assert auto[MULTI][0] != source[MULTI][0]
        assert auto[SINGLE] == source[SINGLE] and auto[NONZERO] == source[NONZERO]
        for name, factor in (('merged', 2), ('copied', 1)):
            actual = assignments(root(directory / f'{name}.mvr'))
            assert source.items() <= actual.items()
            expected = collections.Counter(source.values())
            if name == 'merged':
                expected = collections.Counter({key: count * factor for key, count in expected.items()})
            else:
                expected[source[MULTI]] += 1
            assert collections.Counter(actual.values()) == expected
        for name in ('empty', 'absent', 'spelling', 'published', 'large'):
            assert assignments(root(directory / f'{name}-source.mvr')) == assignments(root(directory / f'{name}.mvr'))


if __name__ == '__main__':
    main()
