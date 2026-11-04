# Install WVRPS Creators (Unreal Engine editor plugin, 0.1.1 beta)

WVRPS Creators puts the free WVRPS creators pack in your Unreal editor. You pair the editor once with a free
creator key from wvrps.com, then you browse the pack and import assets into your project.
- Ship-room props, fitted gear and vault characters import as meshes through Interchange.
- Songs import as SoundWaves.

## Licence
You may use these WVRPS assets in your own games and projects, including commercially. You may not resell the assets,
or transfer any commercial rights in them to anyone else.

## Supported engines
Both builds are Windows 64-bit, editor only.

| Engine | Zip | Evidence (2026-11-04) |
|---|---|---|
| Unreal Engine **5.5** (Epic Games launcher) | `WVRPSCreators_UE55_20261104.zip` | Built with `BuildPlugin`: 0 warnings. Tested end-to-end in the editor against the real Creator API on a local test server: pair, catalogue (3,272 items), and imports of a prop, gear on its body, a vault character, a wav song and an mp3. 19/19 checks passed and the editor exited with code 0. |
| Unreal Engine **5.4** (tested on 5.4.4) | `WVRPSCreators_UE54_20261104.zip` | Compiled from the same source with 0 plugin warnings. Smoke test in the 5.4.4 editor (no rendering) against the same API: pair, catalogue (3,272 items), one prop import and one mp3 import. The test passed and the editor exited with code 0. The Slate panel has **not** been looked at on 5.4. |
| Other versions, macOS, Linux, the Otherside ODK editor | none | Not built and not tested. The ODK build is planned separately. |

## Install
1. Close the Unreal editor.
2. Unzip the zip for your engine into your project's `Plugins` folder. You should end up with
   `<YourProject>/Plugins/WVRPSCreators/WVRPSCreators.uplugin`. Create the `Plugins` folder if your project does not have one.
   - Blueprint-only projects can use the plugin as it is, because the zip contains the compiled `Binaries/Win64`.
   - C++ projects can also rebuild it from `Source/`.
3. Open the project. Go to **Edit › Plugins**, search for "WVRPS Creators", tick **Enabled**, then restart the editor.
   The plugin also turns on Epic's built-in **Interchange** plugin, which it needs.
4. Open the panel from **Tools › Misc › WVRPS Creators**. The exact menu label depends on the engine version.
   You can also type `WVRPS.Creators.Open` in the console.

## Pair the editor (once per editor and project)
1. On the **Connect** tab, press **Pair with wvrps.com**. The panel shows an 8-digit code, and your browser opens
   `https://wvrps.com/creators?code=<code>`.
2. On wvrps.com, sign in with your email or wallet, check that the key label reads like
   `Creator key · UE 5.5 - <YourProject>`, then approve. Holders and non-holders both get a free key.
3. The panel picks up the key within a few seconds and shows **CONNECTED · PUBLIC** (or HOLDER).
   - The key is stored in the editor's per-project user settings, under `Saved/Config/…/EditorPerProjectUserSettings.ini`.
     It is never stored in your Content folder or in your project files.
   - The key lasts 90 days.
   - **Sign out** revokes the key on the server and removes it from the editor. You can also revoke keys on
     wvrps.com under Signed-in devices. One account can have 5 live keys.

## Browse and import
- **Browse** lists the whole pack, about 3,300 items. Use the class filter and the search box to narrow it.
  Press **IMPORT** on a tile.
- Assets land in `/Game/WVRPS/<class>/<item>/`. You can change the root on the **Import** tab.

| What | Result in Unreal |
|---|---|
| Ship-room props, chess, printables (GLB) | Static or skeletal mesh, with materials and textures |
| Fitted gear, labelled "GEAR on DRAGOON / GNAR / NAYOMI" | The matching base body is imported first, or reused if it is already in `/Game/WVRPS/body/`. The gear's skeletal mesh is then bound to the **same USkeleton** as the body. |
| Vault NFTs (voxel GLB) | Skeletal mesh + skeleton, with the animations the file carries. The tested token brought 157 AnimSequences. |
| WVRPS characters (MML) | The body first, then every gear GLB, all bound to the body's skeleton |
| Music (wav, mp3) | One SoundWave each, through Unreal's standard audio importer |
| Brick kits (`.json`) | Listed for reference only. They cannot be imported. |

Before it imports anything, the plugin checks every file's header (`glTF` for GLB, RIFF/WAVE for wav, ID3/MPEG for mp3).
If a missing file comes back as a web page, the plugin reports it instead of importing it.

## Fair-use quota
- Each key can download **200 MB per day**. The count resets at 00:00 UTC.
- Downloads are charged at the catalogue size. When the catalogue has no size for an item, a fixed estimate is charged:
  body 20 MB, fitted gear 10 MB, vault/voxel 4 MB, anything else 10 MB.
  - Example: in our test, a 5.5 MB body, 9.1 MB of gear and a 5.0 MB song together used 40 MB of quota.
- When the quota runs out, the import fails with a message like "Daily download quota (200 MB per key) reached: this asset
  needs 0.4 MB, 0.1 MB left today. Resets at … UTC". Smaller items can still fit.

## Known limits (0.1.1)
- Windows 64-bit editor only. No runtime module and no cooking rules. ODK cook checks are not included yet.
- Thumbnails decode PNG and JPG only. Most pack thumbnails are webp, so most tiles show a placeholder icon.
- Imports run one at a time. A gear item with its body downloads two files, about 15 MB, which took 14–44 s in our tests.
- The two kinds of import behave differently:
  - Audio import runs on the editor thread. A 5 MB wav took about 0.3 s.
  - Mesh import runs in the background, through Interchange.
- Body and gear files linked from an MML character are fetched straight from their public URLs. The quota is charged
  for the MML item only.
- Animation retargeting, LODs and Nanite settings are left at Interchange defaults.
- Some catalogue entries are not meshes. For example, `room:Signs.glb` contains no geometry, and the import reports
  "Interchange imported nothing".
- **UE 5.4 with the third-party LightForge plugin installed in the engine:** Epic's `BuildPlugin` crashes before
  compiling (`ArgumentNullException … ModuleRules.IsValidForTarget`). This only matters if you rebuild from source.
  The prebuilt 5.4 zip is not affected. To rebuild, add `-DisablePlugin=LightForge` to the UnrealBuildTool command
  line (see `build54_workaround.cmd` in the build kit).

## Editor Python
`unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)` runs the same flow as the panel:
- `start_pairing`
- `refresh_catalog`
- `import_item(id, "/Game/WVRPS")`
- `get_status_json`
- `sign_out`

Support: https://wvrps.com/creators
