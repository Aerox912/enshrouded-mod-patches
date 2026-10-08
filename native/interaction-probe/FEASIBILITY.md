# Native interaction feasibility, 7 October 2026

Status: four dedicated-server observations analyzed. Neither standalone mod is
enabled for automatic actions or approved for release.

## Verified in client 1076226 executable

SHA256: `af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781`.
`tools/inspect-interactions.py` extracts the relevant reflection metadata and
named system callbacks from an executable supplied locally.

- `AcceptOfferedInteractionEvent` is 24 bytes. Guest, host and offer IDs are at
  offsets 8, 12 and 16. `interaction_attempt` consumes these events.
- `LootInteractionEvent` is 120 bytes, with the same IDs and `lootAll` and
  `forAllPlayers` flags at offsets 20 and 21. `immediate_loot_action` consumes it.
- `InteractionOffer` has `offerId` at 112, `verbId` at 116 and `isOffered` at 121.
- `ExperienceSource` contains `lastHitPlayerId` at offset 0. The
  `combat_experience_source` callback writes this from a resolved hit source and
  uses it when processing death events for XP. This is promising attribution
  evidence, not proof that a remote client receives it or retains it on loot.
- `client_transformed_to_loot` consumes an 80-byte event with entity ID at
  offset 8. Its reflected fields do not include a killer ID.
- Mature crops and seedlings can both have `Inventory`, `InteractionOffer` and
  `DestroyOnLoot`. Component presence alone cannot identify harvestable maturity.

## First dedicated-server observation

On 7 October the player reported manually looting wolf corpses and harvesting
corn and dried corn for plant fiber on the Enshrouded dedicated server.
The client log covered 12:22:07 to 12:30:18 UTC. It recorded eight
`TransformedToLootEvent` observations and 696 hit observations (including repeated
records). These do not establish eight wolf kills or 696 distinct attacks.

Final callback counts: `interaction_query_new=0`, `interaction_attempt=336`,
`immediate_loot_action=0`, `combat_experience_source=98712`,
`client_transformed_to_loot=32119`. No candidate, attempt, loot or death records
were emitted. The two never-called server-side functions cannot observe this
client's manual crop and corpse transactions. No diagnostic fault or dropped
rows was reported. The user's successful manual actions do not establish
automation feasibility or kill ownership.

Version 0.2 replaces those two entry hooks with verified named client systems:

- `ui_possible_interactions`, callback `0x2adf60`, descriptor `0x1d4ec90`,
  record size `0x48`: reads `ClientInteractionOffer`, `RenderTransform`,
  `LocalPlayerData` and the declared read-only transform lookup. Observes nearby
  targets, verb, offered state and last acceptance entity, without writing UI.
- `player_camera_control`, callback `0x2653f0`, descriptor `0x1d43430`,
  record size `0x158`: reads local `ClientPlayerInput` and UI data. Copies the
  32-byte inventory transfer fields, the digital input word at offset 792 and
  the UI entity field at `FbUiPlayData+0x16308`. The latter is read by
  `client_interaction_toggle_new`; its precise semantics remain unverified.
- `keen::VersionedData` is four bytes; its `version` field is at offset zero.
  A changed request version is logged even when source, destination and quantity
  match the previous request. This is not confirmation the server accepted it.

The camera callback always runs exactly once with its original context; the
observer never changes input, camera settings, ECS change counters, inventory
or transaction state.

## Second dedicated-server observation

The version 0.2 session ran on 7 October from 12:58:09 to 13:06:18 UTC. The player
reported killing and manually looting enemies, recovering their own tombstone,
harvesting corn and dried corn, and collecting fireflies.

Both replacement entry hooks ran. Eight changed inventory-transfer requests
were captured, all targeting local player 1, with type 2, flags 0 and amount 0.
Seven source entity IDs matched earlier loot-transformation observations. The
remaining source, 4925795, was not classified. Its possible association with
tombstone recovery is not proof of identity or ownership. Requests alone do not
prove server acceptance or establish what every transfer flag means.

The probe recorded ten loot transformations and 890 hit rows, including repeated
hit records. It observed only one nearby client offer, despite 3,377 scans and
21,470 scanned rows. No interaction-attempt or death records were emitted. No
fault, dropped rows or truncated scans were reported. The current offer path
does not identify the reported crop and insect transactions.

Static template evidence distinguishes collectible fireflies and moths from
decorative variants. The player-loot tombstone template is also distinct from
decorative graves. Template identity does not establish whose tombstone it is.
The reflected `NetworkActor.interactionHostId` contains a 16-byte `GameObjectId`
(64-bit value at offset 0, type at offset 8); its relationship to crop and insect
requests still needs verification.

