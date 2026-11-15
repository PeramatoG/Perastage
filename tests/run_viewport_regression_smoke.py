#!/usr/bin/env python3
"""Run the real GUI against existing Linux Ninja application objects.

Build Perastage first with CMAKE_EXPORT_COMPILE_COMMANDS=ON, then run this script
under a desktop or xvfb-run. No production hooks or duplicate application build.
"""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
parser.add_argument('--source', type=Path, help='Alternative native smoke entry point')
parser.add_argument('--artifacts', type=Path, help='Copy smoke artifacts to this directory')
parser.add_argument('--nanovg-library', type=Path, help='Qualify another NanoVG font-size backend with the same app objects')
args = parser.parse_args()
build = args.build_dir.resolve()
root = Path(__file__).resolve().parents[1]
source = (args.source or (root / 'tests/viewport_regression_smoke.cpp')).resolve()
entries = json.loads((build / 'compile_commands.json').read_text())
main = next(entry for entry in entries if Path(entry['file']) == root / 'main.cpp')
compiler = shlex.split(main['command'])
original_object = compiler[compiler.index('-o') + 1]
commands = subprocess.check_output(['ninja', '-t', 'commands', 'Perastage'], cwd=build, text=True)
link = next(line for line in commands.splitlines() if ' -o Perastage ' in line)
with tempfile.TemporaryDirectory(prefix='viewport-smoke-', dir=build) as temporary:
    scratch = Path(temporary)
    obj = scratch / 'smoke.o'
    binary = build / 'viewport_regression_smoke'
    compiler[compiler.index('-o') + 1] = str(obj)
    compiler[compiler.index(str(root / 'main.cpp'))] = str(source)
    subprocess.run(compiler, cwd=build, check=True)
    # Retain the application's exact libraries, definitions and runtime paths.
    link = link.removeprefix(': && ').split(' && ', 1)[0]
    link = link.replace(original_object, shlex.quote(str(obj)))
    link = link.replace('-o Perastage ', '-o ' + shlex.quote(str(binary)) + ' ')
    if args.nanovg_library:
        tokens = shlex.split(link)
        indexes = [i for i, token in enumerate(tokens) if Path(token).name in ('libnanovg.a', 'libnanovg.so')]
        if len(indexes) != 1:
            raise RuntimeError('Expected one NanoVG library in the application link command')
        tokens[indexes[0]] = str(args.nanovg_library.resolve())
        link = shlex.join(tokens)
    try:
        subprocess.run(link, cwd=build, shell=True, check=True)
        for mode in ([], ['existing']):
            subprocess.run([str(binary), str(scratch), *mode], cwd=build, check=True, timeout=90)
    finally:
        if args.artifacts:
            shutil.copytree(scratch, args.artifacts, dirs_exist_ok=True)
        binary.unlink(missing_ok=True)
