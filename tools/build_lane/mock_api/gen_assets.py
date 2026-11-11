"""Generate the mock Creator API assets (stdlib only: no numpy/trimesh/pygltflib on this machine).

Writes into ./assets and ./thumbs:
  mock_crate.glb      static cube (12 tris)                      -> 1 StaticMesh
  mock_skinned.glb    skinned column on Root/Spine/Head (3 joints) -> 1 SkeletalMesh + 1 Skeleton
  mock_body.glb       skinned body, same 3-joint skeleton          -> MML body
  mock_hat.glb        skinned gear on the SAME joint names          -> MML gear child (binds to body skeleton)
  mock_character.mml  <m-character src=body><m-model src=hat/></m-character>
  dragoon_real.mml    real public Dragoon body + real bound gear (S3, read-only)
  mock_track.mp3      a tiny fake mp3 (ID3 header + zeros) - download-only class
  thumbs/<id>.png     128x128 brand-colour thumbnails
"""
import json, os, struct, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
A = os.path.join(HERE, "assets")
T = os.path.join(HERE, "thumbs")
os.makedirs(A, exist_ok=True)
os.makedirs(T, exist_ok=True)


def pad4(b, fill=b"\x00"):
    return b + fill * ((4 - len(b) % 4) % 4)


def glb(gltf, binblob):
    binblob = pad4(binblob)
    gltf["buffers"] = [{"byteLength": len(binblob)}]
    js = pad4(json.dumps(gltf, separators=(",", ":")).encode(), b" ")
    total = 12 + 8 + len(js) + 8 + len(binblob)
    return (struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(js), b"JSON") + js
            + struct.pack("<I4s", len(binblob), b"BIN\x00") + binblob)


class Bin:
    def __init__(self):
        self.data = b""
        self.views = []
        self.accs = []

    def add(self, raw, comp, typ, count, target=None, minmax=None):
        off = len(self.data)
        self.data = pad4(self.data + raw)
        v = {"buffer": 0, "byteOffset": off, "byteLength": len(raw)}
        if target:
            v["target"] = target
        self.views.append(v)
        a = {"bufferView": len(self.views) - 1, "componentType": comp, "count": count, "type": typ}
        if minmax:
            a["min"], a["max"] = minmax
        self.accs.append(a)
        return len(self.accs) - 1


def f32(vals):
    return struct.pack("<%df" % len(vals), *vals)


def bounds(pos):
    xs, ys, zs = pos[0::3], pos[1::3], pos[2::3]
    return ([min(xs), min(ys), min(zs)], [max(xs), max(ys), max(zs)])


def material(name, rgba):
    return {"name": name, "pbrMetallicRoughness": {"baseColorFactor": rgba, "metallicFactor": 0.0, "roughnessFactor": 0.6}}


