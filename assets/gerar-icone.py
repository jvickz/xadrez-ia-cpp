"""Render the original code-native chess mark into PNG and a multi-size Windows ICO.

Uses the same knight silhouette as pieces.hpp and assets/logo.svg.
"""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
SCALE = 12
image = Image.new("RGBA", (1200, 1200), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)


def box(coords):
    return tuple(round(c * SCALE) for c in coords)


def point(coords):
    return tuple(round(c * SCALE) for c in coords)


def curve(start, control1, control2, end):
    result = []
    for step in range(1, 65):
        t = step / 64
        u = 1 - t
        result.append(tuple(u**3 * start[i] + 3 * u**2 * t * control1[i] +
                            3 * u * t**2 * control2[i] + t**3 * end[i] for i in (0, 1)))
    return result


def shape(points, fill):
    scaled = [point(p) for p in points]
    draw.polygon(scaled, fill=fill)
    draw.line(scaled + [scaled[0]], fill="#192c23", width=round(2.2 * SCALE), joint="curve")


draw.rounded_rectangle(box((2, 2, 98, 98)), radius=21*SCALE, fill="#243b30")
draw.rounded_rectangle(box((5, 5, 95, 95)), radius=18*SCALE, outline="#43664c", width=SCALE)
draw.ellipse(box((21, 82, 79, 92)), fill="#192c23")
head = [(30, 72), (35, 55), (24, 56)]
head += curve((24, 56), (17, 53), (22, 46), (26, 41))
head += [(41, 25), (41, 13), (54, 20)]
head += curve((54, 20), (75, 26), (77, 53), (67, 73))
shape(head, "#f5f0d6")
mane = [(56, 28)] + curve((56, 28), (68, 39), (64, 53), (55, 62))
draw.line([point(p) for p in mane], fill="#b8af8d", width=round(2.4*SCALE), joint="curve")
draw.ellipse(box((39.6, 33.6, 44.4, 38.4)), fill="#243b30")
base = [(29, 73), (71, 73)] + curve((71, 73), (75, 77), (78, 81), (77, 86))
base += [(23, 86)] + curve((23, 86), (22, 81), (25, 77), (29, 73))
shape(base, "#f5f0d6")
draw.line([point((29, 80)), point((71, 80))], fill="#b8af8d", width=round(1.7*SCALE))
for rect in [(75, 17, 81, 23), (81, 23, 87, 29)]:
    draw.rounded_rectangle(box(rect), radius=SCALE, fill="#81b64c")

image.resize((512, 512), Image.Resampling.LANCZOS).save(ROOT / "logo.png")
icon = image.resize((256, 256), Image.Resampling.LANCZOS)
icon.save(ROOT / "xadrez.ico", sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
print("Created logo.png and xadrez.ico (16–256 px).")
