import unreal, json
s = unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)
print("signed_in", s.is_signed_in(), "catalog", s.get_catalog_count(), "pair", s.get_pair_state(), "busy", s.is_busy(), "cache", s.get_cache_dir())
for i in ["room:Node8.glb", "vault:186", "gear:cap-op-black:dragoon"]:
    print("QUEUE", i, s.import_item(i, "/Game/WVRPS"))
