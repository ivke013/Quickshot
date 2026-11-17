"""Generate sharp white-on-transparent toolbar icons (64 and 32)."""
from __future__ import annotations

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
DIR64 = ROOT / "assets" / "toolbar" / "64"
DIR32 = ROOT / "assets" / "toolbar" / "32"
WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)
W = 64


def new_canvas() -> Image.Image:
    return Image.new("RGBA", (W, W), CLEAR)


def save(img: Image.Image, name: str) -> None:
    DIR64.mkdir(parents=True, exist_ok=True)
    DIR32.mkdir(parents=True, exist_ok=True)
    p64 = DIR64 / name
    img.save(p64, "PNG")
    img32 = img.resize((32, 32), Image.Resampling.LANCZOS)
    img32.save(DIR32 / name, "PNG")
    print(f"wrote {name}")


def icon_pencil() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    # diagonal pencil body
    body = [(18, 46), (22, 50), (48, 24), (44, 20)]
    d.polygon(body, fill=WHITE)
    # tip
    d.polygon([(18, 46), (14, 54), (22, 50)], fill=WHITE)
    # eraser end cut
    d.line([(44, 20), (48, 24)], fill=CLEAR, width=1)
    d.ellipse([45, 17, 51, 23], outline=WHITE, width=2)
    save(im, "icon_pencil.png")


def icon_line() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    d.line([(14, 50), (50, 14)], fill=WHITE, width=4)
    save(im, "icon_line.png")


def icon_rect() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    d.rectangle([14, 18, 50, 46], outline=WHITE, width=3)
    save(im, "icon_rect.png")


def icon_ellipse() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    d.ellipse([14, 18, 50, 46], outline=WHITE, width=3)
    save(im, "icon_ellipse.png")


def icon_text() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    try:
        font = ImageFont.truetype("segoeui.ttf", 36)
    except OSError:
        try:
            font = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 36)
        except OSError:
            font = ImageFont.load_default()
    # center T
    d.text((32, 28), "T", font=font, fill=WHITE, anchor="mm")
    d.line([(20, 50), (44, 50)], fill=WHITE, width=3)
    save(im, "icon_text.png")


def icon_color() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    d.ellipse([16, 16, 48, 48], outline=WHITE, width=3)
    d.ellipse([24, 24, 40, 40], fill=WHITE)
    save(im, "icon_color.png")


def icon_undo() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    # arc
    d.arc([14, 16, 50, 48], start=40, end=300, fill=WHITE, width=3)
    # arrow head
    d.polygon([(16, 22), (28, 18), (26, 30)], fill=WHITE)
    save(im, "icon_undo.png")


def icon_copy() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    d.rectangle([22, 12, 48, 42], outline=WHITE, width=3)
    d.rectangle([14, 22, 40, 52], outline=WHITE, width=3)
    # fill front sheet interior slightly so overlap reads clearly
    d.rectangle([17, 25, 37, 49], fill=CLEAR)
    save(im, "icon_copy.png")


def icon_save() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    # floppy body
    d.rounded_rectangle([14, 14, 50, 50], radius=3, outline=WHITE, width=3)
    d.rectangle([22, 14, 42, 26], outline=WHITE, width=2)
    d.rectangle([20, 34, 44, 48], outline=WHITE, width=2)
    save(im, "icon_save.png")


def icon_close() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    d.line([(18, 18), (46, 46)], fill=WHITE, width=4)
    d.line([(46, 18), (18, 46)], fill=WHITE, width=4)
    save(im, "icon_close.png")


def _letter(ch: str, italic: bool = False, underline: bool = False) -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    font_name = "C:/Windows/Fonts/segoeuib.ttf" if ch == "B" else "C:/Windows/Fonts/segoeui.ttf"
    if italic:
        font_name = "C:/Windows/Fonts/segoeuii.ttf"
    try:
        font = ImageFont.truetype(font_name, 34)
    except OSError:
        font = ImageFont.load_default()
    d.text((32, 30), ch, font=font, fill=WHITE, anchor="mm")
    if underline:
        d.line([(18, 48), (46, 48)], fill=WHITE, width=3)
    name = {"B": "icon_bold.png", "I": "icon_italic.png", "U": "icon_underline.png"}[ch]
    save(im, name)


def icon_font_inc() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    try:
        big = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 28)
        sm = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 18)
    except OSError:
        big = sm = ImageFont.load_default()
    d.text((24, 32), "A", font=big, fill=WHITE, anchor="mm")
    d.text((46, 18), "+", font=sm, fill=WHITE, anchor="mm")
    save(im, "icon_font_inc.png")


def icon_font_dec() -> None:
    im = new_canvas()
    d = ImageDraw.Draw(im)
    try:
        big = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 22)
        sm = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 18)
    except OSError:
        big = sm = ImageFont.load_default()
    d.text((24, 32), "A", font=big, fill=WHITE, anchor="mm")
    d.text((46, 18), "-", font=sm, fill=WHITE, anchor="mm")
    save(im, "icon_font_dec.png")


def main() -> None:
    icon_pencil()
    icon_line()
    icon_rect()
    icon_ellipse()
    icon_text()
    icon_color()
    icon_undo()
    icon_copy()
    icon_save()
    icon_close()
    _letter("B")
    _letter("I", italic=True)
    _letter("U", underline=True)
    icon_font_inc()
    icon_font_dec()
    print("done")


if __name__ == "__main__":
    main()
