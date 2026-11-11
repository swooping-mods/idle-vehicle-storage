# Idle Vehicle Storage: behavior and implementation

Describes the exported implementation **0.1.0**. The linked source and metadata define this version; gameplay qualification is a separate requirement.

## Behavior

The host scans up to 256 owned records every two seconds and recalls continuously idle vehicles through native RecallToGarage. A live car needs no cargo/dust, driver, cleaning/autopilot or significant motion; parked records need a stable empty in-world holder. Exact passenger-seat reads must succeed and show no occupants. Default qualifying time is five minutes, configurable from 30 to 86,400 seconds.

## State and lifecycle

Per-record idle timers are temporary and reset when qualifying conditions or world identity change. Recall and RequestSave use native ownership/storage rules; the mod does not bypass refusal for another buyer’s car. Rooted snapshots are released after use. Unknown seat counts or binding/read failures prevent recall.

## Integration and configuration

No additional feature-mod runtime dependency is declared. Included shared headers are source reuse, not extra DLL dependencies.

The [module metadata](../source/mods/idle-vehicle-storage/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

Packaged defaults in [settings.ini](../source/mods/idle-vehicle-storage/settings.ini); supported bounds and interpretation remain in the linked parser.

| Section | Setting | Default |
|---|---|---|
| `[idle-storage]` | `timeout_seconds` | `300` |

## Implementation sources

- [mod.c](../source/mods/idle-vehicle-storage/mod.c)
- [base.h](../source/mods/idle-vehicle-storage/base.h)

## Verification boundary

Passenger detection and live/parked transitions are fail-closed but still need game qualification. No forced guest-owned recall, off-world timer persistence or guaranteed recovery of unsupported vehicles.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
