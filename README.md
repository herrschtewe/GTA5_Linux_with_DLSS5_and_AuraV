# GTA V Enhanced on Linux with AuraV (Path Tracing) & DLSS 5 (Neural Rendering)

Complete guide and required files to run **Grand Theft Auto V Enhanced** on Linux (Nobara / Fedora / SteamOS / Ubuntu) via **Proton / GE-Proton** with both **AuraV (Path Tracing + F10 Dock)** and **ReShade + RenoDX DLSS 5 Neural Rendering** working side-by-side.

Tested on:
- **OS:** Nobara Linux (Wayland / KDE)
- **GPU:** NVIDIA GeForce RTX 4090 (driver 560+ / current)
- **Proton:** GE-Proton 11-12
- **Mods:** AuraV Lite 0.3.2, ReShade 6.8 + `renodx-dlss5.addon64` (`DLSSNR 310.8.2.0`)

---

## Why This Doesn't Work Out of the Box on Linux

1. **Missing Windows Compression API in Wine/Proton (`cabinet.dll`):**
   - `AuraV.asi` decompresses its shader bank (`AuraV.bank`, >20,000 shaders) via `cabinet.dll` (`CreateDecompressor` / `Decompress` / `CloseDecompressor`) using algorithm `4` (`COMPRESS_ALGORITHM_XPRESS_HUFF`) with a 28-byte Windows Compression API buffer header.
   - Wine/Proton's built-in `cabinet.dll` only implements the legacy FCI/FDI API, and Wine's `ntdll.dll` does not implement format `4` in `RtlGetCompressionWorkSpaceSize`.
   - **Result without fix:** `path tracing failed to start`, pressing `F10` only unhides the mouse cursor without rendering the Dock, or the game crashes on world load with a null-pointer access violation.
   - **Fix in this repo:** A custom `cabinet.dll` ([`cabinet-proxy/`](cabinet-proxy/)) built on Microsoft's reference XPRESS Huffman implementation (`pyxca`), including a fix for an infinite loop during overlapping match copies near block ends and automatic 28-byte header skipping.
2. **Missing Windows Fonts (`DWrite.dll`):**
   - Without standard Windows TrueType fonts (`arial.ttf`, `segoeui.ttf`, etc.) in the Proton prefix, DirectWrite fails when rendering AuraV's F10 Dock UI.
3. **Separate NVIDIA DLLs for AuraV & DLSS 5:**
   - **AuraV** requires its bundled `nvngx_dlssd.dll` (Ray Reconstruction, v310.9.0) to pass Streamline's signature check.
   - **DLSS 5** (`renodx-dlss5.addon64`) additionally requires the signed `nvngx_dlssnr.dll` (Neural Rendering) next to `GTA5_Enhanced.exe`.
4. **Wine DLL Overrides & VKD3D-Proton Flags:**
   - ReShade (`dxgi.dll`), `cabinet.dll`, `msdmo.dll`, `d3dcompiler_47.dll`, and `nvngx` all need explicit `WINEDLLOVERRIDES` entries.

---

## Repository Contents

- [`cabinet-proxy/`](cabinet-proxy/)
  - `cabinet.dll` – prebuilt 64-bit Windows DLL for Proton/Wine
  - `my_cabinet_ms.c`, `pyxca_xpress.c`, `xpress.h`, `cabinet.def` – full source code
  - `build.sh` – build script to compile from source on Linux (`gcc` + `ld -m i386pep`)
- [`configs/`](configs/)
  - `AuraV.ini` – tested reference config for AuraV
  - `ReShade.ini` – tested reference config for ReShade + `[RenoDX.DLSS5]`

---

## Step-by-Step Setup

### 1. Prerequisites
- Up-to-date proprietary **NVIDIA driver** (560+ or newer).
- **GE-Proton** (e.g. `GE-Proton11-12`) selected in Steam for *Grand Theft Auto V Enhanced* (App ID `3240220`).
- Launch the game once normally and enable in the in-game Graphics settings:
  - **NVIDIA DLSS** (e.g. *Quality*)
  - **Ray Tracing** (Raytraced Global Illumination, Shadows, Reflections)
