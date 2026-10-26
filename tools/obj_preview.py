"""Top-down preview of an OBJ written by map2obj (vertex 4th value = shade)."""
import sys

from PIL import Image, ImageDraw


def main(src, dst, size=1024):
    verts = []
    faces = []
    with open(src) as f:
        for line in f:
            if line.startswith("v "):
                x, y, z, shade = line.split()[1:5]
                verts.append((float(x), float(y), float(z), int(shade)))
            elif line.startswith("f "):
                faces.append(tuple(int(i) - 1 for i in line.split()[1:4]))

    xs = [v[0] for v in verts]
    zs = [v[2] for v in verts]
    ys = [v[1] for v in verts]
    min_x, max_x, min_z, max_z = min(xs), max(xs), min(zs), max(zs)
    min_y, max_y = min(ys), max(ys)
    scale = (size - 20) / max(max_x - min_x, max_z - min_z)

    img = Image.new("RGB", (size, size), (12, 12, 16))
    draw = ImageDraw.Draw(img)
    faces.sort(key=lambda f: sum(verts[i][1] for i in f))
    for face in faces:
        pts = [(10 + (verts[i][0] - min_x) * scale, 10 + (max_z - verts[i][2]) * scale) for i in face]
        height = (sum(verts[i][1] for i in face) / 3 - min_y) / max(max_y - min_y, 1e-3)
        shade = verts[face[0]][3]
        c = int(shade * (0.45 + 0.55 * height))
        draw.polygon(pts, fill=(c, c, int(c * 0.95)))
    img.save(dst)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
