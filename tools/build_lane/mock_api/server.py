"""Mock WVRPS Creator API (M2 contract) for the WVRPS Creators UE plugin golden test. Stdlib only.

python server.py [--port 8765]

Contract served (exactly as the M4 brief):
  POST /api/device/grant_start.php  {client:'creator'} -> {success, grant_id, claim_secret, user_code(8 digits), expires_in}
  POST /api/device/grant_poll.php   {grant_id, claim_secret} -> {success, status, token?, jti?, exp?}  (token once)
  GET  /api/creator/whoami.php      Bearer -> {did, wallet, label, exp, tier, license}
  GET  /api/creator/catalog.php     Bearer, ETag / If-None-Match -> manifest {generated, items:[...]}
  GET  /api/creator/download.php?id Bearer -> 302 public url | 429 {error:'quota', reset_at} | 403
Browser approval stand-in:
  GET  /creators?code=<user_code>   approves the grant (what wvrps.com/creators does after sign-in)
Like the real SPA host, ANY unknown path returns 200 text/html (so clients must check the glTF magic).
Debug: GET /mock/state -> JSON of grants/quota/log.
"""
import argparse, hashlib, json, os, secrets, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

HERE = os.path.dirname(os.path.abspath(__file__))
LICENSE = ("You may use these assets in your own games and projects, including commercially. "
           "You may not resell the assets or transfer any commercial rights to anyone else.")
QUOTA_BYTES = 200 * 1024 * 1024
LOCK = threading.RLock()   # re-entrant: send_response -> log_message also takes it
GRANTS = {}   # grant_id -> dict
TOKENS = {}   # token -> dict
QUOTA = {}    # token -> bytes used today
LOG = []
GENERATED = "2026-11-04T09:00:00Z"
SEED_JSON = r"F:\Estate\Claude\_scratch\CREATORS_HUB_M10_ENVIRONMENTS\environments_seed.json"


def items(base):
    a = base + "/assets/"
    t = base + "/thumbs/"
    sz = lambda f: os.path.getsize(os.path.join(HERE, "assets", f)) if os.path.exists(os.path.join(HERE, "assets", f)) else 0
    return [
        {"id": "wvrps_mock_crate", "class": "ship_room", "name": "Mock Crate (static)", "glb_url": a + "mock_crate.glb",
         "thumb_url": t + "wvrps_mock_crate.png", "bytes": sz("mock_crate.glb"), "tier": "public", "license": "wvrps-creator-1", "source": "mock"},
        {"id": "wvrps_mock_skinned", "class": "house_gear", "name": "Mock Skinned Column", "glb_url": a + "mock_skinned.glb",
         "thumb_url": t + "wvrps_mock_skinned.png", "bytes": sz("mock_skinned.glb"), "tier": "public", "license": "wvrps-creator-1", "source": "mock"},
        {"id": "wvrps_mock_character", "class": "vault_nft", "name": "Mock Character (body + hat)", "mml_url": a + "mock_character.mml",
         "thumb_url": t + "wvrps_mock_character.png", "bytes": sz("mock_body.glb") + sz("mock_hat.glb"), "tier": "public", "license": "wvrps-creator-1", "source": "mock"},
        {"id": "wvrps_dragoon_real", "class": "body", "name": "DJ Dragoon + Boom Hammer (live S3)", "mml_url": a + "dragoon_real.mml",
         "thumb_url": t + "wvrps_dragoon_real.png", "bytes": 5469576 + 9142232, "tier": "public", "license": "wvrps-creator-1", "source": "s3:wvrps-mml-assets+wvrps-atalier"},
        {"id": "wvrps_token_1", "class": "vault_nft", "name": "WVRPS #1 (live MML)", "mml_url": "https://wvrps-mml-links.s3.us-east-1.amazonaws.com/1.mml",
         "thumb_url": t + "wvrps_token_1.png", "bytes": 0, "tier": "public", "license": "wvrps-creator-1", "source": "s3:wvrps-mml-links"},
        {"id": "wvrps_mock_track", "class": "music", "name": "Mock Track", "mp3_url": a + "mock_track.mp3",
         "thumb_url": t + "wvrps_mock_track.png", "bytes": sz("mock_track.mp3"), "tier": "public", "license": "wvrps-creator-1", "source": "mock"},
        {"id": "wvrps_spa_trap", "class": "chess", "name": "SPA fallback trap (missing file)", "glb_url": a + "does_not_exist.glb",
         "thumb_url": t + "wvrps_spa_trap.png", "bytes": 1000, "tier": "public", "license": "wvrps-creator-1", "source": "mock"},
        {"id": "wvrps_mock_big", "class": "voxel_lego", "name": "Quota trap (250 MB)", "voxel_raw_url": a + "mock_crate.glb",
         "thumb_url": t + "wvrps_mock_big.png", "bytes": 250 * 1024 * 1024, "tier": "public", "license": "wvrps-creator-1", "source": "mock"},
        {"id": "wvrps_holder_only", "class": "vault_nft", "name": "Holder-only twin", "glb_url": a + "mock_crate.glb",
         "thumb_url": t + "wvrps_holder_only.png", "bytes": 1000, "tier": "holder", "license": "wvrps-creator-1", "source": "mock"},
    ]


