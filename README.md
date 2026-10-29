# Guitar Hero: Warriors of Rock — PC (native recompilation)

An unofficial PC port of **Guitar Hero: Warriors of Rock** (Xbox 360), made by statically recompiling the original game code from PowerPC to x86-64. It is not an emulator: the game runs natively on your CPU.

This is a continuation of **[re-wor](https://github.com/ronniecloud/re-wor) by ronniecloud**, who started the project. This fork adds note/music sync fixes, DLC support, native Xbox One guitars, 2× resolution, 60 FPS animations and a launcher.

> [!IMPORTANT]
> **This project does not include any game files.** You need your own legally acquired copy of Guitar Hero: Warriors of Rock for Xbox 360.
> Not affiliated with Activision, Neversoft or Microsoft. "Guitar Hero" is a trademark of its respective owners.

[**Download the latest release**](https://github.com/Gu1r1ngacho/guitar-hero-warriors-of-rock-pc/releases/latest) · [Español](#español)

## Features

- **Notes in sync with the music** for the whole song (fixed timing of the game clock, vblank and song clock).
- **~60 FPS**, with an optional **60 FPS animations** mode (characters and stage; the original game animates them at 30).
- **2× resolution** (2560×1440) with correct instrument textures.
- **Xbox One guitars read natively** (PDP Riffmaster, PDP Jaguar, Mad Catz Stratocaster) — no RB4InstrumentMapper needed. Adjustable tilt sensitivity for Star Power.
- **Keyboard as a guitar** (remappable) and configurable player order (guitar / keyboard / controllers).
- **DLC without extracting**: put the packages (LIVE/CON files) in the `GH_DLC` folder. Custom songs (Onyx) work with a song cache.
- **Audio** of all song tracks in sync, and sound in the Bink videos.
- **Launcher** in English, Spanish, French, German and Italian (the game language is set from there too).
- Lower CPU usage (~45-60 % instead of 100 %).

## System requirements

- Windows 10/11, 64-bit.
- **CPU with AVX2** (Intel 4th gen / 2013 or newer, AMD Ryzen or newer).
- GPU with DirectX 12.
- Your own copy of the game (Xbox 360 disc).

## How to install

1. Download the ZIP from [Releases](https://github.com/Gu1r1ngacho/guitar-hero-warriors-of-rock-pc/releases/latest) and extract it to a folder (for example `C:\Games\GHWoR`).
2. Extract the contents of **your** Xbox 360 disc (with [extract-xiso](https://github.com/XboxDev/extract-xiso) or similar) into the `assets` folder. It must look like this:
   ```
   assets/
   ├── default.xex
   └── data/
   ```
3. Run **`Launcher.exe`**, choose your options and press **Play**.

### DLC and custom songs

- Put the DLC packages in the `GH_DLC` folder (subfolders are fine).
- Official DLC only: nothing else is needed.
- Custom songs or fan packs (made with [Onyx](https://github.com/mtolly/onyx)): you **must** create a song cache with Onyx (*Other tools → GH:WoR cache*) including **all** your packages, official ones too. The launcher has a **"How do I create the cache?"** button with the steps.

### Useful keys in game

| Key | Action |
|---|---|
| F3 | Show/hide FPS |
| F4 | Technical settings of the SDK |
| F11 | Fullscreen |

## Known issues

- An occasional hang when a song starts streaming (the game keeps drawing but waits).
- A crash when closing the game (after it has finished; it does not affect saves).
- Occasional FPS drops (52-58) at 2× resolution.
- Microphone is not supported.
- Online features and leaderboards are not available (the official servers are closed).

Found a bug? Open an [issue](https://github.com/Gu1r1ngacho/guitar-hero-warriors-of-rock-pc/issues). If the game crashed, attach `re_wor_crash_detail.log` (next to the game) and, if logging is enabled in the launcher, `logs\juego.log`.

## Building from source

Requirements: Clang 20+ (LLVM), CMake 3.25+, Ninja, Visual Studio Build Tools (Windows SDK), Git. Use the **x64 Native Tools Command Prompt**.

```cmd
git clone https://github.com/Gu1r1ngacho/guitar-hero-warriors-of-rock-pc.git re-wor
cd re-wor

:: ReXGlue SDK v0.2.2 + this project's patches
git clone --recursive --branch v0.2.2 https://github.com/rexglue/rexglue-sdk.git thirdparty\rexglue-sdk
git -C thirdparty\rexglue-sdk apply ..\..\parches_sdk.patch

:: Git for Windows stores these libmspack symlinks as text files: replace them with the real files
for %f in (cab.h cabd.c lzx.h lzxd.c macros.h mspack.h mszip.h mszipd.c qtm.h qtmd.c readbits.h readhuff.h system.c system.h) do copy /y thirdparty\rexglue-sdk\thirdparty\libmspack\libmspack\mspack\%f thirdparty\rexglue-sdk\thirdparty\libmspack\cabextract\mspack\%f

:: Copy your default.xex to assets\default.xex, then:
cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR=thirdparty/rexglue-sdk -DCMAKE_C_FLAGS="-march=x86-64-v3" -DCMAKE_CXX_FLAGS="-march=x86-64-v3"
cmake --build out/build/win-amd64-relwithdebinfo --target re_wor_codegen

:: Configure again (so the generated sources are included) and build
cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR=thirdparty/rexglue-sdk -DCMAKE_C_FLAGS="-march=x86-64-v3" -DCMAKE_CXX_FLAGS="-march=x86-64-v3"
cmake --build out/build/win-amd64-relwithdebinfo
```

The game is built as `Guitar Hero Warriors of Rock.exe` and the launcher as `launcher\Launcher.exe`. `-DREWOR_DEV=ON` enables extra diagnostic logs.

> [!NOTE]
> This project uses ReXGlue SDK **v0.2.2**. Newer SDK versions changed the API and are not compatible yet.

## Project structure

```
├── src/
│   ├── main.cpp          # Entry point, crash handler, game function replacements (timing, DLC, CPU)
│   ├── stubs.cpp         # Xbox 360 API stubs
│   └── discord_rpc.cpp   # Discord Rich Presence
├── launcher/             # Launcher (Win32)
├── parches_sdk.patch     # Changes to ReXGlue SDK v0.2.2 (audio, GPU, input, DLC, timing...)
├── re_wor_config.toml    # Codegen configuration
├── generated/            # Recompiled code (generated, not in the repository)
└── assets/               # Your game files (not in the repository)
```

## Credits

- **[ronniecloud](https://github.com/ronniecloud/re-wor)** — creator of re-wor, on which this fork is based.
- **[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)** (Tom Clay) and the **[Xenia](https://xenia.jp)** project — recompilation toolchain and Xbox 360 runtime.
- **[YoshiCrystal9](https://github.com/YoshiCrystal9/re-gh2)** — re-gh2, the base reference for re-wor.
- **[RB4InstrumentMapper](https://github.com/TheNathannator/RB4InstrumentMapper)** (TheNathannator) — Xbox One guitar input mapping.
- **[Onyx](https://github.com/mtolly/onyx)** (mtolly) — custom song and cache tools.
- **Neversoft / Activision** — the original game.

---

## Español

Port no oficial para PC de **Guitar Hero: Warriors of Rock** (Xbox 360), por recompilación estática (no es un emulador). Continúa el proyecto **[re-wor de ronniecloud](https://github.com/ronniecloud/re-wor)**.

**No incluye archivos del juego**: necesitas tu propia copia original de Xbox 360.

**Instalación:**
1. Descarga el ZIP de [Releases](https://github.com/Gu1r1ngacho/guitar-hero-warriors-of-rock-pc/releases/latest) y descomprímelo en una carpeta.
2. Extrae el contenido de **tu** disco de Xbox 360 (con extract-xiso o similar) en la carpeta `assets` (debe quedar `assets\default.xex` y `assets\data\`).
3. Abre **`Launcher.exe`**, elige idioma, resolución y controles, y presiona **Jugar**.

**DLC:** ponlos en la carpeta `GH_DLC`. Si usas canciones custom o packs de fans hechos con Onyx, crea un cache con Onyx que incluya **todos** los paquetes (también los oficiales); el botón "¿Cómo crear el cache?" del launcher explica los pasos.

**Requisitos:** Windows 10/11 de 64 bits, procesador con AVX2 (Intel desde 2013, AMD Ryzen), tarjeta de video con DirectX 12.
