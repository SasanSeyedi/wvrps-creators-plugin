# Install WVRPS Creators (Unreal Engine editor plugin, 0.1.3 beta)

WVRPS Creators puts the free WVRPS creators pack in your Unreal editor. You pair the editor once with a free
creator key from wvrps.com, then browse the pack and import assets into your project.
- Ship-room props, fitted gear and vault characters import as meshes through Interchange.
- Songs and the Environments sound library import as SoundWaves (optionally SoundCue + Blueprint actor).

## Licence
The plugin source code is MIT (see `LICENSE`). The WVRPS assets themselves are licensed separately:
you may use them in your own games and projects, including commercially; you may not resell the assets or
transfer any commercial rights in them to anyone else.

## Supported engines
Windows 64-bit, editor only.

| Engine | Build | Notes |
|---|---|---|
| Unreal Engine **5.5** (Epic Games launcher) | prebuilt zip + source | Built clean with `BuildPlugin`. |
| Unreal Engine **5.4** (tested on 5.4.4) | prebuilt zip + source | Built with the LightForge-disable workaround (see below). |
| **Otherside ODK editor** (UE 5.5.4, BuildId 299273) | binary-only install into MyProject | Built against the UE 5.5 donor toolchain; ODK symbol-check passes (0 missing). |

Other versions, macOS, Linux: not built, not tested.

## Install
1. Close the Unreal editor.
2. Unzip the zip for your engine into your project's `Plugins` folder. You should end up with
   `<YourProject>/Plugins/WVRPSCreators/WVRPSCreators.uplugin`. Create the `Plugins` folder if your project does not have one.
   - Blueprint-only projects can use the plugin as is, because the zip contains the compiled `Binaries/Win64`.
   - C++ projects can rebuild from `Source/`.
3. Open the project. Go to **Edit › Plugins**, search for "WVRPS Creators", tick **Enabled**, then restart the editor.
   The plugin also turns on Epic's built-in **Interchange** plugin, which it needs.
4. Open the panel from **Tools › Misc › WVRPS Creators** (the exact label depends on the engine version),
   or type `WVRPS.Creators.Open` in the console.

## Pair the editor (once per editor and project)
1. On the **Connect** tab, press **Pair with wvrps.com**. The panel shows an 8-digit code, and your browser opens
   `https://wvrps.com/creators?code=<code>`.
2. On wvrps.com, sign in with your email or wallet and approve. Holders and non-holders both get a free key.
3. The panel picks up the key within a few seconds and shows **CONNECTED · PUBLIC** (or HOLDER).
   - The key is stored in the editor's per-project user settings (`Saved/Config/…/EditorPerProjectUserSettings.ini`),
     never in Content or project files. It lasts 90 days.
   - **Sign out** revokes the key on the server and removes it here. Revoke keys on wvrps.com under Signed-in devices.
     One account can have 5 live keys.

## Browse and import
- **Assets** mode: the whole 3D/audio pack. Use the class filter and search, press **IMPORT** on a tile.
  Assets land in `/Game/WVRPS/<class>/<item>/`; change the root on the Import tab.
- **Environments** mode: the Otherside sound library, drilled down 29 biomes → Music / SFX → category → clips.
  Each clip has **PLAY** (preview; charges the same quota as an import) and **IMPORT**. The "With Blueprint"
  toggle makes the import also create a SoundCue + a placeable Blueprint actor (off = SoundWave only).

| What | Result in Unreal |
|---|---|
| Ship-room props, chess, printables (GLB) | Static or skeletal mesh, materials + textures |
| Fitted gear ("GEAR on DRAGOON / GNAR / NAYOMI") | Matching body imported first (or reused), gear bound to the same USkeleton |
| Vault NFTs (voxel GLB) | Skeletal mesh + skeleton + carried animations |
| WVRPS characters (MML) | Body first, then every gear GLB bound to the body's skeleton |
| Music / environments (wav, mp3) | SoundWave (or SoundWave + SoundCue + Blueprint actor with the toggle) |
| Brick kits (`.json`) | Listed for reference only — not importable |

Before importing, the plugin checks each file's header (`glTF` for GLB, RIFF/WAVE for wav, ID3/MPEG for mp3);
a missing file that comes back as a web page is reported, not imported.

## Fair-use quota
- Each key can download **200 MB per day**, resetting at 00:00 UTC.
- Downloads are charged at the catalogue size; when there is no size, a fixed estimate is charged
  (body 20 MB, fitted gear 10 MB, vault/voxel 4 MB, anything else 10 MB).
- When the quota runs out, the import fails with a message saying how much is left and when it resets.

## Known limits (0.1.3)
- Windows 64-bit editor only. No runtime module, no cooking rules.
- Thumbnails decode PNG and JPEG. Most pack thumbnails are webp, so most tiles show a placeholder icon.
- Imports run one at a time. A gear item with its body downloads two files (~15 MB, 14–44 s in tests).
- Audio import runs on the editor thread (a 5 MB wav ~0.3 s); mesh import runs in the background via Interchange.
- Animation retargeting, LODs and Nanite are left at Interchange defaults.
- UE 5.4 with the third-party **LightForge** plugin in the engine: `BuildPlugin` crashes before compiling
  (`ArgumentNullException`). Rebuild with `-DisablePlugin=LightForge` (see `build54_workaround.cmd` in the build kit).
  The prebuilt 5.4 zip is unaffected.

## Version history
- **0.1.3** (2026-11-07): Environments sound-library browse mode (29 biomes → Music/SFX → categories → clips,
  with Play preview + import-with-blueprint); ODK editor build; category/group icons; taxonomy aligned to the
  Otherside audio library. Preview charges quota; assets stay gated behind the creator API.
- **0.1.2** (2026-11-04): every download charged to the key quota; thumbnails; selection bar + per-tile Import;
  Pair swaps to Sign out while signed in.
- **0.1.1** (2026-11-04): initial creator pack browser (3D/MML/audio import, pairing, 200 MB/day quota).

## Editor Python
`unreal.get_editor_subsystem(unreal.WVRPSCreatorsSubsystem)` runs the same flow as the panel:
`start_pairing`, `refresh_catalog`, `import_item(id, "/Game/WVRPS")`, `preview_audio(id)`, `stop_preview`,
`get_status_json`, `sign_out`.

Support: https://wvrps.com/creators
