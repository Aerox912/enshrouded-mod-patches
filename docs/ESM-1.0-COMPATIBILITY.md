# ESM 1.0 mod compatibility candidate

Four patch-only migrations target client build 25936191, data revision 1265863.
Original sources remain private. Obtain originals from the links in
[CREDITS.md](../CREDITS.md); retain the authors' licenses and notices. These
recipes require the exact already-maintained inputs recorded below, including
the existing Hoarder-compatible Rested patch. They do not accept arbitrary Nexus
downloads or silently bypass a checksum mismatch.

| ESM display name | Original / preserved folder | Recipe |
| --- | --- | --- |
| ESM Recipe Helper | Dacrenzio's Hoarder's Helper / `HoardersHelper` | `hoarder-1.0` |
| ESM Blink | Bem's Electric Blink / `ElectricBlink` | `blink-1.0` |
| ESM Creative | Creative Gem Forges / `creative-gem-forges` | `creative-forges-1.0` |
| ESM Rested | XHL's Rested Enhanced / `XHL-Rested-Enhanced` | `rested-1.0` |

Use the current Python patcher with the current recipes; an older packaged
PatchTool executable may not embed these recipes:

```powershell
python patcher.py patch hoarder-1.0 E:\Scratch\original-mod.lua E:\Scratch\fixed-mod.lua
```

Use each recipe on its corresponding `src/mod.lua`. Input and output must be
different files; existing output is refused. Inspect a disposable complete game
copy with the [ESM-maintained EML candidate](https://github.com/Aerox912/kfc-parser/tree/fix/enshrouded-1.0)
before any separately approved installation.

## Migration contracts

- Hoarder changes localization references to `data.hash`, preserving category,
  bundle and sorting metadata. Its existing same-run content helper remains the
  shared source for subsequent Rested localization writes.
- Blink builds the 1.0 `streamedData` wrapper from vanilla Blink defaults, retaining
  the authored mono PCM (115,198 bytes, 57,599 frames). Sound container defaults
  come from the current baseline. Failed content creation raises an error.
- Creative requires `InteractionOfferRuntime` and its client companion, applies
  observed native defaults and retains all six template/idempotency guards. Forge
  capacities remain 12/18/22/32/40/50 with their original essence mappings.
- Rested accepts only the observed vanilla 1.0 relocation: 23 exact variable
  identities, layout/config/type/size/data checks, an explicit bijection to the
  authored prefix, and unchanged native call IDs. The enhanced authored payload
  remains intact. The buff now lives inside `BuffTypeCollectionResource`
  `02022e86-8efe-40cb-8f6f-d22071cc3be3`, entry ID `3451821184`. Only its
  ineffective UI values are cleared; identity, impact link, map and apply mode
  remain intact. Localization writes preserve reference metadata.

## Evidence and limits

33 Python tests pass with private originals supplied, including wrong-input,
mutation and read/write contract checks. The Lua guard passes 15 observed-fixture
accept/reject cases; the buff contract covers negative inputs, coexistence and
idempotence. The private authored program checker verifies 69 nonoverlapping
layout entries, 11 exact apply/revert pairs, distinct effect storage and a maximum
of seven modeled operand entries. It does not emulate the native engine.

Six reflected node signatures match the earlier executable; all 69 layout types
match, and all 12 non-null attribute references resolve, including nested
`AttributeContainerResource` registrations. The copied full set produces 4,912
patches; serialized readback decodes every changed resource and validates seven
new content hashes. Blink/forge results match their isolated run. Rested's code,
shutdown, data and layout equal the authored payload exactly. English localization
retains Hoarder's tags and changes only Rested's two intended tags.
An injected conflicting Rested edit is rejected before publication, preserving
the prior directory, backup and referenced data. Failed runs can leave unused
staging bytes; those are not part of the published dataset.

```powershell
$env:ESM_ORIGINAL_MODS = 'E:\Scratch\original-mods'
$env:ESM_RESTED_PROGRAM = 'E:\Scratch\private-authored-rested.json'
python -m unittest discover -v
python tools/verify_rested_program.py $env:ESM_RESTED_PROGRAM
```

Private-fixture tests skip explicitly if those files are unavailable. Recipe
generation is reproducible with `tools/build_1_0_recipes.py`; it checks source
hashes and exact transformation counts before creating minimal deltas. Never
commit the originals, raw exported program, localization or audio data.

Native startup, Blink audio playback, forge placement/crafting, Rested effect
application/expiry/reapplication, saves and multiplayer remain unverified. No
dedicated-server 1.0 acceptance is claimed. Successful Lua execution of the rest
of the installed set does not establish that all its features still work.
This patch work does not alter the independent ESM module ownership or create a
second Stack Size implementation.

## Exact script hashes

The authoritative input/output SHA-256 pairs are the four named entries in
[`patches/recipes.json`](../patches/recipes.json). The generated table below is
also included in the candidate package manifest.

hoarder-1.0

- Input: `2b5f3b087508a484dd907de457446903873171106bd5ac06061e04a282dd6253`
- Output: `a2f833538c8f1bd6671762afcc3140626a2bca020f430f961a06f33f1dc56221`

blink-1.0

- Input: `e91de259f8fbfa616f7ee63f06556b4ba96738a2249d7791e9a80c2660de6a94`
- Output: `ad08c4dda001c96b19a3c1e94c73d92c5bac9f37185fbf4b27e67e71cf41f4be`

creative-forges-1.0

- Input: `01e083bc3b07acec714df0185424a65285b13897610908d9de725e1a7bcde4b0`
- Output: `2ebf16ff71b2f0eb615d15e3411dbaf06eadaa35b48d07abc6c4ae39095e0204`

rested-1.0

- Input: `3123d2d0bb9c38a39a30adc1b9979306c009576cce1a04654cf38be31619e29e`
- Output: `7b23cdfae1b2f98bd74c74cfeaeaed6ce6035b808eda0f3f519e3df912e1ff4e`
