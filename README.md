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
- Greedy `placeParts` implementation with `gravity`, `box`, and `convexhull` strategies
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
./build/deepnestcpp_demo --count 20 --output /tmp/star-demo.dxf
```

The demo uses the supplied `neregulara_zvaigzne.svg` path in **normalized local coordinates after applying the SVG group's `scale(1,-1)` flip**. The group translation is treated as SVG canvas placement and is not baked into the part, so the exported DXF remains in millimetres and contains the correct transformed outline for each placed copy.

For a full run with 2,000 copies on one `2000 mm × 2800 mm` sheet:

```bash
./build/deepnestcpp_demo --count 2000 --output /tmp/star-demo-2000.dxf
```

The full nesting run can take noticeable time depending on hardware.

### Windows build and DXF export

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\deepnestcpp_demo.exe --count 20 --output result.dxf
.\build\Release\deepnestcpp_demo.exe --count 2000 --output result-2000.dxf
```

This creates DXF files in millimetres that can be opened in LibreCAD, QCAD, AutoCAD, or Fusion 360.

## Notes

- Clipper2 is fetched with CMake `FetchContent` for reproducible setup.
- Inner-NFP hole handling is implemented with polygon-material intersection/erosion style clipping and validated again during placement with explicit overlap/outside checks.
- Repeated identical parts reuse cached shape-pair NFP geometry keyed by full polygon identity plus rotation, while each placed instance still contributes its own translated exclusion region during placement.