- **Note:** AuraV only works in **Story Mode** (launch with `-nobattleye` to disable BattlEye for mods).

---

### 2. Install AuraV
Extract the AuraV archive into the GTA V Enhanced root directory (next to `GTA5_Enhanced.exe`), e.g.:
`~/.local/share/Steam/steamapps/common/Grand Theft Auto V Enhanced/`

Make sure these files are present:
- `AuraV.asi`
- `msdmo.dll`
- `AuraV/` (containing `AuraV.bank` and `AuraV.ini`)
- `nvngx_dlssd.dll` (**important:** keep the original `nvngx_dlssd.dll` shipped with AuraV; do not overwrite it with a different version!)

---

### 3. Install (or Build) the `cabinet.dll` Proxy

**Option A – Use the prebuilt DLL:**
Copy `cabinet-proxy/cabinet.dll` directly into the GTA V Enhanced root directory (next to `GTA5_Enhanced.exe`).

**Option B – Build from source:**
Requires `gcc`, `binutils` (`ld`, `objcopy`), and a Wine installation containing `x86_64-windows/libkernel32.a`:
```bash
cd cabinet-proxy
./build.sh
# Optional: enable debug logging to cabinet_proxy.log in the game folder:
# DEBUG_LOG=1 ./build.sh
```
Copy the resulting `cabinet.dll` into the GTA V Enhanced root directory.

---

### 4. Install Windows Fonts into the Proton Prefix
To allow DirectWrite (`DWrite.dll`) to render AuraV's F10 Dock UI, install Microsoft Core Fonts into the GTA V Enhanced Proton prefix (App ID `3240220`):

Using **protontricks** (recommended):
```bash
protontricks 3240220 -q corefonts
```

Or using **winetricks** directly (adjust the path if your Steam library is on another drive):
```bash
WINEPREFIX="$HOME/.local/share/Steam/steamapps/compatdata/3240220/pfx" winetricks -q corefonts
```

*(Optionally, you can also copy `segoeui.ttf` / `segoeuib.ttf` from a Windows installation into `<SteamLibrary>/steamapps/compatdata/3240220/pfx/drive_c/windows/Fonts/`.)*

---

### 5. Install ReShade + DLSS 5 (`renodx-dlss5.addon64`)
Place the following files in the GTA V Enhanced root directory (next to `GTA5_Enhanced.exe`):
1. **`dxgi.dll`** — ReShade 6.8+ **with Add-on support** (`ReShade_Setup_6.8.0_Addon.exe`, extract/rename `ReShade64.dll` → `dxgi.dll`).
2. **`renodx-dlss5.addon64`** — RenoDX DLSS 5 Addon (`renodx-dlss5_10.8.0.zip` from the RenoDX DLSS 5 release).
3. **`nvngx_dlssnr.dll`** — NVIDIA's signed Neural Rendering runtime (`nvngx_dlssnr_310.8.SF-v2.zip`, e.g. `310.8.2.0` from NVIDIA Streamline/DLSS 5 SDK or automatically fetched by **DLSS5 OneClick**).

> **Tip:** If you use **DLSS5 OneClick**, it automatically downloads these packages into `~/.cache/dlss5oneclick/downloads/` and installs `dxgi.dll`, `renodx-dlss5.addon64`, and `nvngx_dlssnr.dll` for you.
> **Important:** Do **not** overwrite AuraV's `nvngx_dlssd.dll` — both `nvngx_dlssd.dll` (AuraV) and `nvngx_dlssnr.dll` (DLSS 5) must sit side-by-side in the game root.

Optionally copy [`configs/ReShade.ini`](configs/ReShade.ini) and [`configs/AuraV.ini`](configs/AuraV.ini) as starting presets (adjust `IntermediateCachePath` in `ReShade.ini` if needed).

---

### 6. Set Steam Launch Options
In Steam → *Properties → General → Launch Options*, set:

