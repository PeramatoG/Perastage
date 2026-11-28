#!/usr/bin/env python3
"""Drive the real wxWidgets editor under a clean 1600x1000 Xvfb display.

Uses standard-library X11/XTest bindings, the production toolbar and PSTG saves;
there are no production test hooks or alternate document/rendering paths.
"""
import argparse
import copy
import ctypes as C
from ctypes.util import find_library
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import zipfile
import zlib


def fixture(directory):
    def frame(x, y, w, h):
        return dict(x=x, y=y, width=w, height=h)

    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))

    logo = directory / 'logo.png'
    logo.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', 2, 1, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(b'\0' + bytes([220, 70, 40, 255]) * 2)) + chunk(b'IEND', b''))
    page = dict(name='Alignment smoke', pageSetup=dict(pageSize='A4', landscape=True),
                view2dViews=[dict(id=1, zIndex=0, frame=frame(40, 40, 150, 100), camera=dict(view=0, zoom=1),
                                 viewport=dict(mode='fixed', physical=dict(denominator=50, centerX=2, centerY=3),
                                               frameResidual=[.25, .375, .125, .25]))],
                legendViews=[dict(id=1, zIndex=1, frame=frame(260, 180, 110, 60))],
                eventTables=[dict(id=1, zIndex=2, frame=frame(400, 280, 100, 90), fields=dict(venue='Smoke'))],
                textViews=[dict(id=1, zIndex=3, frame=frame(550, 390, 120, 60), text='Alignment smoke')],
                imageViews=[dict(id=1, zIndex=4, frame=frame(680, 80, 80, 40), path=str(logo), aspectRatio=2)])
    other = dict(name='Other page', pageSetup=page['pageSetup'],
                 textViews=[dict(id=1, zIndex=0, frame=frame(20, 20, 100, 50), text='Other page')])
    config = dict(layouts_collection=json.dumps(dict(schemaVersion=1, layouts=[page, other])),
                  layout_active_name=page['name'], layout_view_mode='layout_mode_view', ui_language='en')
    scene = io.BytesIO()
    with zipfile.ZipFile(scene, 'w') as archive:
        archive.writestr('GeneralSceneDescription.xml', '<?xml version="1.0" encoding="UTF-8"?>'
                         '<GeneralSceneDescription verMajor="1" verMinor="6"><Scene><Layers>'
                         '<Layer name="Smoke" uuid="11111111-1111-4111-8111-111111111111">'
                         '<ChildList/></Layer></Layers></Scene></GeneralSceneDescription>')
    project = directory / 'mixed.pstg'
    with zipfile.ZipFile(project, 'w') as archive:
        archive.writestr('config.json', json.dumps(config))
        archive.writestr('scene.mvr', scene.getvalue())
    return project


