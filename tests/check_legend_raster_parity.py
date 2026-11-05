"""Compare production wx and selectable-vector PDF at a normalized page size.

Uses only Python's standard library and optional test tools Xvfb/Poppler.
One raster pixel is 0.25 document points. Plan/stream tests separately require
identical logical coordinates; these bounds allow renderer antialiasing/hinting.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess
import tempfile


def read_ppm(path):
    data = path.read_bytes()
    pos = 0
    tokens = []
    while len(tokens) < 4:
        if data[pos:pos+1] == b'#':
            pos = data.index(b'\n', pos) + 1
        elif data[pos] in b' \r\n\t':
            pos += 1
        else:
            end = pos
            while data[end] not in b' \r\n\t':
                end += 1
            tokens.append(data[pos:end])
            pos = end
    assert tokens[0] == b'P6' and tokens[3] == b'255', tokens
    pos += 2 if data[pos:pos+2] == b'\r\n' else 1
    width, height = map(int, tokens[1:3])
    pixels = data[pos:]
    assert len(pixels) == width * height * 3
    return width, height, pixels


def ink_bounds(pixels, width, height, clip):
    x, y, w, h = clip
    left, top = max(2, math.floor(x*4)), max(2, math.floor(y*4))
    right, bottom = min(width-2, math.ceil((x+w)*4)), min(height-2, math.ceil((y+h)*4))
    points = [(px, py) for py in range(top, bottom) for px in range(left, right)
              if min(pixels[(py*width+px)*3:(py*width+px)*3+3]) < 160]
    if not points:
        return None
    return min(p[0] for p in points), min(p[1] for p in points), max(p[0] for p in points), max(p[1] for p in points)


def verify(args, root):
    subprocess.run([args.pdf, str(root)], check=True)
    subprocess.run([args.xvfb, '-a', args.preview, str(root)], check=True)
    records = []
    for width in (120, 180, 300, 600):
        w, h, preview = read_ppm(root / f'parity-{width}-preview.ppm')
        clips = json.loads((root / f'parity-{width}-clips.json').read_text())
        for scale in ('0.500000', '1.000000', '2.000000'):
            name = f'parity-{width}-{scale}'
            subprocess.run([args.pdftoppm, '-scale-to-x', str(width*4), '-scale-to-y', '400',
                            '-singlefile', str(root / f'{name}.pdf'), str(root / name)], check=True)
            pw, ph, pdf = read_ppm(root / f'{name}.ppm')
            assert (pw, ph) == (w, h)
            samples = [abs(preview[(y*w+x)*3+c]-pdf[(y*w+x)*3+c])
                       for y in range(2,h-2) for x in range(2,w-2) for c in range(3)]
            mae = sum(samples) / len(samples)
            assert mae < 3, (width, scale, mae)
            max_delta = 0
            for clip in clips:
                a = ink_bounds(preview,w,h,clip)
                b = ink_bounds(pdf,w,h,clip)
                assert (a is None) == (b is None), (width,scale,clip,a,b)
                if a:
                    delta = max(abs(x-y) for x,y in zip(a,b))
                    assert delta <= 1, (width,scale,clip,a,b)
                    max_delta = max(max_delta,delta)
            records.append(dict(width=width,scale=scale,mae_255=round(mae,4),max_ink_bounds_delta_pixels=max_delta))
    (root / 'raster-comparison.json').write_text(json.dumps(records,indent=2)+'\n')
    print(json.dumps(records,indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    for name in ('preview','pdf','xvfb','pdftoppm'):
        parser.add_argument('--'+name, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True,exist_ok=True)
        verify(args,args.output)
    else:
        with tempfile.TemporaryDirectory(prefix='perastage-legend-parity-') as directory:
            verify(args,Path(directory))
