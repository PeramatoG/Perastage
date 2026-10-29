"""Independent Support standard semantics and immutable ZIP resource evidence."""
import collections
import io
import re
import zipfile


def entries(node):
    containers = node.findall('Addresses')
    assert len(containers) <= 1
    if not containers:
        return None
    result = []
    network_seen = False
    for child in containers[0]:
        assert child.tag in ('Address', 'Network')
        if child.tag == 'Network':
            network_seen = True
            assert child.get('geometry')
            assert set(child.attrib) <= {'geometry', 'ipv4', 'subnetmask', 'ipv6', 'dhcp', 'hostname'}
        else:
            assert not network_seen
        result.append((child.tag, tuple(sorted(child.attrib.items())), child.text or ''))
    return tuple(result)


def owners(root, kind='Support'):
    return {node.get('uuid'): node for node in root.find('Scene').iter(kind)}


def values(root):
    return {key: entries(node) for key, node in owners(root).items()}


def scalar(node, field):
    element = node.find(field)
    return None if element is None else element.text or ''


def matrix(node):
    return tuple(float(value) for value in re.findall(r'[-+0-9.eE]+', node.findtext('Matrix', '')))


def verify_support_cases(directory, root, package):
    for name in ('none', 'empty', 'single', 'multi', 'only', 'ipv6', 'mixed'):
        stem = f'support-{name}'
        source = root(directory / f'{stem}-source.mvr')
        expected = values(source)
        source_node = next(iter(owners(source).values()))
        resources = dict(package(directory / f'{stem}-source.mvr'))
        for output in [f'{stem}.mvr'] + [f'{stem}-cycle{cycle}.mvr' for cycle in (1, 2, 3)]:
            result = root(directory / output)
            assert values(result) == expected
            for node in owners(result).values():
                names = [child.tag for child in node]
                assert names.index('Geometries') < names.index('ChainLength') < names.index('FixtureID')
                if 'Addresses' in names:
                    assert names.index('GDTFMode') < names.index('Addresses') < names.index('FixtureID')
                for field in ('FixtureID', 'FixtureIDNumeric', 'UnitNumber', 'CustomIdType', 'CustomId'):
                    assert scalar(node, field) == scalar(source_node, field)
                assert matrix(node) == matrix(source_node)
                exported = dict(package(directory / output))
                assert exported[node.findtext('GDTFSpec')] == resources['fixture.gdtf']
                for geometry in node.findall('Geometries/Geometry3D'):
                    assert exported[geometry.get('fileName')] == resources['support.glb']
        for cycle in (1, 2, 3):
            file = f'{stem}-cycle{cycle}'
            with zipfile.ZipFile(directory / f'{file}.pstg') as project:
                embedded = package(io.BytesIO(project.read('scene.mvr')))
                assert embedded == package(directory / f'{file}-standalone.mvr')
    original = values(root(directory / 'support-mixed-source.mvr'))
    for output in ('support-edited', 'support-undo', 'support-redo', 'support-grouped', 'support-layer'):
        assert values(root(directory / f'{output}.mvr')) == original
    for output in ('support-merged', 'support-copied'):
        assert collections.Counter(values(root(directory / f'{output}.mvr')).values()) == collections.Counter({next(iter(original.values())): 2})
    with zipfile.ZipFile(directory / 'support-retained.pstg') as project:
        assert values(root(io.BytesIO(project.read('scene.mvr')))) == original
    before = owners(root(directory / 'support-conversion-before.mvr'), 'Fixture')
    for output in ('support-converted', 'support-conversion-redo', 'support-converted-reopened'):
        after = owners(root(directory / f'{output}.mvr'))
        assert set(after) == set(before)
        for key, node in after.items():
            assert entries(node) == entries(before[key])
            assert node.get('name') == before[key].get('name')
            for field in ('Classing', 'GDTFMode', 'Position', 'FixtureID', 'FixtureIDNumeric', 'UnitNumber', 'CustomIdType', 'CustomId'):
                assert scalar(node, field) == scalar(before[key], field)
            assert matrix(node) == matrix(before[key])
            old_resources = dict(package(directory / 'support-conversion-before.mvr'))
            new_resources = dict(package(directory / f'{output}.mvr'))
            assert new_resources[node.findtext('GDTFSpec')] == old_resources[before[key].findtext('GDTFSpec')]
    with zipfile.ZipFile(directory / 'support-converted.pstg') as project:
        assert package(io.BytesIO(project.read('scene.mvr'))) == package(directory / 'support-converted.mvr')
    assert owners(root(directory / 'support-conversion-undo.mvr'), 'Support') == {}
