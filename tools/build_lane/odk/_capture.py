import unreal
sub = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
OUT = r"C:\Users\Sasan\wvrps_creators_build\odk\shots"
import os; os.makedirs(OUT, exist_ok=True)
steps = [(0, "connect"), (1, "assets")]
state = {"i": 0, "n": 0, "phase": "open"}

def tick(delta):
    state["n"] += 1
    if state["phase"] == "open":
        if state["n"] >= 6:
            tab, name = steps[state["i"]]
            p = os.path.join(OUT, name + ".png")
            ok = sub.capture_panel_screenshot(p)
            unreal.log("SHOT %s ok=%s -> %s" % (name, ok, p))
            state["i"] += 1
            state["n"] = 0
            if state["i"] < len(steps):
                state["phase"] = "open"
                sub.open_panel(steps[state["i"]][0])
            else:
                unreal.unregister_slate_post_tick_callback(tick)
                unreal.log("SHOT all done")

sub.open_panel(0)
unreal.register_slate_post_tick_callback(tick)
