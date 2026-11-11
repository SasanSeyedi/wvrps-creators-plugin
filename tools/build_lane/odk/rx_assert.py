import unreal, json
s = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
st = json.loads(s.get_status_json())
print("quota_used", st.get("quota_used_bytes"), "remaining", st.get("quota_remaining_bytes"))
for j in st.get("jobs", []):
    print("JOB", j["id"], j["state"], j["message"], [(p["role"], p.get("api_item"), p.get("bound_skeleton")) for p in j["parts"]])
ar = unreal.AssetRegistryHelpers.get_asset_registry()
from collections import Counter
for root in ["/Game/WVRPS/ship_room/room_Node8_glb", "/Game/WVRPS/vault_nft/vault_186", "/Game/WVRPS/vault_nft/vault_6796", "/Game/WVRPS/body/body_dragoon", "/Game/WVRPS/house_gear/gear_cap_op_black_dragoon"]:
    c = Counter(str(a.asset_class_path.asset_name) for a in ar.get_assets_by_path(root, recursive=True))
    print("ASSERT", root, dict(c))
skels = set()
for root in ["/Game/WVRPS/body/body_dragoon", "/Game/WVRPS/house_gear/gear_cap_op_black_dragoon"]:
    for a in ar.get_assets_by_path(root, recursive=True):
        if str(a.asset_class_path.asset_name) == "SkeletalMesh":
            m = unreal.load_asset(str(a.package_name))
            skels.add(m.get_editor_property("skeleton").get_path_name())
print("SAME_SKELETON", len(skels) == 1, sorted(skels))
s.open_panel(2)
print("SHOT", s.capture_panel_screenshot("F:/Estate/_scratch/CREATORS_HUB/screens_2026-11-04/odk_08_import_done.png"))
