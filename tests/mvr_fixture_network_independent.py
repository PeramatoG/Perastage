"""Independent XML/ZIP comparison of Fixture Network ownership and lexical data."""
import collections
import io
import ipaddress
import zipfile

MULTI = '51000000-0000-4000-8000-000000000002'


def networks(root):
    result = {}
    for fixture in root.find('Scene').iter('Fixture'):
        addresses = fixture.find('Addresses')
        if addresses is None:
            result[fixture.get('uuid')] = None
            continue
        children = list(addresses)
        tags = [entry.tag for entry in children]
        assert tags == sorted(tags, key={'Address': 0, 'Network': 1}.__getitem__)
        entries = []
        for entry in children:
            if entry.tag != 'Network':
                continue
            assert entry.get('geometry') in ('ethernet_1', 'wireless_1', '51000000-0000-4000-8000-000000000001')
            assert set(entry.attrib) <= {'geometry', 'ipv4', 'subnetmask', 'ipv6', 'dhcp', 'hostname'}
            if entry.get('ipv6'):
                ipaddress.IPv6Address(entry.get('ipv6'))
            # Retain authored IPv4 leading zeros instead of normalizing via ipaddress.
            for field in ('ipv4', 'subnetmask'):
                if entry.get(field):
                    octets = entry.get(field).split('.')
                    assert len(octets) == 4 and all(0 <= int(value) <= 255 for value in octets)
            entries.append(tuple(sorted(entry.attrib.items())))
        result[fixture.get('uuid')] = tuple(entries)
    return result


def verify_network_cases(directory, root, package, assignments):
    with zipfile.ZipFile(io.BytesIO(dict(package(directory / 'source.mvr'))['fixture.gdtf'])) as gdtf:
        assert gdtf.read('models/gltf/body.glb')[:4] == b'glTF'
    source_gdtf = dict(package(directory / 'source.mvr'))['fixture.gdtf']
    source = networks(root(directory / 'source.mvr'))
    assert len(source[MULTI]) == 3
    assert dict(source[MULTI][0]) == {'geometry': 'ethernet_1', 'ipv4': '192.168.001.005', 'subnetmask': '255.255.0.0'}
    assert dict(source[MULTI][2]) == {'geometry': '51000000-0000-4000-8000-000000000001'}
    assert dict(source[MULTI][1]) == {'geometry': 'wireless_1', 'dhcp': 'on', 'hostname': 'Rig.Left'}
    for name in ('unchanged', 'undo', 'redo', 'edited', 'edited-reopened', 'cleared',
                 'cleared-reopened', 'legacy-edited', 'auto-patched', 'auto-gui', 'hierarchy',
                 'cycle1', 'cycle2', 'cycle3', 'nested-groups', 'network-rebound', 'network-original-reference'):
        assert networks(root(directory / f'{name}.mvr')) == source, name
        resources = dict(package(directory / f'{name}.mvr'))
        for fixture in root(directory / f'{name}.mvr').iter('Fixture'):
            assert resources[fixture.findtext('GDTFSpec')] == source_gdtf
    assert root(directory / 'nested-groups.mvr').find('.//GroupObject/ChildList/GroupObject') is not None
    assert networks(root(directory / 'auto-all.mvr'))[MULTI] == source[MULTI]
    for name in ('merged', 'copied'):
        actual = networks(root(directory / f'{name}.mvr'))
        assert source.items() <= actual.items()
        expected = collections.Counter(source.values())
        if name == 'merged':
            expected = collections.Counter({value: count * 2 for value, count in expected.items()})
        else:
            expected[source[MULTI]] += 1
        assert collections.Counter(actual.values()) == expected
    for name in ('none', 'empty', 'only', 'repeat', 'ipv4', 'ipv6', 'dhcp', 'lexical'):
        stem = f'network-{name}'
        original = networks(root(directory / f'{stem}-source.mvr'))
        assert networks(root(directory / f'{stem}.mvr')) == original
        assert assignments(root(directory / f'{stem}-source.mvr')) == assignments(root(directory / f'{stem}.mvr'))
        with zipfile.ZipFile(directory / f'{stem}.pstg') as project:
            embedded = io.BytesIO(project.read('scene.mvr'))
            assert package(embedded) == package(directory / f'{stem}.mvr')
        source_resources = dict(package(directory / f'{stem}-source.mvr'))
        exported = dict(package(directory / f'{stem}.mvr'))
        for fixture in root(directory / f'{stem}.mvr').iter('Fixture'):
            assert exported[fixture.findtext('GDTFSpec')] == source_resources['fixture.gdtf']
    only = networks(root(directory / 'network-only-source.mvr'))
    assert networks(root(directory / 'network-auto-only.mvr')) == only
    auto_only = assignments(root(directory / 'network-auto-only.mvr'))[MULTI]
    assert len(auto_only) == 1 and auto_only[0][0] == '0'
    for name in ('added', 'removed'):
        assert networks(root(directory / f'network-{name}-primary.mvr')) == only
    assert assignments(root(directory / 'network-added-primary.mvr'))[MULTI] == (('0', '529'),)
    assert assignments(root(directory / 'network-removed-primary.mvr'))[MULTI] == ()
    with zipfile.ZipFile(directory / 'network-retained.pstg') as project:
        assert package(io.BytesIO(project.read('scene.mvr'))) == package(directory / 'unchanged.mvr')
