import unreal, os
sub = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
OUT = r"C:\Users\Sasan\wvrps_creators_build\odk\shots"
state = {"phase": 0, "n": 0}

def tick(delta):
    state["n"] += 1
    ph = state["phase"]
    if ph == 0:
        if state["n"] == 2:
            sub.open_panel(1)
            sub.set_panel_environment_state(1, "", "", "")
        if state["n"] >= 150:
            sub.capture_panel_screenshot(os.path.join(OUT, "environments_biomes.png"))
            unreal.log("SHOT biomes")
            state["phase"] = 1; state["n"] = 0
    elif ph == 1:
        if state["n"] == 2:
            sub.set_panel_environment_state(1, "bog", "music", "explore")
        if state["n"] >= 90:
            sub.capture_panel_screenshot(os.path.join(OUT, "environments_clips.png"))
            unreal.log("SHOT clips")
            unreal.unregister_slate_post_tick_callback(tick)
            unreal.log("SHOT done")

unreal.register_slate_post_tick_callback(tick)
