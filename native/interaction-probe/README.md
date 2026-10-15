# Interaction diagnostics

Development build for two planned independent mods: **Auto Harvest** and
**Auto Corpse Loot**. Both use a **2 metre** activation radius. Neither depends
on XHL Auto Loot. This diagnostic DLL does **not** implement either feature.

Auto Harvest will start off each session, with a configurable F9 toggle. Mature
crops, harvestable wild plants and harvestable dried corn are in scope;
seedlings, decorations and trees are excluded.

Automatic looting has an independent enabled setting. Whenever it is enabled,
it also collects fireflies and moths and recovers the player's own tombstone,
using the same 2 metre range and gameplay, focus and inventory-capacity rules
as corpse looting. These targets do not depend on F9 or a separate tombstone
toggle. Corpse loot is limited to the player's credited kills; other players'
kills and tombstones stay manual. Unknown kill credit or tombstone ownership
also stays manual. Items that do not fit must remain available.

## Purpose and limits

The diagnostic observes client interaction prompts, manual input and inventory
transfer fields, interaction attempts, combat events and loot transformations on their existing callback
threads. It copies a bounded number of observations and writes them to the
Shroudtopia log on the loader's update thread. It never sends an interaction,
changes inventory, destroys a plant, or sets kill ownership.

Version 0.2 replaces two server-side observation points that were never called
in the first dedicated-server client test. Nearby client offers are sampled at
most ten times per second, with bounded iteration and output. Input changes
are copied only for the local player. Both observers pause while unfocused.
`clientOffer.lastAcceptionId` is an interaction field, not kill credit.
`clientInput.hudHost` is an observed UI entity field whose precise semantics
remain unverified. Input snapshots show requests, not successful transfers.

Version 0.3 also observes `actor_network_to_client` for the local player. It
records the full 64-bit `interactionHostId` and its object type, action sequence,
trigger time and trigger type. A world-object target must not be truncated or
treated as an entity ID. Only changed interaction targets/actions and target
clearing are logged. The observer requires a recent local-player observation
and pauses when unfocused. It does not establish kill credit or ownership.

Client data revision 1076226 is the only supported executable. The on-disk SHA256,
PE identity, named system descriptors and callback prefixes must match before
hooks can be prepared. Unsupported layouts leave native behavior available.
An unexpected memory layout disables observations. Native callbacks still run.
No game executables, assets, worlds or personal settings are included.

Version 0.4 adds a post-callback observation of `ActorInput` from
`player_control_action_sequence`. It captures the selected interaction offer ID
as well as the complete object ID. This helps distinguish target selection from
the replicated animation already captured by version 0.3. It does not establish
plant maturity, item receipt, kill credit or tombstone ownership. The fourth
dedicated-server session captured this observation point; it missed both crop
actions, which were visible in the replicated-action observer.

The DLL stays loaded until game exit to protect callbacks already in flight
during deactivation. Deactivation removes the entry hooks; restart before
replacing or removing the DLL. Hook trampolines are retained until process exit.

## Build and test

Windows x64, Visual Studio 2022 C++ build tools and CMake:

```powershell
cmake -S native/interaction-probe -B build/interaction-probe -A x64
cmake --build build/interaction-probe --config Release
ctest --test-dir build/interaction-probe -C Release --output-on-failure
```

No installed game is needed to build or run the offline tests. These tests do
not establish safe harvesting or kill attribution in a running game.

## Local diagnostic session

Close the game. With Shroudtopia installed, put `interaction_probe.dll` and
`mod.json` in `mods/interaction_probe/`. Enable `mods.InteractionProbe.active` in
`shroudtopia.json`. The log should say `observing only`.

On a dedicated server, perform these actions **manually**:

1. Manually harvest one ripe corn plant, then one dried corn plant, leaving a
   few seconds between the actions.
2. Collect one firefly or moth, then leave the world after testing.

Four manual sessions were completed on 7 October. The latest session isolated
dried corn, ripe corn, a wolf corpse, the player's tombstone and fireflies in
that order. Version 0.4 recorded five distinct replicated target actions and
two inventory-transfer requests. Template identity, ownership and inventory
capacity handling remain unproven. See `FEASIBILITY.md` for the associations
and remaining gaps; another identical test is not currently needed.

Version 0.5 adds passive observation of the full offered action for the game's
selected Interaction target. It uses only read-only helpers already called by
the native prediction path, with verified executable/helper prefixes, bounded
sampling and component-size checks. Logs distinguish a missing component from
an unavailable offer. It does not run the prediction helper or automate input.
Its 38 event checks and 145 callback-boundary checks pass. Live selected-offer
results remain unverified.

Retain the log and note which action happened when. `hit` records identify hit
sources, not verified final kill ownership. A client that sees loot
transformations but no authoritative death/experience events may require server
support. Do not infer ownership from proximity or XP alone.

To remove the diagnostic, close the game, delete `mods/interaction_probe/` and
remove only its `InteractionProbe` configuration entry.

## Credits and source

Original diagnostic code: Aerox912, MIT (repository `LICENSE`).
Shroudtopia API: Miguel Oppermann / s0T7x, MIT, copied from
[`Aerox912/shroudtopia`](https://github.com/Aerox912/shroudtopia) commit
`f861395ea5390b9b16fa8bf3de797606f546ee40` (`include/shroudtopia.h`).
Original project: [s0t7x/shroudtopia](https://github.com/s0t7x/shroudtopia).
MinHook 1.3.4: Tsuda Kageyu and contributors,
[TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook/tree/v1.3.4).
Its license, including the bundled instruction decoder's notices, is preserved
in `vendor/minhook-1.3.4/LICENSE.txt`.

Enshrouded and its underlying game content belong to Keen Games.
