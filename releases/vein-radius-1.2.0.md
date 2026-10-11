# Vein Mining radius 1.2.0 integration release

This component update prepares the matching client/server change for Soulrend
and the next mod-manager rebuild. It is an integration candidate: sustained
Soulrend performance, resource payout and reconnect acceptance are still open.
Local F12 switching and restoration of the saved radius were verified by the
player. The loader's repeated incompatible-DLL messages stopped in the combined
local session. No hosted deployment is part of this release.

## Behavior

F12 cycles 1x, 2x and 3x (5, 10 and 15 m). A new installation starts at 2x;
later launches remember `RadiusMultiplier` in
`mods/XHL-Vein-Mining/radius.ini`. Hold, Toggle and Always on remain supported.
Toggle and Always keep the hidden preview scanner inactive while ordinary
mining actions still activate mining. The original payout implementation is
retained. Larger active mining operations require the sustained test below.

Shroudtopia caches missing-entrypoint DLLs until their size or write time
changes, avoiding repeated probes and log messages. It retains transient-load
and null-factory retries. Historical log files are preserved.

## Required pair and manager integration

- Install the maintained client helper and loader from the release manifest.
- Prepare both XHL DLLs locally from the verified author's originals. The
  public release contains no original or patched XHL DLL, game data, worlds,
  account identifiers, passwords or personal settings.
- The server must contain the radius adapter from server-compat commit
  `e2b5c145b5ad1601698a012a98222cb93639cb64`, integrated into the current
  combined Creative/Journey server. The manifest identifies that combined
  binary; do not substitute the standalone server proxy or candidate 4's
  earlier combined build.
- Manager source commit `f7c0036e3d0acc8b71a29cb8d6ba46489b18dd5a`
  preserves the expanded capacity and helper import across Hold, Toggle,
  Always and key rebinding. The old manager can reject expanded files while
  restoring saved Toggle settings, so this source update is required before
  the new manager is built.
- Preserve `.EnshroudedClientMods/hotkeys.json`, `controls.ini` and `radius.ini`
  independently of installation receipts. Do not ship a default radius.ini
  over an existing choice. Update receipt/hash inventories to the actual
  generated DLL bytes after activation rebinding.
- F12 is reserved. Radius-enabled Hold also needs vmkeys.dll. Removal must
  restore the matching non-radius DLL/helper pair through the manager's
  rollback transaction.
- Only A6 requests and the pinned expanded server DLL are accepted. There is
  no legacy protocol fallback.

Use Python 3 with the extracted component package:

```text
python tools/prepare-vein-radius.py --client path/to/client/XHL-Vein-Mining.dll --server path/to/official/XHL-Vein-Mining.dll --output new-staging-directory --mode toggle --key 0x77
```

Use the player's saved activation mode/key. Omitting `--key` preserves the
client DLL's current key. Original-language and already localized client DLLs
are accepted by normalized full checksum. Existing output directories,
unsupported versions and altered DLLs are rejected. `prepared-files.json`
records both generated outputs. Copy the original mod's support files from
the user's verified author import when assembling a clean install; preserve
existing configuration when applying an update.

Soulrend's historical Normal package does not include the combined adapter.
Apply an explicit delta to a freshly verified Soulrend baseline. Preserve the
current world, settings, permissions, GlobalXPShare.original.dll/XP chain,
Workshop 20x and Wine `dbghelp=native,builtin`. A deployment owner must verify
the complete rollback backup, the server executable pin and zero players
immediately before an authorized graceful stop. Preparation does not stop or
start a server.

## Evidence and remaining test

The radius gate covers actual original/localized DLL callbacks, all three
activation modes with 100,000 idle samples each, saved-radius restoration in a
fresh process, bounded geometry, sequence/replay rejection and 8,000 concurrent
player requests. Manager rebind tests cover six mode/key combinations and
reject corruption and F12 conflicts. The combined installation owner also
verified 18 transaction checks, including preservation and rollback.

GPT-6 Pro reviewed the original loader/idle fix and the first radius candidate.
Two radius findings were reproduced and corrected locally. The final paired
candidate was verified locally; no independent Pro review of that final
candidate or live performance result is claimed. Publication preparation adds
the pinned build dependency and original-import staging tool without changing
the tested runtime algorithms.

For Soulrend, compare Toggle off/on and Always idle/moving/menu use for at least
30 minutes per mode, extending to the previous failure window when possible.
Record frame-time percentiles, client/server CPU and memory, newly appended log
bytes, errors and disconnects. Mine comparable isolated resource deposits at
1x, 2x and 3x and record inventory/drop deltas, consumed terrain and completion
time. Check ordinary mining with Toggle off, controller and keyboard input,
reconnect, and another player's isolation. Use separate acceptance rows for
performance, payout and reconnect. Do not equate a larger radius with an exact
resource multiplier or claim zero lag from a short session.

Combined testing belongs to **Fix Creative input capture and Journey**,
chat `01a12ad1-e3f2-7953-9d73-8ba2e9d9f271`. Return installed hashes, durations,
measurements, payout observations, failures and rollback/retention state to
the Vein source chat `01a12922-0311-7fc0-aa96-d7de532323b9` after testing.
