# Optional Vein Mining activation modes

Original mod: XHL's Vein Mining 1.0.33, https://www.nexusmods.com/enshrouded/mods/129.
The input helper and its source are by Aerox912 and licensed under the repository's MIT license. It does not replace or relicense the original mod.

The Mod Manager exposes Hold, Toggle and Always on in Settings > Hotkeys. Hold remains the default and uses the original Windows import. Toggle suggests F8, which the player can change. Always on requires no activation key. Both optional modes still require the normal mining action on the controller or keyboard.

Hold keeps the original target-ore bar. Toggle and Always on hide it. Toggle shows a click-through `Vein Mining: On` or `Vein Mining: Off` notice for 1.5 seconds after a fresh key press. Always on shows no notice. Losing focus dismisses the notice.

Only the pinned Vein Mining DLL's USER32 import name changes to vmkeys.dll. The helper handles GetAsyncKeyState and four window lifecycle/display imports; other calls are forwards. Window handling is limited to the verified `XHLVeinMiningPreviewOverlay` class. The notice uses the original overlay thread's message pump and is destroyed with that overlay. It cannot take focus or intercept clicks. No game-wide keyboard hook or synthetic input is used. The three existing key reads share one serialized toggle state so mining and network requests agree. Losing focus pauses output and retains the toggle state; returning with the key already held does not toggle. A new game process starts with toggle off.

Files: vmkeys.dll belongs beside enshrouded.exe. Personal activation mode is in mods/XHL-Vein-Mining/controls.ini, [VeinMining], Mode=hold, toggle or always. Change it while the game is closed. The manager also saves the preference for reinstalls and profile switches. Clients choose independently; no server update is needed.

Build with CMake and MSVC for Windows x64, then run CTest. Tests cover hold passthrough, debouncing across repeated readers, background/focus transitions, notice expiry and the real DLL exports in all three modes. Window fixtures verify preview suppression, unchanged unrelated windows, click-through behavior and cleanup. In-game notice appearance and controller mining require a separate gameplay check.

To remove: switch to Hold in the manager before removing vmkeys.dll, or restore the verified original/localized Vein Mining DLL. Never remove the helper while a DLL still imports it.
