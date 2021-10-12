#!/usr/bin/env python3
"""Convert a PNG image to a C header with raw RGBA pixel data, optionally scaled."""

import sys
from PIL import Image

def main():
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <input.png> <output.h> <array_name> [scale]", file=sys.stderr)
        print("  scale: 1=full, 2=half, 3=third, etc. (default 2)", file=sys.stderr)
        sys.exit(1)

    input_png = sys.argv[1]
    output_h = sys.argv[2]
    array_name = sys.argv[3]
    scale = int(sys.argv[4]) if len(sys.argv) > 4 else 2

    img = Image.open(input_png).convert("RGBA")
    w, h = img.size
    if scale > 1:
        nw = max(1, w // scale)
        nh = max(1, h // scale)
        img = img.resize((nw, nh), Image.LANCZOS)
        w, h = nw, nh

    pixels = list(img.getdata())

    with open(output_h, "w") as f:
        f.write(f"/* Auto-generated from {input_png}, scale 1/{scale} */\n")
        f.write(f"#ifndef PANIC_LOGO_DATA_H\n#define PANIC_LOGO_DATA_H\n\n")
        f.write(f"#define {array_name.upper()}_WIDTH  {w}\n")
        f.write(f"#define {array_name.upper()}_HEIGHT {h}\n\n")
        f.write(f"static const unsigned char {array_name}[{w * h * 4}] = {{\n")

        for i, (r, g, b, a) in enumerate(pixels):
            if i % 8 == 0:
                f.write("    ")
            f.write(f"0x{r:02x}, 0x{g:02x}, 0x{b:02x}, 0x{a:02x}")
            if i < len(pixels) - 1:
                f.write(", ")
            if i % 8 == 7:
                f.write("\n")

        f.write("\n};\n\n#endif\n")

    print(f"Generated {output_h}: {w}x{h}, {len(pixels)*4} bytes", file=sys.stderr)

if __name__ == "__main__":
    main()
