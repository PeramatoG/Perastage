"""Independent Truss standard semantics; this does not claim application interoperability."""
import collections
import io
import zipfile
from mvr_support_addresses_independent import entries, scalar, matrix


def owners(document):
    return {node.get('uuid'): node for node in document.find('Scene').iter('Truss')}


def verify_truss_cases(directory, root, package):
    for name in ('none', 'bound-none', 'bound-dmx', 'empty', 'single', 'multi', 'nonzero', 'only', 'mixed', 'symbol', 'ipv6'):
        stem = f'truss-{name}'
        source = root(directory / f'{stem}-source.mvr')
        original = next(iter(owners(source).values()))
        original_resources = dict(package(directory / f'{stem}-source.mvr'))
        for output in [f'{stem}.mvr'] + [f'{stem}-cycle{cycle}.mvr' for cycle in (1, 2, 3)]:
            document = root(directory / output)  # Well-formedness, separately from native XSD checks.
            nodes = owners(document)
            assert set(nodes) == {original.get('uuid')}
            node = next(iter(nodes.values()))
            assert entries(node) == entries(original)
            assert node.attrib == original.attrib
            for field in ('Classing', 'Position', 'Function', 'FixtureID', 'FixtureIDNumeric', 'UnitNumber', 'CustomIdType', 'CustomId'):
                assert scalar(node, field) == scalar(original, field)
            assert matrix(node) == matrix(original)
            names = [child.tag for child in node]
            if 'Addresses' in names:
                assert names.index('Geometries') < names.index('Addresses') < names.index('FixtureID')
                if 'GDTFMode' in names:
                    assert names.index('GDTFMode') < names.index('Addresses')
            resources = dict(package(directory / output))
            if original.find('GDTFSpec') is not None:
                assert scalar(node, 'GDTFMode') == scalar(original, 'GDTFMode')
                assert resources[scalar(node, 'GDTFSpec')] == original_resources['fixture.gdtf']
            else:
                assert node.find('GDTFSpec') is None and node.find('GDTFMode') is None
            old_geometry = list(original.find('Geometries'))
            new_geometry = list(node.find('Geometries'))
            assert [geo.tag for geo in new_geometry] == [geo.tag for geo in old_geometry]
            if name == 'symbol':
                assert new_geometry[0].attrib == old_geometry[0].attrib
                definition = document.find(".//Symdef[@uuid='" + new_geometry[0].get('symdef') + "']")
                assert definition is not None
                assert resources[definition.find('ChildList/Geometry3D').get('fileName')] == original_resources['truss.glb']
            else:
                assert resources[new_geometry[0].get('fileName')] == original_resources['truss.glb']
            assert matrix(new_geometry[0]) == matrix(old_geometry[0])
        for cycle in (1, 2, 3):
            file = f'{stem}-cycle{cycle}'
            with zipfile.ZipFile(directory / f'{file}.pstg') as archive:
                assert package(io.BytesIO(archive.read('scene.mvr'))) == package(directory / f'{file}-standalone.mvr')
    original = entries(next(iter(owners(root(directory / 'truss-mixed-source.mvr')).values())))
    original_gdtf = dict(package(directory / 'truss-mixed-source.mvr'))['fixture.gdtf']
    for output in ('edited', 'undo', 'redo', 'grouped', 'layer', 'merged', 'copied', 'auxiliary'):
        path = directory / f'truss-{output}.mvr'
        nodes = owners(root(path))
        assert len(nodes) == (2 if output in ('merged', 'copied') else 1)
        assert collections.Counter(entries(node) for node in nodes.values()) == collections.Counter({original: len(nodes)})
        for node in nodes.values():
            assert dict(package(path))[scalar(node, 'GDTFSpec')] == original_gdtf
    with zipfile.ZipFile(directory / 'truss-retained.pstg') as archive:
        assert entries(next(iter(owners(root(io.BytesIO(archive.read('scene.mvr')))).values()))) == original

    auxiliary = root(directory / 'truss-auxiliary.mvr').find('.//TrussInfo/AuxGdtf')
    assert auxiliary is not None
    assert dict(package(directory / 'truss-auxiliary.mvr'))[auxiliary.text] == (directory / 'truss-auxiliary.gdtf').read_bytes()
