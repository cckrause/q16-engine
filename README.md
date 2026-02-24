# q16 Engine

A feature-complete reimplementation of the **Jedi Engine** — the 2.5D portal engine behind *Star Wars: Dark Forces*™ (1995) — written from scratch in **pure C99**.

The goal: build the entire engine from the ground up based on reverse-engineered specifications, targeting **3dfx Glide on Windows 98** with real Voodoo hardware. No OpenGL, no DirectX, no modern abstractions. Just Glide, fixed-point math, and manual memory management.

This is an educational and entertainment project. A deep dive into C programming, retro 3D rendering, and the kind of engine architecture that powered mid-90s shooters before hardware-accelerated GPUs took over.

## Target Platform

| Component | Target |
|---|---|
| OS | Windows 98 (Win32 API) |
| CPU | Pentium I / Pentium II (single-core x86) |
| GPU | 3dfx Voodoo (Glide 2.x) |
| RAM | 32–256 MB |
| Compiler | MinGW GCC (`i686-w64-mingw32-gcc`), C99 |

## Render Pipeline

Portal traversal and visibility are CPU-side. Walls, flats, and sprites are submitted as textured triangles to 3dfx Glide. The display list is the boundary between CPU geometry processing and GPU submission.

```mermaid
graph TD
    START([render_draw_frame]) --> RESET[Reset all buffers]
    RESET --> CAM[Camera transform<br/>eye pos + yaw/pitch]
    CAM --> FRUST[Build camera frustum<br/>left / right / near planes]
    FRUST --> RDS

    subgraph RDS ["Sector Traversal — recursive portal walk"]
        direction TB
        GUARD{{"depth limit?<br/>already drawn?"}}
        GUARD -- skip --> RET([return])
        GUARD -- ok --> WALLS

        subgraph WALLS ["Wall Loop"]
            direction TB
            W0([wall]) --> W1[World → View transform]
            W1 --> W2{backface cull}
            W2 -- yes --> CULL([cull])
            W2 -- no --> W3{frustum test}
            W3 -- out --> CULL
            W3 -- in --> W4[Near + side clip]
            W4 --> W5[Project to screen X]
            W5 --> W6[Texture U, depth, adjoin classify]
            W6 --> SEG([WallSegment])
        end

        SEG --> SBUF[S-Buffer 2D occlusion]
        SBUF --> EMIT[Emit to display list]
        EMIT --> ADJ{open adjoin?}
        ADJ -- no --> NEXT([next wall])
        ADJ -- yes --> PORTAL[Compute portal edge pair<br/>Build child frustum<br/>Save state → recurse → restore]
        PORTAL --> NEXT
    end

    RDS --> DL

    subgraph DL ["Display List — CPU / GPU boundary"]
        direction LR
        OP[("Opaque<br/>walls + flats")]
        TR[("Transparent<br/>signs + mid-tex")]
        PL[("Clip planes<br/>portal bounds")]
    end

    DL --> GPU[HAL draws via Glide / GL backend]

    classDef entry fill:#2d8a56,stroke:#1a5e3a,color:#fff
    classDef culled fill:#b33,stroke:#822,color:#fff
    classDef gpu fill:#3572a5,stroke:#24507a,color:#fff
    classDef default fill:#2b2b2b,stroke:#555,color:#eee
    classDef decision fill:#3b3b3b,stroke:#666,color:#eee

    class START entry
    class CULL culled
    class GPU gpu
    class GUARD,W2,W3,ADJ decision
```

## Dev Tools

These tools run natively on macOS/Linux and are used for development, debugging, and visualization. None of them are part of the final Win32 target.

