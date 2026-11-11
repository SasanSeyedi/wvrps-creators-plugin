import unreal, json
s = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
st = json.loads(s.get_status_json())
print("BEFORE version", st["version"], "signed_in", st["signed_in"], "tier", st["who_tier"], "quota_used", st["quota_used_bytes"], "catalog", st["catalog_count"], "thumbs", st.get("thumbs"), "busy", s.is_busy(), "jobs", len(st["jobs"]))
for i in ["room:Node8.glb", "vault:186", "gear:cap-op-black:dragoon"]:
    print("QUEUE", i, s.import_item(i, "/Game/WVRPS"))