def cube():
    b = Bin()
    pos, nrm, idx = [], [], []
    faces = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, 1, 0), (0, 0, -1)), ((0, 1, 0), (0, 0, 1), (1, 0, 0)),
             ((0, -1, 0), (0, 0, -1), (1, 0, 0)), ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (-1, 0, 0), (0, 1, 0))]
    for n, u, v in faces:
        base = len(pos) // 3
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            for k in range(3):
                pos.append(0.5 * (n[k] + su * u[k] + sv * v[k]))
            nrm.extend(n)
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    ap = b.add(f32(pos), 5126, "VEC3", len(pos) // 3, 34962, bounds(pos))
    an = b.add(f32(nrm), 5126, "VEC3", len(nrm) // 3, 34962)
    ai = b.add(struct.pack("<%dH" % len(idx), *idx), 5123, "SCALAR", len(idx), 34963)
    g = {"asset": {"version": "2.0", "generator": "wvrps mock gen_assets.py"},
         "scene": 0, "scenes": [{"nodes": [0]}],
         "nodes": [{"name": "WVRPS_Crate", "mesh": 0}],
         "meshes": [{"name": "WVRPS_Crate", "primitives": [{"attributes": {"POSITION": ap, "NORMAL": an}, "indices": ai, "material": 0}]}],
         "materials": [material("M_WVRPS_Teal", [0.145, 0.851, 0.878, 1.0])],
         "accessors": b.accs, "bufferViews": b.views}
    return glb(g, b.data)


JOINT_Y = [0.0, 1.0, 2.0]   # world Y of Root, Spine, Head (each child is +1 Y from its parent)


def skinned(mesh_name, levels, mat):
    """levels: list of (y, half_width, joint_index). Quad strip in XY, skinned 100% to the level's joint."""
    b = Bin()
    pos, nrm, jnt, wgt, idx = [], [], [], [], []
    for (y, w, j) in levels:
        for x in (-w, w):
            pos += [x, y, 0.0]
            nrm += [0.0, 0.0, 1.0]
            jnt += [j, 0, 0, 0]
            wgt += [1.0, 0.0, 0.0, 0.0]
    for i in range(len(levels) - 1):
        a, c = 2 * i, 2 * (i + 1)
        idx += [a, a + 1, c + 1, a, c + 1, c]
    ap = b.add(f32(pos), 5126, "VEC3", len(pos) // 3, 34962, bounds(pos))
    an = b.add(f32(nrm), 5126, "VEC3", len(nrm) // 3, 34962)
    aj = b.add(struct.pack("<%dH" % len(jnt), *jnt), 5123, "VEC4", len(jnt) // 4, 34962)
    aw = b.add(f32(wgt), 5126, "VEC4", len(wgt) // 4, 34962)
    ai = b.add(struct.pack("<%dH" % len(idx), *idx), 5123, "SCALAR", len(idx), 34963)
    ibm = []
    for y in JOINT_Y:  # inverse of a pure translation, column-major
        ibm += [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -y, 0, 1]
    aibm = b.add(f32(ibm), 5126, "MAT4", 3)
    g = {"asset": {"version": "2.0", "generator": "wvrps mock gen_assets.py"},
         "scene": 0, "scenes": [{"nodes": [0, 3]}],
         "nodes": [{"name": "Root", "children": [1]},
                   {"name": "Spine", "translation": [0, 1, 0], "children": [2]},
                   {"name": "Head", "translation": [0, 1, 0]},
                   {"name": mesh_name, "mesh": 0, "skin": 0}],
         "skins": [{"name": "WVRPS_Mock_Skin", "joints": [0, 1, 2], "skeleton": 0, "inverseBindMatrices": aibm}],
         "meshes": [{"name": mesh_name, "primitives": [{"attributes": {"POSITION": ap, "NORMAL": an, "JOINTS_0": aj, "WEIGHTS_0": aw}, "indices": ai, "material": 0}]}],
         "materials": [mat],
         "accessors": b.accs, "bufferViews": b.views}
    return glb(g, b.data)


def png(path, w, h, bg, fg):
    rows = b""
    for y in range(h):
        row = b"\x00"
        for x in range(w):
            dx, dy = x - w / 2, y - h / 2
            inside = (dx * dx + dy * dy) < (w * 0.3) ** 2
            band = abs(y - h * 0.78) < h * 0.04
            row += bytes(fg if (inside or band) else bg)
        rows += row

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    data = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
    open(path, "wb").write(data)


def main():
    teal, pink = [0.145, 0.851, 0.878, 1.0], [1.0, 0.184, 0.616, 1.0]
    open(os.path.join(A, "mock_crate.glb"), "wb").write(cube())
    open(os.path.join(A, "mock_skinned.glb"), "wb").write(
        skinned("WVRPS_Skinned", [(0.0, 0.3, 0), (0.9, 0.35, 1), (1.6, 0.3, 1), (2.2, 0.25, 2)], material("M_Skinned", teal)))
    open(os.path.join(A, "mock_body.glb"), "wb").write(
        skinned("WVRPS_MockBody", [(0.0, 0.3, 0), (1.0, 0.4, 1), (2.0, 0.3, 2), (2.4, 0.2, 2)], material("M_Body", teal)))
    open(os.path.join(A, "mock_hat.glb"), "wb").write(
        skinned("WVRPS_MockHat", [(2.4, 0.35, 2), (2.6, 0.25, 2), (2.9, 0.05, 2)], material("M_Hat", pink)))
    open(os.path.join(A, "mock_character.mml"), "w").write(
        '<m-character src="mock_body.glb">\n  <m-model src="mock_hat.glb" socket="Head"></m-model>\n</m-character>\n')
    open(os.path.join(A, "dragoon_real.mml"), "w").write(
        '<m-character src="https://wvrps-mml-assets.s3.amazonaws.com/Dragoon.glb">\n'
        '  <m-model src="https://wvrps-atalier.s3.us-east-1.amazonaws.com/gear/bound/dragoon/boom-hammer.b12c2659ef.glb"></m-model>\n'
        '</m-character>\n')
    open(os.path.join(A, "mock_track.mp3"), "wb").write(b"ID3\x03\x00\x00\x00\x00\x00\x00" + b"\x00" * 2048)
    thumbs = {
        "wvrps_mock_crate": ((8, 9, 12), (37, 217, 224)),
        "wvrps_mock_skinned": ((16, 19, 25), (37, 217, 224)),
        "wvrps_mock_character": ((16, 19, 25), (255, 47, 157)),
        "wvrps_dragoon_real": ((8, 9, 12), (232, 163, 61)),
        "wvrps_token_1": ((8, 9, 12), (255, 98, 183)),
        "wvrps_mock_track": ((22, 26, 34), (37, 217, 224)),
        "wvrps_spa_trap": ((22, 26, 34), (255, 59, 48)),
        "wvrps_mock_big": ((22, 26, 34), (154, 163, 175)),
        "wvrps_holder_only": ((22, 26, 34), (255, 47, 157)),
    }
    for k, (bg, fg) in thumbs.items():
        png(os.path.join(T, k + ".png"), 128, 128, bg, fg)
    for f in sorted(os.listdir(A)):
        print(f, os.path.getsize(os.path.join(A, f)))


if __name__ == "__main__":
    main()
