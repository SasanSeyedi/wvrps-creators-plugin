
import sys, time, json
sys.path.insert(0, r"C:\ODK\Installs\1\output\Windows\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rx
CMD = r"""
import unreal, json
s = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
st = json.loads(s.get_status_json())
st.pop("jobs", None)
print(json.dumps({k: st[k] for k in st if k not in ("log",)})[:1500])
print("PAIR", s.get_pair_state(), s.is_signed_in(), s.get_catalog_count())
print("SHOT", s.capture_panel_screenshot("F:/Estate/_scratch/CREATORS_HUB/screens_2026-11-04/odk_07_signed_in_live.png"))
"""
r = rx.RemoteExecution(); r.start(); t=time.time(); node=None
while time.time()-t<8 and not node:
    node = next((n for n in r.remote_nodes if n.get("project_name")=="MyProject"), None); time.sleep(0.3)
r.open_command_connection(node["node_id"])
res = r.run_command(CMD, exec_mode=rx.MODE_EXEC_FILE)
for o in res["output"]: print(o["output"])
print(res["success"], res["result"])
r.close_command_connection(); r.stop()
