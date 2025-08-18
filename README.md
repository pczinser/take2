# Defold Sim Kernel Starter (C++ Native Extension)

This is a minimal Defold project that runs a deterministic fixed-step simulation **inside a native C++ extension**.
It sets up:
- Floors with a grid of chunks (each chunk is W×H tiles).
- Observer-driven chunk activation (hot/warm rings).
- A portal table (from Tile3 -> Tile3), no gameplay yet.
- A tiny Lua API to control and inspect the sim.

## Files of interest
- `extensions/sim/src/sim_ext.cpp` – the native sim kernel with the fixed-step `Update` hook.
- `extensions/sim/ext.manifest` – compiler flags per platform.
- `main/main.script` – demo usage; prints sim stats and moves an observer on click/tap.
- `game.project` – points to `/main/main.collection`.

## Running
Open this folder in Defold, build & run. Click/tap to move the observer randomly and watch hot/warm chunk counts change.

## Next steps
- Add SoA systems: PathSystem, GolemSystem, MachineSystem, FluidSystem.
- Keep per-system SoA stored **per chunk** (for locality), with global sparse handles if needed.
- Batch rendering sync by dumping transforms into a Defold `buffer` (later).
