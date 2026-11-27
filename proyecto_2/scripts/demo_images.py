#!/usr/bin/env python3
"""Image helpers for `make demo`: prepares the input photo and renders the outputs.

Usage (from proyecto_2):
    python3 -I scripts/demo_images.py prepare <photo> <input.pgm> <size>
    python3 -I scripts/demo_images.py render <demo-dir>

prepare  converts any photo to an 8-bit grayscale binary PGM (P5), center-cropped to size x size.
render   reads every <filter>_<backend>.pgm in <demo-dir>, writes a PNG per image under png/,
         builds montage.png (one row per filter, one column per backend) and checks that all the
         backends produced exactly the same image for each filter. Exits with 1 if any differ.
"""
import hashlib
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

FILTERS = ["gauss", "sobel"]
BACKEND_ORDER = ["seq", "seq_sep", "simd", "simd_sep", "simd512", "simd_sep512",
                 "cuda", "cuda_tiled", "fpga"]
TILE = 256
LABEL_HEIGHT = 16
OUTPUT_NAME = re.compile(r"^(?P<filter>[a-z]+)_(?P<backend>.+)\.pgm$")


def prepare(photo, destination, size):
    image = ImageOps.exif_transpose(Image.open(photo)).convert("L")
    ImageOps.fit(image, (size, size)).save(destination)
    print(f"  entrada: {destination} ({size}x{size}, escala de grises)")
    return 0


def find_outputs(demo_dir):
    outputs = {}
    for path in sorted(demo_dir.glob("*.pgm")):
        match = OUTPUT_NAME.match(path.name)
        if match and match["filter"] in FILTERS:
            outputs.setdefault(match["filter"], {})[match["backend"]] = path
    return outputs


def sorted_backends(outputs):
    found = {backend for by_backend in outputs.values() for backend in by_backend}
    known = [b for b in BACKEND_ORDER if b in found]
    return known + sorted(found - set(known))


def write_pngs(demo_dir, outputs):
    png_dir = demo_dir / "png"
    png_dir.mkdir(exist_ok=True)
    Image.open(demo_dir / "input.pgm").save(png_dir / "input.png")
    for filter_name, by_backend in outputs.items():
        for backend, path in by_backend.items():
            Image.open(path).save(png_dir / f"{filter_name}_{backend}.png")


def write_montage(demo_dir, outputs, backends):
    columns = len(backends) + 1
    rows = [f for f in FILTERS if f in outputs]
    row_height = TILE + LABEL_HEIGHT
    sheet = Image.new("L", (TILE * columns, row_height * len(rows)), 255)
    draw = ImageDraw.Draw(sheet)
    for row, filter_name in enumerate(rows):
        top = row * row_height
        tiles = [("entrada", demo_dir / "input.pgm")]
        tiles += [(b, outputs[filter_name].get(b)) for b in backends]
        for column, (name, path) in enumerate(tiles):
            draw.text((column * TILE + 4, top + 2), f"{filter_name}: {name}", fill=0)
            if path is not None:
                sheet.paste(Image.open(path).resize((TILE, TILE)), (column * TILE, top + LABEL_HEIGHT))
    montage = demo_dir / "montage.png"
    sheet.save(montage)
    return montage


def md5_of(path):
    return hashlib.md5(path.read_bytes()).hexdigest()


def report_identical(outputs):
    all_identical = True
    for filter_name in FILTERS:
        by_backend = outputs.get(filter_name)
        if not by_backend:
            continue
        hashes = {backend: md5_of(path) for backend, path in by_backend.items()}
        distinct = set(hashes.values())
        status = "IDENTICAS" if len(distinct) == 1 else "DIFERENTES"
        print(f"  {filter_name}: {len(hashes)} backends, {len(distinct)} hash(es) distinto(s) -> {status}")
        for backend, digest in hashes.items():
            print(f"      {backend:<12} {digest}")
        all_identical = all_identical and len(distinct) == 1
    return all_identical


def render(demo_dir):
    outputs = find_outputs(demo_dir)
    if not outputs or not (demo_dir / "input.pgm").exists():
        print(f"No hay salidas en {demo_dir}. Corra primero make demo_seq, demo_simd, demo_cuda o demo_fpga",
              file=sys.stderr)
        return 1
    write_pngs(demo_dir, outputs)
    montage = write_montage(demo_dir, outputs, sorted_backends(outputs))
    print(f"  PNG individuales: {demo_dir / 'png'}/")
    print(f"  montaje: {montage}")
    print("Comparacion byte a byte de las salidas:")
    if report_identical(outputs):
        return 0
    print("Hay backends con salidas distintas: revise los hashes de arriba", file=sys.stderr)
    return 1


def main(argv):
    if len(argv) == 5 and argv[1] == "prepare":
        return prepare(Path(argv[2]), Path(argv[3]), int(argv[4]))
    if len(argv) == 3 and argv[1] == "render":
        return render(Path(argv[2]))
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
