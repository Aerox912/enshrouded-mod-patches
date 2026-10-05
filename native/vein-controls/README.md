# Optional Vein Mining activation modes

Original mod: XHL's Vein Mining 1.0.33, https://www.nexusmods.com/enshrouded/mods/129.
The input helper and its source are by Aerox912 and licensed under the repository's MIT license. It does not replace or relicense the original mod.

The Mod Manager exposes Hold, Toggle and Always on in Settings > Hotkeys. Hold remains the default and uses the original Windows import. Toggle suggests F8, which the player can change. Always on requires no activation key. Both optional modes still require the normal mining action on the controller or keyboard.

Only the pinned Vein Mining DLL's USER32 import name changes to vmkeys.dll. The helper exports exactly the original module's USER32 imports and forwards everything except GetAsyncKeyState to Windows. It does not hook the game's keyboard API or generate keyboard events. The three existing key reads share one serialized toggle state so mining, network requests and the preview agree. Losing focus pauses output and retains the toggle state; returning with the key already held does not toggle. A new game process starts with toggle off.

Files: vmkeys.dll belongs beside enshrouded.exe. Personal activation mode is in mods/XHL-Vein-Mining/controls.ini, [VeinMining], Mode=hold, toggle or always. Change it while the game is closed. The manager also saves the preference for reinstalls and profile switches. Clients choose independently; no server update is needed.

Build with CMake and MSVC for Windows x64, then run CTest. Tests cover hold passthrough, debouncing across repeated readers, background/focus transitions, both optional modes and the real DLL exports. Actual controller mining remains a separate in-game check.

To remove: switch to Hold in the manager before removing vmkeys.dll, or restore the verified original/localized Vein Mining DLL. Never remove the helper while a DLL still imports it.