## Accepted automatic-looting scope

Automatic looting includes the player's credited enemy kills, collectible
fireflies and moths, and recovery of the player's own tombstone. All use the
same enabled state, 2 metre range, gameplay/focus restrictions, duplicate-request
suppression and inventory-capacity handling. Tombstone recovery has no separate
toggle and does not depend on F9. Unknown ownership, other players' kills and
other players' tombstones remain manual. Auto Harvest's F9 toggle applies to
plants only.

## Version 0.3 action observer

The `actor_network_to_client` named system is at descriptor `0x1d0e420`, callback
`0x22a130`, with a `0x30`-byte iteration record. Its native callback reads
`NetworkActor` from record offset `0x10` and writes `ClientActor` through offset
`0x18`. The diagnostic reads only the former. The executable hash, descriptor
and 16-byte callback prefix are checked before enabling the hook.

The reflected `NetworkActor` stores its sequence at offsets 0, 8, 16 and 20;
the interaction host value/type at 56 and 64; and trigger/state at 204 and 206.
The native function copies that host into `ActionSequenceTriggerContext` before
starting the replicated action. The completed capture below confirms additional
targets missing from the UI-offer scan. Their template identities remain unknown.

All 115 offline checks pass (38 event/range checks and 77 callback-boundary
checks). They cover preservation of the original callback and data, local-player
filtering, full-width target IDs, repeated actions, target clearing, focus loss,
stale identity, missing components, bounded iteration and concurrent capture.
These tests do not demonstrate automatic collection or establish ownership.

The version 0.3 dedicated-server capture ran from 14:41:44 to process detach at
14:59:34 UTC on 7 October. The player confirmed collecting corn, dried corn,
fireflies and berries, and killing and manually looting enemies. No tombstone
recovery was reported for this session.

After deduplicating state changes by player, host, host type, sequence, trigger
time and call index, there are 41 nonzero-host actions across four sequences:

| Sequence | Actions | Observed association |
| --- | ---: | --- |
| 3481069144 | 10 | Each host matches a loot transformation and a transfer source |
| 428699201 | 9 | World-object interactions; individual templates not identified |
| 279711371 | 1 | World-object interaction; template not identified |
| 2227211401 | 21 | Dynamic and world-object targets, including repeated attempts |

Fifteen distinct nonzero inventory-transfer versions were recorded. Ten use
type 2, source equal to a transformed entity, and destination player 1. The
remaining five reference inventory slots with source entity zero: four type 4
and one type 2. Persistent request fields and changing HUD targets must not be
counted as new requests or used to relabel the original transfer.

There are 16 distinct loot transformations, eight nearby-offer rows and 489 hit
rows (including repeats). No attempt or death rows were emitted. No fault,
dropped rows or truncated scans were reported. Actor rows capture requests and
replicated actions, not item receipt, harvest maturity or ownership. The four
sequence IDs alone cannot safely classify corn, berries or insects. Existing
ground Auto Loot may also contribute actions; this capture is not an isolated
test of the planned automatic features.

Additional static leads, not ownership proof:

- `ClientActor` inherits `BaseActor.currentAction` at offset zero. Its reflected
  trigger context contains the host at offset 120 and type at 128. This gives
  a second observation source if replicated actions prove incomplete.
- `player_items_loot` obtains a second entity ID through `0x8d6150`, then uses
  declared `Actor`, `InventorySetup` and `SlotSelection` lookups for that ID.
  Its record also includes an `OwnerRelationship` write lookup. These describe
  the native loot-creation/transfer path; they do not yet establish persistent
  tombstone ownership on the client.
- `replicate_map_markers_for_ui` stores the current entity ID at marker offset
  zero and the ID returned by `0x8d6150` at offset four. The marker's text comes
  from `CustomPlayerString`, whose reflected field is a string ID. Do not treat
  the displayed name as authoritative ownership.

## Version 0.4 target-selection observer

Static tracing on the same executable identified `player_control_action_sequence`
at descriptor `0x1d3d2e0`, callback `0x284230`, with a `0x358`-byte record.
The callback uses record offset `0xa8` for `ActorInput`, clearing its previous
interaction host and offer at `0x2845cd` before calculating the next action.
`ActorInput.triggerContext` begins at offset 8; its full-width host value, host
type and offer ID therefore reside at offsets `0x80`, `0x88` and `0x90`.
Unlike the existing replicated-action observer, this observation includes the
offer ID. Reflection identifies object type 0 as Entity and 1 as SnappingBox.
These numeric IDs are not template identities or ownership evidence.