class NativeInput:
    def __init__(self):
        self.x = C.CDLL(find_library('X11'))
        self.t = C.CDLL(find_library('Xtst'))
        self.x.XOpenDisplay.argtypes = [C.c_char_p]
        self.x.XOpenDisplay.restype = C.c_void_p
        self.display = self.x.XOpenDisplay(None)
        assert self.display, 'X11 display is unavailable'
        self.x.XFlush.argtypes = [C.c_void_p]
        self.x.XCloseDisplay.argtypes = [C.c_void_p]
        self.x.XDefaultRootWindow.argtypes = [C.c_void_p]
        self.x.XDefaultRootWindow.restype = C.c_ulong
        self.x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                                     C.c_uint, C.c_uint, C.c_ulong, C.c_int]
        self.x.XGetImage.restype = C.c_void_p
        self.x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
        self.x.XGetPixel.restype = C.c_ulong
        self.x.XDestroyImage.argtypes = [C.c_void_p]
        self.x.XStringToKeysym.argtypes = [C.c_char_p]
        self.x.XStringToKeysym.restype = C.c_ulong
        self.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
        self.x.XKeysymToKeycode.restype = C.c_uint
        self.t.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
        self.t.XTestFakeButtonEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
        self.t.XTestFakeMotionEvent.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_ulong]

    def key(self, name, down=True):
        code = self.x.XKeysymToKeycode(self.display, self.x.XStringToKeysym(name.encode()))
        assert code, name
        self.t.XTestFakeKeyEvent(self.display, code, down, 0)
        self.x.XFlush(self.display)

    def chord(self, *names):
        for name in names:
            self.key(name)
        for name in reversed(names):
            self.key(name, False)
        time.sleep(.2)

    def move(self, x, y):
        self.t.XTestFakeMotionEvent(self.display, -1, x, y, 0)
        self.x.XFlush(self.display)
        time.sleep(.05)

    def button(self, down=True):
        self.t.XTestFakeButtonEvent(self.display, 1, down, 0)
        self.x.XFlush(self.display)
        time.sleep(.05)

    def click(self, x, y, modifier=None):
        self.move(x, y)
        if modifier:
            self.key(modifier)
        self.button()
        self.button(False)
        if modifier:
            self.key(modifier, False)
        time.sleep(.2)

    def workspace(self, row):
        self.chord('Escape')
        self.click(98, 39)
        self.move(175, 363)
        time.sleep(.3)
        self.click(325, 363 + 27 * row)
        time.sleep(1)

    def paper(self):
        """Read the rendered white paper, independently of sidebar widths."""
        root = self.x.XDefaultRootWindow(self.display)
        image = self.x.XGetImage(self.display, root, 0, 0, 1600, 1000, 0xffffff, 2)
        assert image, 'Cannot read native Layout paper'

        def longest(coordinates, white):
            runs = []
            start = None
            for p in coordinates:
                if white(p):
                    if start is None:
                        start = p
                elif start is not None:
                    runs.append((start, p))
                    start = None
            return max(runs, key=lambda r: r[1] - r[0], default=(0, 0))

        def white(x, y):
            return self.x.XGetPixel(image, x, y) & 0xffffff == 0xffffff

        try:
            left, right = max(
                (longest(range(220, 1580), lambda x: white(x, y)) for y in range(150, 860, 20)),
                key=lambda r: r[1] - r[0])
            top, bottom = max(
                (longest(range(100, 960), lambda y: white(x, y)) for x in range(left + 12, right - 12, 14)),
                key=lambda r: r[1] - r[0], default=(0, 0))
            width, height = right - left, bottom - top
            assert width > 500 and height > 350 and abs(width / height - 297 / 210) < .05, (left, top, right, bottom)
            scale = width / (297 * 72 / 25.4)
            return lambda x, y: (round(left + x * scale), round(top + y * scale))
        finally:
            self.x.XDestroyImage(image)


def page(doc):
    return next(p for p in doc['layouts'] if p['name'] == 'Alignment smoke')


def frames(doc):
    result = {}
    for kind in ['view2dViews', 'legendViews', 'eventTables', 'textViews', 'imageViews']:
        for item in page(doc).get(kind, []):
            residual = item.get('viewport', {}).get('frameResidual', [0] * 4)
            result[(kind, item['id'])] = tuple(item['frame'][name] + residual[i]
                                             for i, name in enumerate(['x', 'y', 'width', 'height']))
    return result


def only_positions(before, after):
    a, b = copy.deepcopy(page(before)), copy.deepcopy(page(after))
    for p in [a, b]:
        for kind in ['view2dViews', 'legendViews', 'eventTables', 'textViews', 'imageViews']:
            for item in p.get(kind, []):
                item['frame']['x'] = item['frame']['y'] = 0
                if 'viewport' in item:
                    item['viewport']['frameResidual'][:2] = [0, 0]
    assert a == b, 'A non-position document property changed'


