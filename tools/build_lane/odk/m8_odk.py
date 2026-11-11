"""M8 in the founder's MyProject (ODK, production API): 3 imports + quota refresh + Pair guard + thumbnails + screenshots.
Leaves the editor OPEN on the WVRPS Creators Browse tab (no quit)."""
import json, os, time, traceback
import unreal
SHOTS = "F:/Estate/_scratch/CREATORS_HUB/screens_2026-11-04"
OUT = "C:/Users/Sasan/wvrps_creators_build/odk/m8_odk_result.json"
ITEMS = ["room:Node8.glb", "vault:186", "gear:cap-op-black:dragoon"]
sub = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
S = {"step": "warm", "t0": time.time(), "ts": time.time(), "R": {"log": []}, "h": None}
def say(m):
    l = "[M8-ODK] %6.1fs %s" % (time.time() - S["t0"], m); S["R"]["log"].append(l); unreal.log(l)
def goto(s): S["step"] = s; S["ts"] = time.time(); say("-> " + s)
def waited(x): return time.time() - S["ts"] >= x
def st(): return json.loads(sub.get_status_json())
def shot(n):
    p = "%s/%s.png" % (SHOTS, n); ok = sub.capture_panel_screenshot(p); S["R"].setdefault("shots", []).append([p, bool(ok)]); say("shot %s %s" % (p, ok))
def disk(s):
    t = 0
    for j in s["jobs"]:
        if j["id"] in ITEMS:
            for p in j["parts"]:
                f = p.get("cache_file"); t += os.path.getsize(f) if f and os.path.isfile(f) else 0
    return t
def tick(dt):
    try:
        R = S["R"]; s0 = S["step"]
        if s0 == "warm" and waited(8) and sub.get_catalog_count() > 0:
            x = st(); R["before"] = {k: x[k] for k in ("version", "signed_in", "who_tier", "quota_used_bytes", "catalog_count")}
            say("before %s" % R["before"]); sub.open_panel(1); goto("thumbs")
        elif s0 == "thumbs" and waited(10):
            R["thumbs"] = st().get("thumbs"); say("thumbs %s" % R["thumbs"]); shot("odk_thumbs_1")
            sub.start_pairing(False); R["pair_guard"] = [sub.get_pair_state(), sub.get_user_code()]; say("pair while signed in -> %s" % R["pair_guard"])
            sub.open_panel(0); goto("connect")
        elif s0 == "connect" and waited(2):
            shot("odk_m8_connect_signed_in")
            sub.refresh_who_am_i(); goto("q0")
        elif s0 == "q0" and waited(3):
            R["quota_before"] = st()["quota_used_bytes"]; say("quota before %s" % R["quota_before"])
            for i in ITEMS: say("queue %s %s" % (i, sub.import_item(i, "/Game/WVRPS")))
            sub.open_panel(2); goto("imports")
        elif s0 == "imports" and waited(3) and not sub.is_busy():
            goto("measure")
        elif s0 == "measure" and waited(5):
            x = st(); R["quota_after"] = x["quota_used_bytes"]; R["disk_bytes"] = disk(x)
            R["jobs"] = [{"id": j["id"], "state": j["state"], "msg": j["message"], "parts": [(p["role"], p.get("api_route"), p.get("api_item")) for p in j["parts"]]} for j in x["jobs"]]
            R["delta"] = R["quota_after"] - R["quota_before"]
            say("quota %s -> %s delta %s, disk bytes %s, jobs %s" % (R["quota_before"], R["quota_after"], R["delta"], R["disk_bytes"], R["jobs"]))
            shot("odk_m8_import_done"); sub.open_panel(1); goto("final")
        elif s0 == "final" and waited(4):
            R["thumbs_final"] = st().get("thumbs"); shot("odk_thumbs_2")
            R["plugin_log"] = sub.get_log_text().splitlines()[-60:]
            json.dump(R, open(OUT, "w"), indent=1); say("done; editor left open on the Browse tab")
            unreal.unregister_slate_post_tick_callback(S["h"])
        elif time.time() - S["t0"] > 600:
            json.dump(R, open(OUT, "w"), indent=1); say("TIMEOUT"); unreal.unregister_slate_post_tick_callback(S["h"])
    except Exception:
        say("EXC " + traceback.format_exc()); unreal.unregister_slate_post_tick_callback(S["h"])
S["h"] = unreal.register_slate_post_tick_callback(tick)
