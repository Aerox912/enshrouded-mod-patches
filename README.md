# Enshrouded mod patches and profiles

Maintained compatibility patches and the complete Normal/Cheeze component catalog. Both profiles enable EMBER magic furniture, magic production stations, buff refresh, no-build-zone removal and building in Shroud fog. Altar requirements remain enabled.

Download originals from the author links in catalog.json. The Windows patch utility validates the original hash before patching and never edits its input. See CREDITS.md and LICENSE. Generated patched originals are for your local installation; this release does not redistribute them.

`PatchTool.exe patch <recipe> <input> <output>` applies one patch. Preserve the original input for rollback. Server packages provide profile assembly commands; the private launcher handles client imports. Global XP Share is server-only and remains at its original 1x setting.

The catalog records verified file hashes and game compatibility. Updating a dependency requires new hashes and revalidation; do not substitute an arbitrary newer original.

## Manual corpse pickups with Auto Loot

The `auto-loot-critters` patch for XHL Auto Loot 1.5.0 preserves manual pickup
of ordinary and explosive Fell Critter remains and two skeleton bone piles:
`LootPickup_Material_Skeleton_Weakling_Bones`
(`346db2aa-7375-44cf-a0a5-2c27f09b694c`) and
`LootPickup_Material_Skeleton_Hound_Bones`
(`e7ae4fac-17f0-4425-9924-b9a25bf3baf6`). Auto Loot classifies these as
material piles and removes their `InteractionOffer` and `SpawnTime` components.
The patch skips these four verified templates before Auto Loot modifies them,
leaving other supported automatic drops enabled.

Client imports and server package assembly both apply this verified patch. The
native Auto Loot DLL and user settings are unchanged. Rebuild game data with EMM
from the clean baseline and all selected mods before testing; replacing the Lua
file alone does not update already-patched game data. Close the game or stop the
server before applying an update, preserve the complete pre-update backup, and
test newly defeated Fell Critters and both skeleton types after restarting.
Multiplayer installations need matching client and server patches.

To prepare the script separately without changing the original:

```text
PatchTool.exe patch auto-loot-critters original-mod.lua fixed-mod.lua
```

Pinned EML 0.1.2 offline probes against client revision 1076226 and server
revision 1024233 each enumerated all 10,745 templates. Both skeleton bone rows
and both critter rows have `true` in the TSV's fifth `unchanged` column. For
those four targets, the probe captures `r.data` before Auto Loot runs by
recursively serializing its Lua-visible table keys and values, sorting entries,
and representing leaf values as their type plus `tostring` output. It compares
that representation with the post-run `r.data`; `true` means the representations
matched. This probe-level comparison covers values as well as component types,
including the bone rows' `InteractionOffer` and `SpawnTime`. It is not a binary
serialization of the underlying game resource. All other component lists
matched the previous critter-only output, with only the two bone rows returning
to their original component lists. The client and server probe logs report 89
Auto Loot interaction and spawn-delay removals, down from 91 previously.

The all-selected server rebuild completed from a clean baseline. The first
all-selected client attempts stopped in `HoardersHelper` while reading a
localization resource (`read_data():read_resource`, Windows error 2) because the
isolated clean stage lacked the installed client's original `enshrouded_000.dat`
through `enshrouded_031.dat` archives. After copying those 32 archives into the
isolated stage from the installed client as a read-only reference, the pinned
EMM runner completed the client rebuild with all selected mods, including
HoardersHelper and Auto Loot. The staged client and server data has not been
installed. Live pickup and multiplayer acceptance remain required.

Hollow coverage is limited to the two confirmed material templates above. The
mapping review found 18 `Enemy_Skeleton_*_BonePile` templates with
`RandomLoot`/`RandomLootContainer` and separate
`LootContainer_Enemy_Skeleton_*` templates with inventory and interaction
components. Those resource families do not match Auto Loot's
`LootPickup_Material_` classifier. The local exports do not establish that
every Hollow corpse resolves to the two fixed templates; the weakling/melee
mapping to `LootContainer_Enemy_Skeleton_Melee_Dungeon` comes from community
extracted data, not local values. If the report includes these corpse
containers, verify that path in live gameplay. Giant Bones and reanimator
bone-pile structures are separate cases.

To roll back, restore the backed-up Auto Loot `src/mod.lua` on both client and
server, then regenerate each target from its clean baseline with all selected
mods. Restore the complete pre-update server backup if regeneration or startup
fails. Do not restore vanilla game data by itself, since that removes other
selected mods.

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
