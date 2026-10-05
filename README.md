# Enshrouded mod patches and profiles

Maintained compatibility patches and the complete Normal/Cheeze component catalog. Both profiles enable EMBER magic furniture, magic production stations, buff refresh, no-build-zone removal and building in Shroud fog. Altar requirements remain enabled.

Download originals from the author links in catalog.json. The Windows patch utility validates the original hash before patching and never edits its input. See CREDITS.md and LICENSE. Generated patched originals are for your local installation; this release does not redistribute them.

`PatchTool.exe patch <recipe> <input> <output>` applies one patch. Preserve the original input for rollback. Server packages provide profile assembly commands; the private launcher handles client imports. Global XP Share is server-only and remains at its original 1x setting.

The catalog records verified file hashes and game compatibility. Updating a dependency requires new hashes and revalidation; do not substitute an arbitrary newer original.

## Fell Critter pickups with Auto Loot

The `auto-loot-critters` patch for XHL Auto Loot 1.5.0 preserves normal manual
pickup of Fell Critter remains, including the explosive variant. Upstream
classifies their `LootPickup_Material_Critter_Parts` templates as material piles
and removes their interaction prompts. The patch leaves those templates intact;
other supported drops still use automatic pickup.

Client imports and server package assembly both apply this verified patch. The
native Auto Loot DLL and user settings are unchanged. Rebuild game data with EMM
from the clean baseline and all selected mods before testing; replacing the Lua
file alone does not update already-patched game data. Close the game or stop the
server before applying an update, preserve its backup, and test newly defeated
Fell Critters after restarting. Multiplayer installations need the matching
client and server patch.

To prepare the script separately without changing the original:

```text
PatchTool.exe patch auto-loot-critters original-mod.lua fixed-mod.lua
```

Offline checks against client revision 1076226 and server revision 1024233
preserve both complete critter templates. Across 10,745 templates on each target,
all other component lists match the upstream patch result, with 91 automatic
pickup templates retained. This verifies the data patch; in-game pickup still
requires acceptance testing.

## Vein Mining hold key

The mod manager uses `patches/vein-hotkey.json` to configure the pinned 1.0.33
client DLL, including the English preview version. The complete normalized
checksum and all three key-reading instructions must match before any write.
An independently customized DLL is rejected. Original files are not included.

The Windows utility can also prepare a separate output file:

```text
PatchTool.exe vein-key original.dll customized.dll --key 0x76
```

This example uses F7. No server change is needed. The client DLL must be replaced
only while the game is closed. Keep your original and existing English translation.