def seed_manifest():
    """Load the M10 environments seed (biomes + categories + music_env items)."""
    try:
        with open(SEED_JSON, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def all_items(base):
    """Mock GLB/mesh items + the environments seed items (music_env), their audio URL rewritten
    to a locally-served dummy WAV so import (SoundWave + optional cue/blueprint) can be exercised."""
    out = list(items(base))
    dummy = base + "/assets/dummy_env.wav"
    dummy_bytes = os.path.getsize(os.path.join(HERE, "assets", "dummy_env.wav")) \
        if os.path.exists(os.path.join(HERE, "assets", "dummy_env.wav")) else 0
    for it in seed_manifest().get("items", []):
        it = dict(it)
        it.pop("mp3_url", None)
        it["wav_url"] = dummy
        it["bytes"] = dummy_bytes
        out.append(it)
    return out


def biome_rows(base):
    """Seed biomes with their art rewritten to locally-served PNG files (present after the copy step)."""
    out = []
    for b in seed_manifest().get("biomes", []):
        b = dict(b)
        slug = b.get("id", "")
        local = os.path.join(HERE, "thumbs", "biomes", slug + ".png")
        if os.path.exists(local):
            b["thumb_url"] = base + "/thumbs/biomes/" + slug + ".png"
        out.append(b)
    return out


class H(BaseHTTPRequestHandler):
    server_version = "WVRPSMock/1.0"

    def log_message(self, fmt, *args):
        line = "%s %s" % (time.strftime("%H:%M:%S"), fmt % args)
        with LOCK:
            LOG.append(line)
        print(line, flush=True)

    def base(self):
        return "http://%s" % (self.headers.get("Host") or "127.0.0.1:8765")

    def send_json(self, code, obj, extra=None):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def spa(self):
        body = b"<!doctype html><html><head><title>WVRPS</title></head><body><div id=root></div></body></html>"
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def bearer(self):
        h = self.headers.get("Authorization", "")
        if not h.startswith("Bearer "):
            return None
        tok = h[7:].strip()
        with LOCK:
            t = TOKENS.get(tok)
        if not t or t["exp"] < time.time():
            return None
        return tok

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n) if n else b"{}"
        try:
            return json.loads(raw or b"{}")
        except Exception:
            return {}

    def do_POST(self):
        p = urlparse(self.path).path
        b = self.body()
        if p == "/api/device/grant_start.php":
            if b.get("client") != "creator":
                return self.send_json(400, {"success": False, "error": "client"})
            gid, sec = secrets.token_hex(8), secrets.token_hex(16)
            code = "%08d" % secrets.randbelow(10 ** 8)
            with LOCK:
                GRANTS[gid] = {"secret": sec, "code": code, "exp": time.time() + 300, "approved": False, "delivered": False}
            return self.send_json(200, {"success": True, "grant_id": gid, "claim_secret": sec, "user_code": code, "expires_in": 300})
        if p == "/api/device/grant_poll.php":
            with LOCK:
                g = GRANTS.get(b.get("grant_id", ""))
                if not g or g["secret"] != b.get("claim_secret"):
                    return self.send_json(403, {"success": False, "error": "grant"})
                if g["exp"] < time.time():
                    return self.send_json(200, {"success": True, "status": "expired"})
                if not g["approved"]:
                    return self.send_json(200, {"success": True, "status": "awaiting_approval"})
                if g["delivered"]:
                    return self.send_json(200, {"success": True, "status": "approved"})
                g["delivered"] = True
                tok = "mock." + secrets.token_urlsafe(24)
                jti = secrets.token_hex(8)
                exp = int(time.time()) + 30 * 24 * 3600
                TOKENS[tok] = {"jti": jti, "exp": exp, "tier": "public"}
            return self.send_json(200, {"success": True, "status": "approved", "token": tok, "jti": jti, "exp": exp})
        return self.spa()

    def do_GET(self):
        u = urlparse(self.path)
        p, q = u.path, parse_qs(u.query)
        if p == "/creators":
            code = (q.get("code") or [""])[0]
            with LOCK:
                hit = [g for g in GRANTS.values() if g["code"] == code and g["exp"] > time.time()]
                for g in hit:
                    g["approved"] = True
            msg = ("Approved code %s. Return to Unreal." % code) if hit else "Unknown or expired code."
            body = ("<html><body style='background:#08090C;color:#fff;font-family:sans-serif'><h2>WVRPS Creators (mock)</h2><p>%s</p></body></html>" % msg).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            return self.wfile.write(body)
        if p == "/mock/state":
            with LOCK:
                return self.send_json(200, {"grants": len(GRANTS), "tokens": len(TOKENS), "quota": QUOTA, "log_tail": LOG[-40:]})
        if p.startswith("/assets/") or p.startswith("/thumbs/"):
            f = os.path.normpath(os.path.join(HERE, p.lstrip("/")))
            if f.startswith(HERE) and os.path.isfile(f):
                data = open(f, "rb").read()
                self.send_response(200)
                ct = "model/gltf-binary" if f.endswith(".glb") else "image/png" if f.endswith(".png") else "image/jpeg" if f.endswith(".jpg") or f.endswith(".jpeg") else "text/plain" if f.endswith(".mml") else "application/octet-stream"
                self.send_header("Content-Type", ct)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                return self.wfile.write(data)
            return self.spa()   # SPA fallback, exactly like the live host
        if p.startswith("/api/creator/"):
            tok = self.bearer()
            if not tok:
                return self.send_json(401, {"error": "auth"})
            if p == "/api/creator/whoami.php":
                t = TOKENS[tok]
                return self.send_json(200, {"did": "did:privy:mock-creator", "wallet": "0x0000000000000000000000000000000000c0ffee",
                                            "label": "Mock Creator", "exp": t["exp"], "tier": t["tier"], "license": LICENSE})
            if p == "/api/creator/catalog.php":
                seed = seed_manifest()
                man = {"generated": GENERATED, "license": LICENSE,
                       "biomes": biome_rows(self.base()), "categories": seed.get("categories", []),
                       "items": all_items(self.base())}
                raw = json.dumps(man, sort_keys=True).encode()
                etag = '"%s"' % hashlib.sha1(raw).hexdigest()[:16]
                if self.headers.get("If-None-Match") == etag:
                    self.send_response(304)
                    self.send_header("ETag", etag)
                    self.end_headers()
                    return
                return self.send_json(200, man, {"ETag": etag})
            if p == "/api/creator/download.php":
                iid = (q.get("id") or [""])[0]
                it = next((i for i in all_items(self.base()) if i["id"] == iid), None)
                if not it:
                    return self.send_json(404, {"error": "not_found"})
                if it["tier"] == "holder" and TOKENS[tok]["tier"] != "holder":
                    return self.send_json(403, {"error": "tier"})
                with LOCK:
                    used = QUOTA.get(tok, 0)
                    if used + it["bytes"] > QUOTA_BYTES:
                        reset = time.strftime("%Y-%m-%dT00:00:00Z", time.gmtime(time.time() + 86400))
                        return self.send_json(429, {"error": "quota", "reset_at": reset})
                    QUOTA[tok] = used + it["bytes"]
                url = it.get("glb_url") or it.get("mml_url") or it.get("voxel_raw_url") or it.get("mp3_url") or it.get("wav_url")
                self.send_response(302)
                self.send_header("Location", url)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            return self.send_json(404, {"error": "endpoint"})
        return self.spa()


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    a = ap.parse_args()
    print("WVRPS mock Creator API on http://127.0.0.1:%d" % a.port, flush=True)
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()