Version 0.4 observes this component immediately after the original callback,
using a private copy of its iterator. It never writes component data or requests
an action. It filters to a recently observed local player and logs only changed
targets/offers, repeated actions with changed sequence/time/counter, and clearing.
The scan and output queue remain bounded. No game-owned pointer is retained.

The build passes 38 event checks and 101 callback-boundary checks. The latter
execute the production observer with a synthetic original callback that changes
the offer ID, verifying the post-callback value is captured and the original
runs exactly once. Focus, stale identity, missing components, repeated targets,
large scans and concurrency are covered. A dedicated-server capture of this
new observation point was completed below. This remains a diagnostic, not Auto
Harvest or Auto Corpse Loot, and it does not establish successful transactions.

## Fourth dedicated-server observation

The 0.4 session ran from 16:55:00 to process detach at 17:04:54 UTC on 7 October.
The player reported this order: one dried corn, one ripe corn, killing and
looting a wolf, dying and recovering their own tombstone, then collecting
fireflies before logging out. These reports allow the following associations;
they do not independently prove the target's template or ownership.

| UTC | Reported action | Observed target | Sequence |
| --- | --- | --- | --- |
| 16:57:16 | Dried corn | `0x8fb4065a` | `428699201` |
| 16:57:21 | Ripe corn | `0x8fb40627` | `428699201` |
| 16:57:46 | Wolf loot | `5064395` | `3481069144` |
| 17:00:08 | Own tombstone | `5085868` | `2550083813` |
| 17:04:45 | Firefly pickup | `0x8fb40750` | `2227211401` |

All observed target types were Entity (0). Preserve their unsigned IDs; the
high-bit crop and firefly IDs must not be interpreted as SnappingBox IDs.
There were five distinct replicated target actions, one loot transformation
(the wolf target), and two distinct inventory-transfer requests, from the wolf
and tombstone targets to local player 1. Both transfers used type 2, flags 0,
amount 0. The same persisted request fields on later input rows are not new
transfers. No death, interaction-attempt or client-offer rows were emitted.
There were no reported faults, dropped rows or truncated scans.

The post-callback ActorInput observer captured the wolf, tombstone and firefly
actions, all with offer ID 0. It did not capture either crop action, although
the replicated-action observer did. This is incomplete coverage of the crop
request path. Distinct sequence IDs do not establish a safe automation API,
successful inventory capacity handling, kill credit or tombstone ownership.

## Version 0.5 selected-offer observer

Static tracing found that the game's prediction helper at `0x270760` reads the
full `InteractionOffer` through the system's declared lookup at record `+0x250`,
using `SelectedTargets` at `+0x148`. Reflection defines TargetType::Interaction
as 2. The existing HUD observer instead reads the smaller ClientInteractionOffer,
which lacks both the action reference and offer ID.

The new observer calls the same read-only target selector (`0x270fd0`), entity
lookup (`0x8c6bc0`) and component reader (`0x8d3020`). Their prefixes are checked
alongside the supported executable hash. It samples at most ten times per
second and logs changed target, predicted sequence, default action reference,
offer ID, verb and offered state. It requires a component stride of at least
128 bytes before reading InteractionOffer, skips missing components, never
truncates a wide object ID for entity lookup, and retains no native pointers.
It never invokes the prediction helper itself, which writes game state.

All 183 offline checks pass: 38 event checks and 145 callback-boundary checks.
The new checks cover unavailable offers, full-width IDs, missing or short
components, throttling, focus loss, and preservation of native data and calls.
This is a prepared diagnostic extension. Its live selected-offer results still
need verification. The fourth capture is sufficient for the current analysis;
do not request another identical sequence without checking this new observation.

## Remaining gates

1. Identify manual harvest and collectible-insect transactions on a dedicated
   server and their relationship to client interaction offers.
2. Verify the normal client request path, including server validation, inventory
   capacity, partial transfers and resource/progression updates.
3. Verify template identity and harvest-state lookup for the exact mature/wild/
   dried-corn targets. Confirm dried corn's normal plant-fiber yield.
4. Establish authoritative final kill credit and its lifetime through corpse
   transformation, and verify ownership of player tombstones. Add server support
   if the client lacks this information.
5. Implement independently installable DLLs and packages, manager settings and
   cloud release outputs after these bindings are established.
6. Validate the behavior in the approved plan, including two-player kill
   isolation, before release or signed-feed promotion.

The latest range requirement is **2 metres for both mods**, measured from player
to target. The diagnostic observes up to 3 metres to examine the boundary; that
observation distance does not change either mod's planned activation range.