```bash
DISABLE_LSFGVK=1 DISABLE_VKBASALT=1 VK_LOADER_LAYERS_DISABLE="*lsfg*,*vkbasalt*" VKD3D_DEBUG=warn DXVK_NVAPI_DRIVER_VERSION=62070 PROTON_ENABLE_NVAPI=1 PROTON_ENABLE_NGX_UPDATER=1 VKD3D_CONFIG=dxr,dxr11 WINEDLLOVERRIDES="dxgi=n,b;cabinet=n,b;d3dcompiler_47=n,b;msdmo=n,b;nvngx=n,b" %command% -nobattleye
```

> **What `WINEDLLOVERRIDES` does:**
> - `dxgi=n,b` loads ReShade (`dxgi.dll`)
> - `cabinet=n,b` loads our custom `cabinet.dll` proxy for `AuraV.bank`
> - `msdmo=n,b` loads AuraV's ASI loader
> - `d3dcompiler_47=n,b` & `nvngx=n,b` enable shader compilation and DLSS/Streamline

*(For debugging, you can temporarily prepend `PROTON_LOG=1` to generate `~/steam-3240220.log`; omit it during normal play to avoid large log files.)*

---

## Recommended Settings (RTX 4090 @ 2560×1440)

### GTA V In-Game Graphics
- **NVIDIA DLSS:** `Quality` (or `Balanced` for extra headroom)
- **Ray Tracing (GI, Reflections, Shadows):** `On`

### AuraV (`F10` / `AuraV/AuraV.ini`)
- Keep `denoiser=1` (Ray Reconstruction), `restir=1`, `restir_gi=1`, and `radiance_cache=1` **enabled**.
- If FPS with DLSS 5 on top is too low, reduce `rays_per_pixel=1` and `bounces=2` for a significant FPS boost with minimal visual loss.

### ReShade (`Insert`) & DLSS 5 Neural Rendering
- **ReShade `Home` tab (`.fx` shaders):** Leave **everything disabled**! Effects like `SSR`, `RadiantGI`, `MXAO`, or `Bloom` waste FPS and conflict with AuraV's native path tracing.
- **`DLSS 5 Neural Rendering` tab:**
  - **Hook Point (`Ansatzpunkt`):** `Post Upscaling` (`Nach Upscaling`)
  - **HDR Mode:** `Auto (recommended)`
  - **Passes (`NRPasses`):** `1`
  - **Independent Skin (`NRSkinIndependent`):** `On`
  - **UI Correction (`NRUICorrection`):** `Off`
  - **Detail Stability (`NRDetailStability`):** `Always` (`Immer`) or `Auto (recommended)`
  - **Local Tone / Structure & Strength:** Values around `0.65 – 0.85` often look more natural with path tracing than `1.00`.
  - Press **F6** in-game anytime to toggle DLSS 5 on/off for live A/B comparison.

---

## Troubleshooting

- **AuraV fails to start path tracing (`AuraV/AuraV.log` -> `path tracing failed to start`):**
  - Verify *NVIDIA DLSS* and *Ray Tracing* are enabled in GTA V's Graphics menu.
  - Verify `VKD3D_CONFIG=dxr,dxr11` and `DXVK_NVAPI_DRIVER_VERSION=62070` are set in Steam launch options.
  - Ensure `nvngx_dlssd.dll` is the original file from the AuraV archive.
- **Pressing F10 only shows a mouse cursor, or game crashes on world load:**
  - Ensure `cabinet=n,b` is in `WINEDLLOVERRIDES` and `cabinet.dll` from this repo is next to `GTA5_Enhanced.exe`.
  - Rebuild with `DEBUG_LOG=1 ./build.sh` and inspect `cabinet_proxy.log` in the game folder.
- **ReShade says "NR runtime not installed":**
  - `nvngx_dlssnr.dll` is missing next to `GTA5_Enhanced.exe` / `renodx-dlss5.addon64`.
- **ReShade overlay doesn't open (`Insert`):**
  - Ensure `dxgi=n,b` is in `WINEDLLOVERRIDES` and check `ReShade.log` in the game folder.
