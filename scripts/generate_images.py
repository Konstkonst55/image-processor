import argparse
import random
from pathlib import Path

from PIL import Image, ImageDraw

FORMATS = {
    ".png": "PNG",
    ".jpg": "JPEG",
    ".jpeg": "JPEG",
    ".bmp": "BMP",
    ".tga": "TGA",
}


def random_size():
    size_class = random.choice(("small", "medium", "large"))

    if size_class == "small":
        return random.randint(80, 320), random.randint(80, 320)

    if size_class == "medium":
        return random.randint(320, 1000), random.randint(240, 800)

    return random.randint(1000, 1920), random.randint(700, 1440)


def generate_image(path: Path, extension: str):
    width, height = random_size()
    background = tuple(random.randint(0, 255) for _ in range(3))
    image = Image.new("RGB", (width, height), background)
    draw = ImageDraw.Draw(image)

    for _ in range(random.randint(12, 45)):
        color = tuple(random.randint(0, 255) for _ in range(3))
        x1 = random.randint(0, width - 1)
        y1 = random.randint(0, height - 1)
        x2 = random.randint(x1, width - 1)
        y2 = random.randint(y1, height - 1)

        if random.choice((True, False)):
            draw.rectangle((x1, y1, x2, y2), fill=color)
        else:
            draw.ellipse((x1, y1, x2, y2), fill=color)

    image.save(path, format=FORMATS[extension])
    return width, height


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("count", type=int)
    parser.add_argument("--output-dir", default="images")
    args = parser.parse_args()

    if args.count <= 0:
        parser.error("count must be greater than zero")

    output_directory = Path(args.output_dir)
    output_directory.mkdir(parents=True, exist_ok=True)
    extensions = tuple(FORMATS)

    for index in range(1, args.count + 1):
        extension = random.choice(extensions)
        path = output_directory / f"image_{index:04d}{extension}"
        width, height = generate_image(path, extension)
        print(f"Created: {path} ({width}x{height})")

    print(f"Generated {args.count} images in {output_directory}")


if __name__ == "__main__":
    main()
