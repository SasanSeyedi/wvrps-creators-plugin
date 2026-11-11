import json, time, urllib.request

BASE = "http://127.0.0.1:8765"


def post(path, obj):
    return json.loads(urllib.request.urlopen(
        urllib.request.Request(BASE + path, data=json.dumps(obj).encode(),
                               headers={"Content-Type": "application/json"}), timeout=5).read())


def get(path, token=None):
    h = {"Authorization": "Bearer " + token} if token else {}
    return urllib.request.urlopen(urllib.request.Request(BASE + path, headers=h), timeout=5)


for _ in range(20):
    try:
        get("/mock/state")
        break
    except Exception:
        time.sleep(0.5)

g = post("/api/device/grant_start.php", {"client": "creator"})
get("/creators?code=" + g["user_code"])
for _ in range(10):
    p = post("/api/device/grant_poll.php", {"grant_id": g["grant_id"], "claim_secret": g["claim_secret"]})
    if p.get("status") == "approved":
        break
tok = p["token"]

man = json.loads(get("/api/creator/catalog.php", tok).read())
biomes = man.get("biomes", [])
cats = man.get("categories", [])
env = [i for i in man["items"] if i.get("class") == "music_env"]
print("PASS catalog" if len(biomes) == 16 and len(cats) == 8 and len(env) == 27 else "FAIL catalog",
      "| biomes", len(biomes), "| cats", len(cats), "| music_env", len(env))

# rewritten audio URL check
first = env[0]
has_wav = "wav_url" in first and "mp3_url" not in first and (first.get("bytes") or 0) > 0
print("PASS url-rewrite" if has_wav else "FAIL url-rewrite", "| wav_url:", first.get("wav_url", "")[:50], "| bytes:", first.get("bytes"))

# download a music_env item -> should 302 to the dummy wav, then serve RIFF bytes; quota charged
req = urllib.request.Request(BASE + "/api/creator/download.php?id=crimson_amb_dustwind_001",
                             headers={"Authorization": "Bearer " + tok})
resp = urllib.request.urlopen(req, timeout=5)  # follows the 302
body = resp.read()
is_wav = body[:4] == b"RIFF" and body[8:12] == b"WAVE"
st = json.loads(get("/mock/state").read())
charged = sum(st["quota"].values())
print("PASS download" if is_wav and charged > 0 else "FAIL download",
      "| got", len(body), "bytes | RIFF/WAVE:", is_wav, "| quota charged:", charged)