| Tool | Description |
|---|---|
| `q16_view` | **Wireframe level viewer.** SDL2 + OpenGL 3.3. Loads levels from GOB/LAB archives through the full CPU render pipeline and draws the display list output as colored wireframe quads. Three color modes (part type, sector ID, adjoin depth), WASD + mouse fly-camera, fullscreen 2D top-down minimap with visited-sector highlighting, adjustable max portal depth. |
| `q16_gob` | **GOB archive inspector.** Lists directory entries with file sizes and extensions. |
| `q16_lab` | **LAB archive inspector.** Lists directory entries with file sizes and extensions. |
| `q16_lev` | **Level geometry inspector.** Streaming parse from archive, dumps sector and wall data. |
| `q16_dev` | **Dev harness.** Console-based entry point for testing engine subsystems without a window. |
| `q16_engine` | **Glide 2.x test harness.** Win32 cross-compiled target. Runtime DLL loading of `glide2x.dll`, fullscreen 640x480 rendering, rotating lit quad with software transforms. |

## Status

The CPU render pipeline is complete: portal-based sector traversal, frustum culling, wall clipping, S-buffer occlusion, lighting, display list generation, and object sorting. The wireframe level viewer renders full Dark Forces levels in real time.

527 tests across 20 suites, all passing.

### Not Yet Implemented

- Asset loading (BM textures, WAX sprites, 3DO models, VOC sounds, PAL/CMP palettes)
- 8-bit to 16-bit texture conversion (565/1555)
- Glide HAL (textured walls, flats, sprites via `grDrawTriangle`)
- INF elevator / scripting system
- O file parsing (object placement)
- Collision detection and response
- Player controller and physics
- AI actor logic
- Projectile system
- Pickup / item system
- HUD and automap
- Audio (DirectSound / WinMM, VOC playback, iMuse MIDI)
- Save/load serialization

## Build

### Requirements

**Build host (macOS):**
- [MinGW-w64](https://www.mingw-w64.org/) cross-compiler: `brew install mingw-w64`
- [CMake](https://cmake.org/) 3.10+: `brew install cmake`
- [SDL2](https://www.libsdl.org/) (for `q16_view`): `brew install sdl2`

**Target system:**
- Windows 95/98 with 3dfx Voodoo (real or emulated) and `glide2x.dll`
- Emulators: [86Box](https://86box.net/) or [PCem](https://pcem-emulator.co.uk/) with Voodoo enabled
- Alternatively: [nGlide](https://www.zeus-software.com/downloads/nglide) wrapper on modern Windows

### Win32 Cross-Compile

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake
cmake --build build
```

Output: `build/q16_engine.exe` (PE32)

### Native Dev Build (macOS/Linux)

```bash
cmake -B build
cmake --build build
```

```bash
./build/q16_tests    # unit tests
./build/q16_view     # wireframe level viewer
./build/q16_gob      # GOB archive inspector
./build/q16_lab      # LAB archive inspector
./build/q16_lev      # level geometry inspector
./build/q16_dev      # dev harness
```

## Project Structure

```
src/
  game/
    types/          Fixed16, Vec2/Vec3, Angle14, forward declarations
    math/           Trig tables, core math, transforms
    memory/         Region, game memory
    util/           String utilities
    archive/        GOB + LAB unified read-only archive
    io/             StreamReader, TextParser tokenizer
    world/          Sector, wall, level, level parser, object, texture, model, flags
    render/         Camera, frustum, depth, window, lighting, S-buffer, wall processing,
                    flat tracking, adjoin system, display list, object sorting, sector traversal
  tests/            Unit tests (test runner + 19 suites)
  cli/              Archive and level inspector tools (q16_gob, q16_lab, q16_lev)
  dev/              SDL2 + OpenGL wireframe level viewer (q16_view)
  glide-test/       Win32 + Glide 2.x rendering test harness
lib/
  glad/             OpenGL loader (dev build only)
  glide2/           Glide 2.x API headers (3dfx.h, glide.h)
  glide2x.def       Glide 2.x import library definition
cmake/              MinGW cross-compilation toolchain
specs/              Reverse-engineered Jedi Engine specifications
```

## Trademarks

*Star Wars*, *Dark Forces*, and the Jedi Engine are trademarks or registered trademarks of Lucasfilm Ltd. and/or The Walt Disney Company. This project is not affiliated with or endorsed by Lucasfilm or Disney.

## Licence

GPLv3 with attribution clause. See [LICENCE](LICENCE).
