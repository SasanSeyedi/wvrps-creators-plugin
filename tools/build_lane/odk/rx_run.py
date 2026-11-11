"""Run a python snippet file inside the MyProject ODK editor over UE remote exec. usage: rx_run.py <snippet.py>"""
import sys, time
sys.path.insert(0, r"C:\ODK\Installs\1\output\Windows\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rx
code = open(sys.argv[1], encoding="utf-8").read()
r = rx.RemoteExecution(); r.start(); t = time.time(); node = None
while time.time() - t < 20 and not node:
    node = next((n for n in r.remote_nodes if n.get("project_name") == "MyProject"), None); time.sleep(0.3)
if not node:
    print("NO NODE"); r.stop(); sys.exit(2)
r.open_command_connection(node["node_id"])
res = r.run_command(code, exec_mode=rx.MODE_EXEC_FILE)
for o in res["output"]:
    print(o["output"].rstrip())
print("SUCCESS", res["success"], res["result"])
r.close_command_connection(); r.stop()
