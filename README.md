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
./build/deepnestcpp_demo
```

### Windows DXF export

```powershell
cd C:\dev\deepnestcpp\build\Debug
deepnestcpp_demo.exe --output result.dxf
```

This creates `C:\dev\deepnestcpp\build\Debug\result.dxf`, which can be opened in LibreCAD, QCAD, AutoCAD, or Fusion 360.

## Notes

- Clipper2 is fetched with CMake `FetchContent` for reproducible setup.
- Inner-NFP hole handling is implemented with polygon-material intersection/erosion style clipping and validated again during placement with explicit overlap/outside checks.
