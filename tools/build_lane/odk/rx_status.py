import unreal, json
s = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
st = json.loads(s.get_status_json())
print("busy", s.is_busy(), "quota_used", st.get("quota_used_bytes"), "last_error", st.get("last_error"))
for j in st.get("jobs", []):
    print("JOB", json.dumps(j)[:1500])
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for a in ar.get_assets_by_path("/Game/WVRPS", recursive=True):
    print("ASSET", a.package_name, a.asset_class_path.asset_name)
