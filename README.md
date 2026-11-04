# WVRPS Creators (Unreal Engine editor plugin)

Pair the editor with your free WVRPS creator key, browse the WVRPS creators pack and import assets through Interchange.

1. Enable the plugin (Edit > Plugins > WVRPS Creators), restart the editor.
2. Window > Developer Tools > Misc > **WVRPS Creators** (or console `WVRPS.Creators.Open`).
3. Connect: press **Pair with wvrps.com**. Your browser opens `https://wvrps.com/creators?code=<code>`; sign in and approve.
4. Browse: filter by class, search, press **Import**. Assets land in `/Game/WVRPS/<class>/<id>/`.
   WVRPS characters (MML) import as the body first, then every gear GLB bound to the body's skeleton.

Licence: You may use these WVRPS assets in your own games and projects, including commercially. You may not resell the
assets, or transfer any commercial rights in them to anyone else. Install steps, supported engines and limits: `INSTALL.md`.

Editor Python: `unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)` exposes the same flow
(`start_pairing`, `refresh_catalog`, `import_item`, `get_status_json`, `capture_panel_screenshot`).
