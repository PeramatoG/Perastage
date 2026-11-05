"""Normalize actual mixed-page PDFs to two pixels per document point.

PDF stream tests independently check all five bounds, nested CTMs and content.
These optional Poppler checks retain the Legend/Event Table AA limits.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess
import tempfile
from check_legend_raster_parity import read_ppm


def check(args, directory):
    subprocess.run([args.exporter, str(directory)], check=True, stdout=subprocess.DEVNULL)
    reference = {}
    for record in json.loads((directory / 'mapping.json').read_text()):
        source = (record['docW'], record['docH'])
        name = directory / record['file']
        subprocess.run([args.pdftoppm, '-r', str(144 / record['scale']), '-singlefile', str(name), str(name)], check=True)
        width, height, pixels = read_ppm(Path(str(name) + '.ppm'))
        w, h = math.floor(source[0]*2), math.floor(source[1]*2)
        assert width >= w and height >= h
        normalized = b''.join(pixels[(y*width)*3:(y*width+w)*3] for y in range(h))
        # The area below/right of the mapped source document must remain white,
        # even when the authored text frame extends past the source paper edge.
        for y in range(math.ceil(source[1]*2)+2, height-2):
            assert min(pixels[(y*width)*3:((y+1)*width)*3]) == 255, record
        for y in range(height):
            tail = pixels[(y*width+math.ceil(source[0]*2)+2)*3:((y+1)*width)*3]
            if tail:
                assert min(tail) == 255, record
        if source not in reference:
            reference[source] = normalized
            continue
        baseline = reference[source]
        mae = sum(abs(a-b) for a,b in zip(baseline, normalized)) / len(normalized)
        assert mae < 3, (record, mae)
        def bounds(data):
            ink = [(x,y) for y in range(2,h-2) for x in range(2,w-2)
                   if min(data[(y*w+x)*3:(y*w+x)*3+3]) < 160]
            assert ink
            return min(p[0] for p in ink), min(p[1] for p in ink), max(p[0] for p in ink), max(p[1] for p in ink)
        assert max(abs(a-b) for a,b in zip(bounds(baseline),bounds(normalized))) <= 1, record
        print(f"{record['file']}: MAE={mae:.4f}/255; ink bounds <=1 pixel")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exporter', required=True)
    parser.add_argument('--pdftoppm', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='layout-page-mapping-') as root:
        check(args, Path(root))
