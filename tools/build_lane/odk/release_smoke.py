import sys, time
sys.path.insert(0, r"C:\ODK\Installs\1\output\Windows\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rx
CMD = r'''
import gc, unreal
n = 0
for o in gc.get_objects():
    if isinstance(o, dict) and "step" in o and "h" in o and "r" in o and "t0" in o:
        try:
            unreal.unregister_slate_post_tick_callback(o["h"])
        except Exception as e:
            unreal.log("ODKSMOKE unregister err %r" % e)
        o["step"] = "released_to_founder"
        n += 1
unreal.log("[ODKSMOKE] released state machine(s): %d - editor left open for the founder" % n)
print("RELEASED", n)
'''
r = rx.RemoteExecution()
r.start()
t = time.time()
node = None
while time.time() - t < 8:
    for n in r.remote_nodes:
        if n.get("project_name") == "MyProject":
            node = n
    if node:
        break
    time.sleep(0.3)
print("NODE", node)
if node:
    r.open_command_connection(node["node_id"])
    res = r.run_command(CMD, exec_mode=rx.MODE_EXEC_FILE)
    print(res)
    r.close_command_connection()
r.stop()
