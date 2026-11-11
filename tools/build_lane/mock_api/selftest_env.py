import json, time, urllib.request, urllib.parse

BASE = "http://127.0.0.1:8765"


def post(path, obj):
    r = urllib.request.Request(BASE + path, data=json.dumps(obj).encode(),
                               headers={"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(r, timeout=5).read())


def get(path, token=None):
    h = {"Authorization": "Bearer " + token} if token else {}
    return urllib.request.urlopen(urllib.request.Request(BASE + path, headers=h), timeout=5)


# wait for server
for _ in range(20):
    try:
        get("/mock/state")
        break
    except Exception:
        time.sleep(0.5)

# device-code flow
g = post("/api/device/grant_start.php", {"client": "creator"})
print("grant:", g["success"], "code", g["user_code"])
get("/creators?code=" + g["user_code"])  # approve
for _ in range(10):
    p = post("/api/device/grant_poll.php", {"grant_id": g["grant_id"], "claim_secret": g["claim_secret"]})
    if p.get("status") == "approved":
        break
    time.sleep(0.2)
tok = p.get("token")
print("token acquired:", bool(tok))

man = json.loads(get("/api/creator/catalog.php", tok).read())
biomes = man.get("biomes", [])
cats = man.get("categories", [])
env_items = [i for i in man["items"] if i.get("class") == "music_env"]
print("biomes:", len(biomes), "| categories:", len(cats), "| total items:", len(man["items"]), "| music_env:", len(env_items))
print("biome names:", ", ".join(b["name"] for b in biomes[:16]))
print("category labels:", ", ".join(c["label"] for c in cats))
print("sample music_env ids:", ", ".join(i["id"] for i in env_items[:5]))
# spot-check one env item has the fields the plugin parses
e = env_items[0]
print("first env item keys:", sorted(e.keys()))
print("first env item:", e["biome"], e["category"], e["engine"], e["loopable"], e["duration_sec"], e["format"])