def exercise(app, directory, native):
    project = fixture(directory)
    env = dict(os.environ)
    for key in ['XDG_CACHE_HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_STATE_HOME']:
        env[key] = str(directory / key)
    with (directory / 'application.log').open('w') as log:
        process = subprocess.Popen([str(app), str(project)], cwd=app.parent, env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                assert process.poll() is None, 'Application exited before readiness'
                content = (directory / 'application.log').read_text(errors='replace')
                if any('operation=layout_render_rebuild result=completed' in line and 'images=1' in line
                       and 'active_layout="Alignment smoke"' in line for line in content.splitlines()):
                    break
                time.sleep(.2)
            else:
                raise AssertionError('Layout did not become ready')

            def save():
                previous = project.stat().st_mtime_ns
                native.chord('Control_L', 's')
                for _ in range(100):
                    if project.stat().st_mtime_ns != previous:
                        try:
                            with zipfile.ZipFile(project) as archive:
                                doc = json.loads(json.loads(archive.read('config.json'))['layouts_collection'])
                                with zipfile.ZipFile(io.BytesIO(archive.read('scene.mvr'))) as scene:
                                    payloads = {name: scene.read(name) for name in scene.namelist()}
                            return doc, payloads
                        except (zipfile.BadZipFile, KeyError):
                            pass
                    time.sleep(.1)
                raise AssertionError('Native Save did not publish the project')

            point = native.paper()
            native.click(*point(810, 550))
            baseline, scene = save()
            for (x, y), modifier in [((105, 85), None), ((310, 210), 'Control_L'),
                                     ((450, 320), 'Shift_L'), ((600, 415), 'Control_L'),
                                     ((720, 100), 'Shift_L')]:
                native.click(*point(x, y), modifier)
            # The existing shared toolbar: First selected, Align Left.
            native.click(1057, 66)
            native.click(1300, 66)
            aligned, payload = save()
            only_positions(baseline, aligned)
            a, b = frames(baseline), frames(aligned)
            assert b[('view2dViews', 1)] == a[('view2dViews', 1)]
            assert all(abs(f[0] - 40.25) <= .5 and f[1:] == a[ref][1:] for ref, f in b.items())
            assert payload == scene
            native.move(*point(105, 85))
            native.button()
            native.move(*point(125, 100))
            native.button(False)
            dragged, payload = save()
            only_positions(aligned, dragged)
            deltas = {(f[0] - b[ref][0], f[1] - b[ref][1]) for ref, f in frames(dragged).items()}
            assert len(deltas) == 1 and all(abs(dx - 20) <= 1 and abs(dy - 15) <= 1 for dx, dy in deltas), deltas
            assert payload == scene
            native.chord('Control_L', 'z')
            undone, _ = save()
            assert page(undone) == page(aligned), (page(undone), page(aligned))
            native.chord('Control_L', 'y')
            assert page(save()[0]) == page(dragged)
            native.move(*point(125, 100))
            native.button()
            native.move(*point(150, 125))
            native.chord('Escape')
            native.button(False)
            assert page(save()[0]) == page(dragged)
            for row in [1, 0, 2]:  # Scene 2D, scene 3D, Layout.
                native.workspace(row)
                doc, payload = save()
                assert page(doc) == page(dragged) and payload == scene
            point = native.paper()
            native.click(*point(810, 550))
            native.key('Shift_L')
            native.move(*point(10, 12))
            native.button()
            native.move(*point(810, 510))
            native.button(False)
            native.key('Shift_L', False)
            native.click(1081, 66)  # Last: image is last in stable marquee Z order.
            native.click(1252, 66)  # Top.
            top, payload = save()
            expected = frames(dragged)[('imageViews', 1)][1]
            assert all(abs(f[1] - expected) <= .5 for f in frames(top).values())
            assert frames(top)[('imageViews', 1)] == frames(dragged)[('imageViews', 1)]
            assert payload == scene
            native.click(1252, 66)  # Repeat must not add another Undo entry.
            native.chord('Control_L', 'z')
            assert page(save()[0]) == page(dragged)
            native.chord('Delete')
            deleted, payload = save()
            assert not frames(deleted) and payload == scene
            native.chord('Control_L', 'z')
            assert page(save()[0]) == page(dragged)
            # Restore a mixed selection after deletion/Undo before changing pages.
            point = native.paper()
            native.key('Shift_L')
            native.move(*point(10, 12))
            native.button()
            native.move(*point(810, 510))
            native.button(False)
            native.key('Shift_L', False)
            for y in [137, 115]:  # Both pages contain Text ID=1; neither inherits selection.
                native.click(70, y)
                native.click(1300, 66)
                native.chord('Delete')  # Empty document selection must consume Delete.
                doc, payload = save()
                assert doc['layouts'] == dragged['layouts'] and payload == scene
            print('PASS: native mixed selection, shared toolbar, group move, Undo/Redo, Escape, '
                  'workspace/page switching, marquee chronology and unchanged canonical MVR payloads.')
        except Exception:
            print((directory / 'application.log').read_text(errors='replace'))
            raise
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    args = parser.parse_args()
    if not os.environ.get('DISPLAY') or not find_library('X11') or not find_library('Xtst'):
        print('SKIP: native Layout smoke requires X11, XTest and a clean Xvfb display')
        return 77
    native = NativeInput()
    try:
        with tempfile.TemporaryDirectory(prefix='perastage-native-layout-') as directory:
            exercise(args.app.resolve(), Path(directory), native)
    finally:
        native.x.XCloseDisplay(native.display)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
