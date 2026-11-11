import unreal, json, os
s = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
st = json.loads(s.get_status_json())
print("busy", s.is_busy(), "quota_used", st["quota_used_bytes"], "remaining", st["quota_remaining_bytes"], "thumbs", st.get("thumbs"))
tot = 0
for j in st["jobs"]:
    parts = []
    for p in j["parts"]:
        b = os.path.getsize(p["cache_file"]) if p["cache_file"] and os.path.isfile(p["cache_file"]) else None
        tot += b or 0
        parts.append((p["role"], p.get("api_route"), p.get("api_item"), b))
    print("JOB", j["id"], j["state"], j["message"][:90], parts)
print("DISK_TOTAL", tot)
