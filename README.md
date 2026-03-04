<div align="center">

<img src="icon.png" width="128" alt="Reckless Drivin' icon">

# Reckless Drivin'

[![macOS](https://img.shields.io/github/actions/workflow/status/DarrCoh/reckless-drivin-sdl/build.yml?label=macOS&logo=apple)](https://github.com/DarrCoh/reckless-drivin-sdl/actions/workflows/build.yml)
[![Linux](https://img.shields.io/github/actions/workflow/status/DarrCoh/reckless-drivin-sdl/build.yml?label=Linux&logo=linux)](https://github.com/DarrCoh/reckless-drivin-sdl/actions/workflows/build.yml)

### A native SDL2 port of the classic Mac OS racing game

*Originally by [Jonas Echterhoff](https://github.com/jechter/RecklessDrivin) (2000), brought back to life on modern hardware*

**macOS (Apple Silicon & Intel) &bull; Linux &bull; SDL2 &bull; No Emulator Required**

---

<!-- Replace these with actual screenshots once captured -->
<!-- ![Main Menu](screenshots/menu.png) -->
<!-- ![Gameplay](screenshots/gameplay.png) -->

*640x480 software-rendered, pixel-perfect; just like it was in 2000*

</div>

---

## The Story

Reckless Drivin' was one of *those* games. The kind that came pre-installed (or maybe my brothers found it on some shareware CD) on our family Mac back in the day. My brothers and I played it for hours, weaving through traffic, smashing into things, trying to beat each other's times.

Years later, I got hit with a tiny wave of nostalgia. I wanted to play it again, for old time's sake. Not in an emulator, not through some janky compatibility layer, but **running natively on my MacBook**.

The original source code had been [released on GitHub](https://github.com/jechter/RecklessDrivin) by Jonas Echterhoff in 2019 under the MIT license. How hard could it be, you'd say? Turns out: *very*. The original code was pure Classic Mac OS: PowerPC, big-endian, 32-bit, built with CodeWarrior, using DrawSprocket, InputSprocket, the Mac Sound Manager, and Mac Resource Forks. None of that exists anymore. Every platform API had to be replaced. Every byte of game data was stored backwards relative to my CPU. It was a rabbit hole, and I went all the way down.

This is the result: a port of Reckless Drivin' with all 10 levels, every screen, and every effect, running natively with pixel-perfect graphics, proper audio, and full mouse + keyboard + gamepad controls.

---

## Features

- **All 10 levels fully playable:** including the encrypted shareware levels, properly decrypted with the [free registration key](http://jonasechterhoff.com/Reckless_Drivin.html)
- **24 driveable vehicles:** sports cars, muscle cars, motorcycles, semi trucks, a tank, a helicopter, police interceptors, boats, and more; all selectable via the cheat mode level select
- **Pixel-perfect rendering:** 640x480 ARGB1555, software-rendered and uploaded via SDL2 streaming texture
- **Full audio:** 8-channel software mixer at 44.1 kHz stereo, with Doppler effect and 3D positional sound
- **Every screen implemented:** main menu, help, high scores, preferences, pause, game over, and scrolling credits
- **High score table:** top 10 scores rendered on-screen with keyboard name entry
- **Screen transitions:** fade to/from black, venetian-blind shift-in effect, game over animation
- **Menu polish:** hover highlights and press states on all buttons, click sound feedback
- **Remappable controls:** rebind all 8 game keys from the Preferences menu
- **Gamepad support:** SDL2 GameController API with analog stick + triggers
- **Resizable window:** maintains 4:3 aspect ratio with letterboxing
- **Persistent preferences:** settings, high scores, lap records, and key bindings saved between sessions
- **Native macOS app bundle:** builds as a proper `.app` with the original 2002 icon, ad-hoc signed, SDL2 bundled inside; zip it and send to a friend
- **Broad compatibility:** macOS 11.0+ (Big Sur through current), Linux (x86_64, ARM64)
- **No external dependencies on macOS:** SDL2 is bundled into the app automatically; no Homebrew or manual installs needed to run

---

## Quick Start

### Requirements

| | |
|---|---|
| **OS** | macOS (Apple Silicon or Intel), Linux |
| **Compiler** | Clang or GCC (C11) |
| **Build tool** | CMake 3.16+ |
| **Library** | SDL2 |

### Build & Run

```bash
# Install SDL2 (macOS)
brew install cmake sdl2

# Install SDL2 (Ubuntu/Debian)
sudo apt install cmake libsdl2-dev

# Build
cd RecklessDrivin-SDL
mkdir build && cd build
cmake ..
cmake --build .

# Run (macOS: double-click or from terminal)
open RecklessDrivin.app

# Run (Linux)
./RecklessDrivin
```

On macOS, the build produces a self-contained `RecklessDrivin.app` bundle with the original game icon, the `Data` file, and SDL2 bundled inside. The app is ad-hoc code signed automatically. On Linux, it builds a standard executable and copies `Data` next to it.

### Distribution

To share the game, just zip the `.app` and send it; no SDL2 install required on the recipient's machine:

```bash
cd build
zip -r RecklessDrivin.zip RecklessDrivin.app
```

Since the app is ad-hoc signed (no Apple Developer ID), macOS will quarantine it when downloaded. Recipients need to run this once in Terminal before the first launch:

```bash
xattr -cr /path/to/RecklessDrivin.app
```

(Tip: type `xattr -cr ` then drag the app onto the Terminal window to fill in the path.)

---

## Controls

### Keyboard (defaults, remappable in Preferences)

| Action | Key |
|---|---|
| Accelerate | `Up` |
| Brake / Reverse | `Down` |
| Steer Left | `Left` |
| Steer Right | `Right` |
| Kickdown | `Left Shift` |
| Brake | `Space` |
| Fire | `Z` |
| Missile | `X` |
| Pause | `P` |
| Quit | `Escape` |

To rebind keys: **Preferences > Controls > Configure**

### Menu Shortcuts

| Action | Key |
|---|---|
| Start Game | `Enter` / `S` / `N` |
| Preferences | `P` |
| High Scores | `C` / `O` |
| Help | `H` |
| Quit | `Q` |
| Level Select | `Shift` + `Enter` |

### Level Select (Cheat Mode)

Press `Shift` + `Enter` from the main menu to open the level select screen. From here you can:

- **Pick any level (1-10):** with names, descriptions, and time limits shown
- **Choose from 24 vehicles:** with a live sprite preview, name, and description
- Navigate with arrow keys or mouse, confirm with `Enter`

<details>
<summary>Full vehicle list</summary>

| | Land | | Heavy | | Water |
|---|---|---|---|---|---|
| | Sports Car | | Semi Truck | | Speedboat |
| | Muscle Car | | Delivery Truck | | Cargo Ship |
| | Sedan | | Bus | | Barge |
| | Station Wagon | | Monster Truck | | Police Boat |
| | Compact | | Tank | | Armed Patrol Boat |
| | Coupe | | APC | | |
| | Buggy | | Helicopter | | |
| | Motorcycle | | | | |
| | Sport Bike | | | | |
| | Police Car | | | | |
| | Police Bike | | | | |
| | Police Chopper | | | | |

</details>

### Gamepad

D-pad for steering, left stick for analog control, face buttons for actions, triggers for kickdown/brake, Start for pause.

### Mouse

Click the license plate buttons on the main menu. Click anywhere to dismiss help, score, and pause screens.

---

<details>
<summary><h2>The Porting Journey</h2></summary>

This wasn't a simple recompile. The original game was written for a world that no longer exists.

### The Platform Layer

The original game used five major Mac-specific APIs. Every one had to be replaced:

| Original Mac API | Purpose | SDL2 Replacement |
|---|---|---|
| DrawSprocket | Screen management, page flipping, gamma fades | SDL2 window + renderer + streaming texture |
| InputSprocket | Gamepad/keyboard input abstraction | SDL2 event polling + keyboard state + mouse |
| Sound Manager | Multi-channel audio mixing | Custom 8-channel software mixer |
| Resource Manager | Loading assets from resource forks | Custom resource fork parser |
| DriverServices | High-resolution timing | `SDL_GetPerformanceCounter()` |

### The Compatibility Layer

The original code uses Mac-specific types everywhere: `UInt8`, `UInt16`, `Handle`, `Str255`, `nil`... `compat.h` maps all of these to modern C equivalents, plus provides `NewPtr()`, `NewHandle()`, `BlockMoveData()`, and byte-swapping macros.

### The Resource Fork Parser

Mac OS stored data in "resource forks": a structured binary format with typed, numbered entries. The game's `Data` file contains 22 `Pack` resources (compressed game data), 10 `PPic` resources (PICT images in both 8-bit and 16-bit formats), and 1 `Chck` resource. The custom parser (`resources.c`) reads the resource map and returns data by type + ID. Pack resources use LZRW3-A compression (public domain, by Ross Williams).

### Endianness: Everything Is Backwards

PowerPC is big-endian. Apple Silicon is little-endian. Every multi-byte value in every data structure was stored in the wrong byte order: 16-bit pixels, 32-bit object fields, RLE sprite tokens, sound headers, floating-point road data, and even the XOR encryption key for locked levels. Getting any one of these wrong produced garbled colors, corrupted sprites, or audio that sounded like a fax machine.

### The `sizeof(long)` Bug

On 32-bit PowerPC, `sizeof(long) == 4`. On 64-bit ARM64, `sizeof(long) == 8`. The original RLE renderer had manual copy loops using `long`-sized chunks; each iteration copied 8 bytes instead of 4, overwriting adjacent memory and causing immediate SIGBUS crashes. Four separate rendering functions had to be rewritten.

### The RLE Sprite Format

Custom 4-byte token format (1 byte type + 3 bytes big-endian data). Token parsing used `*(unsigned long*)ptr`: wrong endianness AND wrong size on ARM64. Every token parser in the game had to be replaced with explicit byte reads.

### The Sound System

Sound data contains embedded Mac SoundHeader structures (not raw PCM). Standard headers (22 bytes, 8-bit unsigned, ~11 kHz) and Extended headers (64 bytes, 16-bit signed BE, 44.1 kHz) are mixed throughout. The mixer had to detect the header type, extract the actual PCM data, and apply per-channel rate correction (`basePitch = nativeRate / 44100`).

### The PICT Renderer

Menu screens are stored as Mac PICT v2 images with LZRW3-A compression. Two pixel formats are used: 16-bit DirectBitsRect (`0x009A`) for most screens, and 8-bit indexed PackBitsRect (`0x0098`) with embedded color tables for others. Both formats use PackBits row compression.

### The Encryption

Levels 4-10 were encrypted in the original shareware distribution. The XOR-based decryption operates on 32-bit words in memory, but the encryption was applied on big-endian PowerPC, so the key's byte pattern in memory is reversed on little-endian ARM64. A byte-swap of the key before XOR was required for decryption to produce valid data. The free registration key (name: `Free`, code: `B3FB09B1EB`) was [published by the author](http://jonasechterhoff.com/Reckless_Drivin.html) when the game went freeware.

### The Pascal String Off-by-One

One crash only appeared during gameplay when picking up a specific power-up. The "ADDONS LOCKED" text effect used a Pascal string: a format where the first byte is the length, followed by that many characters. The length byte said 15 (`\x0f`), but the actual string `"ADDONShLOCKEDf"` was only 14 characters. On the original Mac, the text renderer had no bounds checking; it would read the 15th byte (a NUL from the struct initializer), try to look up font glyph 63 (`0 - 'A' + 128`) in the sprite pack, land on some random memory, and get away with it. On the SDL port, bounds checking on pack lookups returned NULL, and `NULL + 8` became a segfault. A bug that silently survived 25 years on PowerPC.

### The NULL Safety Audit

The Pascal string crash revealed a broader pattern: the original code *never* checks return values. `NewObject()` allocates a game object and looks up its type definition from a resource pack; if the type ID doesn't exist, the original Mac code would silently read garbage memory and keep going. `GetSortedPackEntry()` and `GetUnsortedPackEntry()` are called dozens of times across the codebase, and every single callsite assumes success. On PowerPC with flat memory and no ASLR, dereferencing a wild pointer might corrupt something silently or just happen to land on valid-ish data. On ARM64 with strict memory protection, it's an instant segfault.

A full audit of every pack lookup callsite across the codebase found 15 unguarded dynamic-ID paths: places where the lookup ID comes from gameplay state (object types, weapon projectiles, debris, explosions, smoke, font glyphs) rather than hardcoded constants. Each was given a NULL guard with graceful degradation: `NewObject()` unlinks and frees on failure, `KillObject()` falls back to `RemoveObject()`, particle effects silently skip, and sprite draws bail out early. The result is a game that can survive a missing resource entry without crashing; something the original never needed because the Mac OS memory model was far more forgiving.

### The Optimization Pass

A performance audit of the finished port found it was mostly clean: no memory leaks, no redundant texture creation, no O(n^2) collision loops, packs decompressed once and cached. But the build had been running at `-O0` (no optimizations) the entire time. Enabling `-O3` with Link-Time Optimization shrank the binary from 272 KB to 211 KB and gave the compiler room to inline and optimize across all 30+ source files. The one hot-path issue was `pow(10, i)` being called every frame for score rendering: a floating-point exponentiation where an integer lookup table (`{1, 10, 100, 1000, ...}`) does the same job in a single array access.

### The App Bundle

The original Reckless Drivin' was a proper Mac application: double-clickable, with an icon in the Dock. The SDL port should be too. CMake builds a native `.app` bundle on macOS: `Info.plist` for metadata, the original 2002 `.icns` icon recovered from an old copy of the game, the `Data` file in `Contents/Resources`, and critically, SDL2 itself bundled inside `Contents/Frameworks`. At build time, `install_name_tool` rewrites the executable's dynamic library reference from the Homebrew path (`/opt/homebrew/opt/sdl2/lib/...`) to `@executable_path/../Frameworks/libSDL2.dylib`, making the app fully self-contained. The whole bundle is ad-hoc code signed so macOS doesn't overlay a prohibition symbol on the Dock icon. The deployment target is set to macOS 11.0 (Big Sur), the first macOS release with Apple Silicon support, so the app runs on every Apple Silicon Mac ever made. The goal was a `.app` you can zip and send to a friend.

</details>

---

## Architecture

```
                        +----------------+
                        |    main.c      |
                        |  Game Loop     |
                        +-------+--------+
                                |
          +------------+--------+--------+--------------+
          |            |        |        |              |
     +----v----+ +-----v--+ +--v--+ +---v------+ +----v-----+
     |Renderer | |Interface| |Sound| |Physics   | | Objects  |
     | Road    | | Menus   | |Mixer| |Collision | | Control  |
     | Sprites | | Prefs   | |     | |          | |          |
     | RLE/FX  | | HUD     | |     | |          | |          |
     +----+----+ +----+----+ +--+--+ +----------+ +----------+
          |           |         |
     +----v-----------v---------v-----------------------------+
     |                platform_sdl.c                          |
     |     Screen  |  Input  |  Audio  |  Timing  | Mouse    |
     +----------------------------+---------------------------+
                                  |
     +----------------------------v---------------------------+
     |                    SDL2 Library                        |
     +--------------------------------------------------------+

     +--------------------------------------------------------+
     |  compat.h  |  resources.c  |  lzrw.c  |  textrender   |
     |  Mac types    Resource fork   LZRW3-A    Bitmap font   |
     +--------------------------------------------------------+
```

---

## Project Structure

```
RecklessDrivin-SDL/
├── CMakeLists.txt           # Build configuration (app bundle + SDL2 bundling on macOS)
├── Info.plist.in            # macOS app bundle metadata template
├── Reckless.icns            # Original 2002 game icon (from Jonas Echterhoff)
├── README.md
├── Data                     # Original game resources (Mac resource fork)
├── include/
│   ├── compat.h             # Mac OS type compatibility layer
│   ├── platform.h           # SDL2 platform abstraction API
│   ├── resources.h          # Resource fork parser
│   ├── textrender.h         # Shared bitmap font rendering
│   ├── lzrw.h               # LZRW3-A decompression
│   └── ...                  # Original game headers
└── src/
    ├── main.c               # Entry point & main loop
    ├── platform_sdl.c       # SDL2 backend (screen, input, audio, timing, mouse)
    ├── resources.c           # Mac resource fork parser
    ├── lzrw.c               # LZRW3-A decompressor
    ├── interface.c           # Menu system & PICT renderer (8-bit + 16-bit)
    ├── preferences.c         # In-game preferences UI
    ├── high.c               # High score display & name entry
    ├── textrender.c          # 8x8 bitmap font for UI screens
    ├── gameendseq.c          # Scrolling credits sequence
    ├── screenfx.c            # Screen transitions (shift-in, game over anim)
    ├── sound.c              # SoundHeader parsing & channel management
    ├── renderframe.c        # Main frame renderer
    ├── roaddraw.c           # Road texture rendering
    ├── rle.c                # RLE sprite decompression & drawing
    ├── sprites.c            # Sprite loading & rendering
    ├── textfx.c             # In-game text effect rendering
    ├── objects.c            # Game object management
    ├── objectPhysics.c      # Physics simulation
    ├── objectCollision.c    # Collision detection
    ├── objectControl.c      # AI driving behavior
    ├── gameframe.c          # Per-frame game logic
    ├── gameinitexit.c       # Level loading & game state management
    ├── input.c              # Input processing & key remapping UI
    ├── packs.c              # Pack loading, decryption & decompression
    ├── pause.c              # Pause screen
    ├── particlefx.c         # Particle effects
    ├── random.c             # TT800 pseudo-random number generator
    └── ...                  # Additional modules
```

---

## Known Limitations

- `Cl16` resource id=8 not found at startup (cosmetic; a synthetic fallback palette is used)
- PPic 1009 (original scrolling credits image) contains QuickDraw text commands that can't be rasterized; replaced with bitmap font credits
- Gamepad button remapping is not yet exposed in the UI (SDL2's standard mappings handle most controllers automatically)

---

## Credits

**Original game:** [Jonas Echterhoff](https://github.com/jechter/RecklessDrivin) (2000), released under the MIT License in 2019

**SDL2 port:** [Darren Cohen](https://github.com/DarrCoh)

**Built with:** [Claude Code](https://claude.ai/code)

**LZRW3-A compression:** Ross Williams (public domain)

**SDL2:** [libsdl.org](https://www.libsdl.org/)

**Free registration key:** Published by Jonas Echterhoff at [jonasechterhoff.com](http://jonasechterhoff.com/Reckless_Drivin.html)

**Standing on the shoulders of** [Nate Craddock](https://github.com/natecraddock/open-reckless-drivin), whose [Open Reckless Drivin'](https://github.com/natecraddock/open-reckless-drivin) project and [blog posts](https://nathancraddock.com/blog/open-reckless-drivin/) were an invaluable guide. His reverse-engineering work identifying the LZRW3-A compression format, discovering the 4-byte offset via Ghidra, documenting the XOR decryption key, and mapping the binary structures saved us countless hours. This port wouldn't exist without his trailblazing.

## License

The original Reckless Drivin' source code is released under the [MIT License](https://github.com/jechter/RecklessDrivin/blob/master/LICENSE) by Jonas Echterhoff. This port inherits that license.

---

<div align="center">

*Built because some games are worth bringing back.*

</div>
