"""Contract self-test of the mock server (stdlib). python selftest.py"""
import json, urllib.request, urllib.error

B = "http://127.0.0.1:8765"


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k):
        return None


op = urllib.request.build_opener(NoRedirect)


def call(method, path, body=None, tok=None):
    req = urllib.request.Request(B + path, method=method, data=json.dumps(body).encode() if body is not None else None)
    if tok:
        req.add_header("Authorization", "Bearer " + tok)
    try:
        r = op.open(req)
        return r.status, dict(r.headers), r.read()
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers), e.read()


s, _, b = call("POST", "/api/device/grant_start.php", {"client": "creator"})
g = json.loads(b); print("grant_start", s, {k: g[k] for k in ("success", "expires_in")}, "code_len", len(g["user_code"]))
s, _, b = call("POST", "/api/device/grant_poll.php", {"grant_id": g["grant_id"], "claim_secret": g["claim_secret"]}); print("poll#1", s, json.loads(b)["status"])
s, _, b = call("GET", "/creators?code=" + g["user_code"]); print("approve", s, b[-80:])
s, _, b = call("POST", "/api/device/grant_poll.php", {"grant_id": g["grant_id"], "claim_secret": g["claim_secret"]}); p = json.loads(b); print("poll#2", s, p["status"], "token" in p)
s, _, b = call("POST", "/api/device/grant_poll.php", {"grant_id": g["grant_id"], "claim_secret": g["claim_secret"]}); print("poll#3", s, json.loads(b), "(token only once)")
t = p["token"]
s, _, b = call("GET", "/api/creator/whoami.php", tok=t); print("whoami", s, json.loads(b)["tier"])
s, h, b = call("GET", "/api/creator/catalog.php", tok=t); m = json.loads(b); print("catalog", s, h.get("ETag"), len(m["items"]), m["generated"])
req = urllib.request.Request(B + "/api/creator/catalog.php"); req.add_header("Authorization", "Bearer " + t); req.add_header("If-None-Match", h.get("ETag"))
try:
    op.open(req)
except urllib.error.HTTPError as e:
    print("catalog If-None-Match", e.code)
for i in ("wvrps_mock_crate", "wvrps_holder_only", "wvrps_mock_big"):
    s, h, b = call("GET", "/api/creator/download.php?id=" + i, tok=t); print("download", i, s, h.get("Location", ""), b[:60])
s, h, b = call("GET", "/assets/does_not_exist.glb"); print("spa fallback", s, h.get("Content-Type"), b[:4])
s, h, b = call("GET", "/api/creator/whoami.php"); print("no bearer", s)
