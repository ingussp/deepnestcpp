# deepnestcpp

C++20 port of computational behavior from:
- `deepnest-next/deepnest`
- `main/background.js`
- pinned commit: `4d8b57f85ffc1831069549e3555eb7d97c408967`

## What is included

- Polygon domain model (points, `exact`, metadata, child polygons/holes)
- Geometry helpers (`mergedLength`, shift/rotate, hull, area-with-holes, conversion helpers)
- In-memory `NfpCache`
- Outer and inner NFP computation paths with caching and pair preprocessing
- Alternative bitmap nesting path with rasterized occupancy checks
- Greedy `placeParts` implementation with `gravity`, `box`, and `convexhull` strategies
- Safe C++20 multithreaded warm-up for independent unique NFP pairs
- Event-sink orchestration API replacing Electron IPC (`background-start` equivalent)
- CLI demo and automated tests

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run demo

```bash
./build/deepnestcpp_demo --count 20 --algorithm nfp --threads 1 --output /tmp/nfp.dxf
./build/deepnestcpp_demo --count 20 --algorithm bitmap --bitmap-resolution 1.0 --bitmap-step 1 --threads 8 --output /tmp/bitmap.dxf
./build/deepnestcpp_demo --count 20 --algorithm bitmap --bitmap-resolution 1.0 --debug-placement --output /tmp/bitmap-debug.dxf
```

The demo uses the supplied `neregulara_zvaigzne.svg` path in **normalized local coordinates after applying the SVG group's `scale(1,-1)` flip**. The group translation is treated as SVG canvas placement and is not baked into the part, so the exported DXF remains in millimetres and contains the correct transformed outline for each placed copy.

The demo sheet is `1500 mm × 1500 mm` and DXF is exported in millimetres.

For a full run with 2,000 copies:

```bash
./build/deepnestcpp_demo --count 2000 --algorithm nfp --threads 8 --output /tmp/star-demo-2000.dxf
```

The demo prints algorithm, sheet size, count, workers, bitmap resolution, SIMD backend (`scalar`/`avx2`), placed/unplaced counts, utilisation, and timing breakdown:

- `timing.setup`
- `timing.nfp_precompute`
- `timing.placement`
- `timing.bitmap`
- `timing.dxf_export`
- `timing.orchestrator_total`
- `timing.total_with_output`

`--threads 1` is a baseline. Worker threads do **not** run the entire greedy sequence concurrently; they are used for independent work (for example NFP warm-up), while accepted placements are still committed sequentially for deterministic non-overlapping results.

## Algorithms

- `--algorithm nfp` keeps the existing NFP path as the reference/correctness implementation.
- `--algorithm bitmap` uses rasterized occupancy checks with configurable `--bitmap-resolution <mm-per-pixel>` (default `1.0`).
- `--debug-placement` prints per-part bitmap progress with candidate/reject counters and part processing time.
- `--bitmap-step <px>` controls deterministic bitmap candidate scan spacing (`1` = full scan, larger values = faster but less dense search).
- Bitmap placements are raster-quantized. Smaller values improve precision but increase CPU/memory cost.
- If raster dimensions become too large, the program rejects the run with a clear error and asks for a larger `--bitmap-resolution`.

### Bitmap performance notes and debug counters

- Previous slow path behavior was dominated by `raster x/y search × rotations × repeated per-candidate vector overlap checks`.
- Current bitmap path keeps occupancy bitmap checks as the primary collision filter and runs expensive vector geometry validation only on accepted candidates (or when explicitly configured).
- Raster masks are cached by `geometry identity + rotation`, so repeated identical parts reuse the same bitmap mask.

When `--debug-placement` is enabled, each processed part logs:

- `candidates`: candidate placements examined (x/y/rotation attempts)
- `boundary_rejects`: candidates rejected because the mask would exceed sheet raster bounds
- `bitmap_collision_rejects`: candidates rejected by occupancy/material bitmap checks
- `vector_rejects`: candidates rejected by vector geometry validation (enabled by default for accepted candidates)
- `part_ms`: per-part elapsed placement time

At the end of bitmap mode, the demo prints aggregate processed/placed/unplaced counts, total candidates, reject totals, mask cache size, SIMD backend, and timing fields.

### AVX2 backend

- Bitmap execution uses scalar code by default on unsupported CPUs/builds.
- On AVX2-capable builds, runtime detection selects 256-bit row operations (`avx2`) automatically; otherwise it falls back to `scalar`.
- AVX2 intrinsics are isolated to bitmap implementation code and are not required for the whole binary.
- On MSVC/Windows, AVX2 code is guarded: runtime checks are required before execution, and scalar fallback always works.

## Thread model

- The greedy placement loop remains sequential. Accepted placements stay in input order because each accepted placement changes the next valid region.
- Only independent missing unique outer-NFP warm-up work is parallelized.
- `NfpCache` is guarded with a shared mutex, and cache lookups return copies so no caller holds references past lock release.
- Progress callbacks for parallel warm-up are marshalled from the orchestrator thread, and the demo prints coarse progress updates to avoid flooding the console.
- Ideal `--threads` values depend on CPU count, shape complexity, and how many unique NFP pairs exist in the workload.

### Windows build and DXF export

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd C:\dev\deepnestcpp\build\Debug
deepnestcpp_demo.exe --count 20 --algorithm nfp --threads 1 --output nfp.dxf
deepnestcpp_demo.exe --count 20 --algorithm bitmap --bitmap-resolution 1.0 --threads 8 --output bitmap.dxf
deepnestcpp_demo.exe --count 20 --algorithm bitmap --bitmap-resolution 1.0 --debug-placement --output bitmap-debug.dxf
```

This creates DXF files in millimetres that can be opened in LibreCAD, QCAD, AutoCAD, or Fusion 360.

## Notes

- Clipper2 is fetched with CMake `FetchContent` for reproducible setup.
- Inner-NFP hole handling is implemented with polygon-material intersection/erosion style clipping and validated again during placement with explicit overlap/outside checks.
- Repeated identical parts reuse cached shape-pair NFP geometry keyed by full polygon identity plus rotation, while each placed instance still contributes its own translated exclusion region during placement.
- Bitmap mode also reuses rasterized masks for identical geometry+rotation.
