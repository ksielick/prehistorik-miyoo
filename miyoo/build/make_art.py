"""The port's own pictures, with no Olduvai marks (LEGAL.md "Project name and marks") and no
game content: the Ports list picture (250x360, Onion's box-art size) and the icon embedded in
the binary. Lettering in Freckle Face (assets/fonts, SIL OFL).
    py -3 miyoo/build/make_art.py
    -> miyoo/card/Roms/PORTS/Imgs/Prehistorik (1991).png, miyoo/art/icon.png"""
import os

from PIL import Image, ImageDraw, ImageFont

repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
font_path = os.path.join(repo, "assets", "fonts", "FreckleFace-Regular.ttf")


def backdrop(w, h):
    """A dusk sky over a dark hill: warm at the bottom."""
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / (h - 1)
        d.line([(0, y), (w, y)], fill=(int(30 + 190 * t * t), int(20 + 80 * t * t), int(48 - 20 * t)))
    d.ellipse([-w * 0.4, h * 0.78, w * 1.4, h * 1.5], fill=(26, 14, 10))
    return img


def text_centered(img, text, size, y, fill, outline=(20, 8, 6)):
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(font_path, size)
    while d.textlength(text, font=f) > img.width - 12 and size > 8:
        size -= 1
        f = ImageFont.truetype(font_path, size)
    x = (img.width - d.textlength(text, font=f)) / 2
    d.text((x, y), text, font=f, fill=fill, stroke_width=max(2, size // 12), stroke_fill=outline)


art = backdrop(250, 360)
text_centered(art, "PREHISTORIK", 44, 110, (255, 190, 60))
text_centered(art, "1991", 30, 168, (255, 230, 170))
text_centered(art, "Miyoo port", 20, 300, (240, 220, 200))
out = os.path.join(repo, "miyoo", "card", "Roms", "PORTS", "Imgs", "Prehistorik (1991).png")
os.makedirs(os.path.dirname(out), exist_ok=True)
art.save(out)
print(out)

icon = backdrop(128, 128)
text_centered(icon, "P", 96, 2, (255, 190, 60))
out = os.path.join(repo, "miyoo", "art", "icon.png")
os.makedirs(os.path.dirname(out), exist_ok=True)
icon.save(out)
print(out)
