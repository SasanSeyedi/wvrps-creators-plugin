"""ODK smoke for WVRPS Creators inside the founder's MyProject (run via -ExecCmds="py <this>").

Read-only against the project content: opens the panel, starts a PRODUCTION pairing (grant_start needs no auth),
captures the code screen, waits briefly, cancels the pending grant, closes the panel and quits the editor.
Never approves (that needs the founder's Privy sign-in). No imports, no content saves.
"""
import json, os, time
import unreal

SHOTS = "F:/Estate/_scratch/CREATORS_HUB/screens_2026-11-04"
OUT = "C:/Users/Sasan/wvrps_creators_build/odk/odk_smoke_result.json"
os.makedirs(SHOTS, exist_ok=True)
sub = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
st = {"step": "warmup", "t0": time.time(), "ts": time.time(), "r": {"log": []}, "h": None}


def say(m):
    line = "[ODKSMOKE] %6.1fs %s" % (time.time() - st["t0"], m)
    st["r"]["log"].append(line)
    unreal.log(line)


def goto(s):
    st["step"] = s
    st["ts"] = time.time()
    say("-> " + s)


def waited(sec):
    return time.time() - st["ts"] >= sec


def dump():
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(st["r"], f, indent=1)


def tick(dt):
    try:
        s = st["step"]
        if s == "warmup" and waited(20):
            st["r"]["plugin_version_status_before"] = json.loads(sub.get_status_json())
            st["r"]["base_url"] = sub.get_base_url()
            st["r"]["signed_in_before"] = sub.is_signed_in()
            say("base_url=%s signed_in=%s" % (sub.get_base_url(), sub.is_signed_in()))
            sub.open_panel(0)
            goto("panel_open")
        elif s == "panel_open" and waited(4):
            p = SHOTS + "/odk_01_connect_idle.png"
            st["r"]["shot_idle"] = [p, bool(sub.capture_panel_screenshot(p))]
            if sub.is_signed_in():
                say("already signed in; not pairing")
                goto("done")
            else:
                sub.start_pairing(False)
                goto("pairing")
        elif s == "pairing":
            code = sub.get_user_code()
            if code and sub.get_pair_state() == "awaiting_approval" and waited(3):
                st["r"]["user_code"] = code
                st["r"]["approve_url"] = sub.get_approve_url()
                st["r"]["pair_state"] = sub.get_pair_state()
                say("code=%s url=%s" % (code, sub.get_approve_url()))
                p = SHOTS + "/odk_02_connect_code.png"
                st["r"]["shot_code"] = [p, bool(sub.capture_panel_screenshot(p))]
                goto("hold")
            elif waited(60):
                st["r"]["pair_state"] = sub.get_pair_state()
                st["r"]["last_status"] = json.loads(sub.get_status_json())
                say("no code after 60 s, state=%s" % sub.get_pair_state())
                goto("done")
        elif s == "hold" and waited(25):
            # full-editor-window proof shot is taken from outside; here just record the state again
            st["r"]["pair_state_after_hold"] = sub.get_pair_state()
            st["r"]["status_after_hold"] = json.loads(sub.get_status_json())
            p = SHOTS + "/odk_03_connect_code_after25s.png"
            st["r"]["shot_hold"] = [p, bool(sub.capture_panel_screenshot(p))]
            goto("about")
        elif s == "about":
            sub.open_panel(3)
            goto("about_shot")
        elif s == "about_shot" and waited(3):
            p = SHOTS + "/odk_04_about.png"
            st["r"]["shot_about"] = [p, bool(sub.capture_panel_screenshot(p))]
            sub.open_panel(0)
            goto("wait_quit_flag")
        elif s == "wait_quit_flag":
            # external driver takes a desktop screenshot, then creates this flag file
            if os.path.exists(OUT + ".quit") or waited(120):
                goto("done")
        elif s == "done":
            if sub.get_pair_state() == "awaiting_approval":
                sub.cancel_pairing()
                say("cancelled pending grant")
            st["r"]["final_status"] = json.loads(sub.get_status_json())
            st["r"]["subsystem_log"] = sub.get_log_text()
            sub.close_panel()
            dump()
            goto("quit")
        elif s == "quit" and waited(3):
            unreal.unregister_slate_post_tick_callback(st["h"])
            say("QUIT_EDITOR")
            unreal.SystemLibrary.quit_editor()
        if s not in ("quit",) and time.time() - st["t0"] > 600:
            say("global timeout")
            goto("done")
    except Exception as e:
        import traceback
        st["r"]["error"] = traceback.format_exc()
        say("ERROR " + repr(e))
        dump()
        goto("quit" if st["step"] in ("done", "quit") else "done")


st["h"] = unreal.register_slate_post_tick_callback(tick)
say("registered")
