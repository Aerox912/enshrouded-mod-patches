# Enshrouded mod patches and profiles

Maintained compatibility patches and the complete Normal/Cheeze component catalog. Both profiles enable EMBER magic furniture, magic production stations, buff refresh, no-build-zone removal and building in Shroud fog. Altar requirements remain enabled.

Download originals from the author links in catalog.json. The Windows patch utility validates the original hash before patching and never edits its input. See CREDITS.md and LICENSE. Generated patched originals are for your local installation; this release does not redistribute them.

`PatchTool.exe patch <recipe> <input> <output>` applies one patch. Preserve the original input for rollback. Server packages provide profile assembly commands; the private launcher handles client imports. Global XP Share is server-only and remains at its original 1x setting.

The catalog records verified file hashes and game compatibility. Updating a dependency requires new hashes and revalidation; do not substitute an arbitrary newer original.

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
