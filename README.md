# GTA V Enhanced unter Linux mit AuraV (Path Tracing) & DLSS 5 (Neural Rendering)

Vollständige Anleitung und benötigte Dateien, um **GTA V Enhanced** unter Linux (Nobara / Fedora / SteamOS / Ubuntu) über **Proton / GE-Proton** gleichzeitig mit **AuraV (Path Tracing + F10-Dock)** und **ReShade + RenoDX DLSS 5 Neural Rendering** stabil zum Laufen zu bringen.

Getestet auf:
- **OS:** Nobara Linux (Wayland / KDE)
- **GPU:** NVIDIA GeForce RTX 4090 (Treiber 560+ / aktuell)
- **Proton:** GE-Proton 11-12
- **Mods:** AuraV Lite 0.3.2, ReShade 6.8 + `renodx-dlss5.addon64` (`DLSSNR 310.8.2.0`)

---

## Warum funktioniert das ohne Anpassungen unter Linux nicht?

1. **Fehlende Windows Compression API in Wine/Proton (`cabinet.dll`):**
   - `AuraV.asi` entpackt seine Shader-Datenbank (`AuraV.bank`, >20.000 Shader) über `cabinet.dll` (`CreateDecompressor` / `Decompress` / `CloseDecompressor`) mit Algorithmus `4` (`COMPRESS_ALGORITHM_XPRESS_HUFF`) inklusive 28-Byte-Windows-Buffer-Header.
   - Wine/Proton implementiert in `cabinet.dll` nur die alte FCI/FDI-API und in `ntdll.dll` kein Format `4` (`RtlGetCompressionWorkSpaceSize`).
   - **Folge ohne Fix:** `path tracing failed to start`, F10 zeigt nur den Mauszeiger oder das Spiel stürzt beim Laden der Welt mit einer Null-Pointer-Exception ab.
   - **Lösung in diesem Repo:** Eine eigene `cabinet.dll` ([`cabinet-proxy/`](cabinet-proxy/)), die auf der Microsoft-XPRESS-Huffman-Referenzimplementierung (`pyxca`) basiert (inkl. Fix für eine Endlosschleife beim Überlappungs-Kopieren am Blockende und automatischer 28-Byte-Header-Erkennung).
2. **Fehlende Windows-Schriftarten (`DWrite.dll`):**
   - Ohne Standard-TrueType-Schriftarten (`arial.ttf`, `segoeui.ttf`, etc.) im Proton-Prefix schlägt DirectWrite beim Rendern des AuraV-F10-Docks fehl.
3. **Getrennte NVIDIA-DLLs für AuraV & DLSS 5:**
   - **AuraV** benötigt seine eigene `nvngx_dlssd.dll` (Ray Reconstruction, v310.9.0) für die Streamline-Signaturprüfung.
   - **DLSS 5** (`renodx-dlss5.addon64`) benötigt zusätzlich die signierte `nvngx_dlssnr.dll` (Neural Rendering) direkt im Spielverzeichnis.
4. **Wine DLL-Overrides & VKD3D-Proton Flags:**
   - Sowohl ReShade (`dxgi.dll`) als auch `cabinet.dll`, `msdmo.dll`, `d3dcompiler_47.dll` und `nvngx` müssen über `WINEDLLOVERRIDES` geladen werden.

---

## Repository-Inhalt

- [`cabinet-proxy/`](cabinet-proxy/)
  - `cabinet.dll` – vorkompilierte 64-Bit-Windows-DLL für Proton/Wine
  - `my_cabinet_ms.c`, `pyxca_xpress.c`, `xpress.h`, `cabinet.def` – vollständiger Quellcode
  - `build.sh` – Build-Skript zum Selbstkompilieren unter Linux (`gcc` + `ld -m i386pep`)
- [`configs/`](configs/)
  - `AuraV.ini` – getestete Beispielkonfiguration für AuraV
  - `ReShade.ini` – getestete Beispielkonfiguration für ReShade + `[RenoDX.DLSS5]`

---

## Schritt-für-Schritt-Anleitung

### 1. Voraussetzungen
- Aktueller proprietärer **NVIDIA-Treiber** (Serie 560+ oder neuer).
- **GE-Proton** (z. B. `GE-Proton11-12`) in Steam für *Grand Theft Auto V Enhanced* (App-ID `3240220`) ausgewählt.
- Das Spiel einmal normal starten, bis ins Grafikmenü gehen und aktivieren:
  - **NVIDIA DLSS** (z. B. *Quality*)
  - **Raytracing** (Raytraced Global Illumination, Shadows, Reflections)
- **Hinweis:** AuraV funktioniert nur im **Story-Modus** (BattlEye für Mods über Startoption `-nobattleye` deaktiviert).

---

### 2. AuraV installieren
Entpacke das AuraV-Archiv direkt in das Hauptverzeichnis von GTA V Enhanced (neben `GTA5_Enhanced.exe`), z. B.:
`~/.local/share/Steam/steamapps/common/Grand Theft Auto V Enhanced/`

Dort müssen liegen:
- `AuraV.asi`
- `msdmo.dll`
- `AuraV/` (mit `AuraV.bank` und `AuraV.ini`)
- `nvngx_dlssd.dll` (**wichtig:** die originale `nvngx_dlssd.dll` aus dem AuraV-Paket verwenden, nicht durch fremde Versionen überschreiben!)

---

### 3. `cabinet.dll` Proxy installieren (oder selbst bauen)

**Variante A – Vorkompilierte DLL nutzen:**
Kopiere `cabinet-proxy/cabinet.dll` direkt ins Hauptverzeichnis von GTA V Enhanced (neben `GTA5_Enhanced.exe`).

**Variante B – Selbst kompilieren:**
Benötigt `gcc`, `binutils` (`ld`, `objcopy`) und eine Wine-Installation mit `x86_64-windows/libkernel32.a`:
```bash
cd cabinet-proxy
./build.sh
# Optional mit Debug-Log (schreibt cabinet_proxy.log ins Spielverzeichnis):
# DEBUG_LOG=1 ./build.sh
```
Kopiere die erzeugte `cabinet.dll` anschließend ins Hauptverzeichnis von GTA V Enhanced.

---

### 4. Windows-Schriftarten in das Proton-Prefix kopieren
Damit DirectWrite (`DWrite.dll`) das F10-Menü von AuraV sauber rendern kann, kopiere Standard-Windows-Schriftarten (`arial.ttf`, `segoeui.ttf`, `tahoma.ttf`, `verdana.ttf`, `consola.ttf` usw. – z. B. von einer Windows-Installation oder über `winetricks corefonts`) in das Fonts-Verzeichnis des GTA-V-Prefix:
```text
<SteamLibrary>/steamapps/compatdata/3240220/pfx/drive_c/windows/Fonts/
```

---

### 5. ReShade + DLSS 5 (`renodx-dlss5.addon64`) installieren
Lege folgende Dateien ebenfalls ins Hauptverzeichnis von GTA V Enhanced (neben `GTA5_Enhanced.exe`):
1. **ReShade (mit Addon-Support)** als `dxgi.dll`
2. **RenoDX DLSS 5 Addon:** `renodx-dlss5.addon64`
3. **NVIDIA Neural Rendering Runtime:** `nvngx_dlssnr.dll` (z. B. `310.8.2.0` / aus dem DLSS5-OneClick-Cache)

> **Wichtig:** `nvngx_dlssd.dll` (AuraV) und `nvngx_dlssnr.dll` (DLSS 5) bleiben beide nebeneinander im Spielverzeichnis liegen.

Optional kannst du [`configs/ReShade.ini`](configs/ReShade.ini) und [`configs/AuraV.ini`](configs/AuraV.ini) als Vorlage übernehmen (ggf. `IntermediateCachePath` in `ReShade.ini` anpassen).

---

### 6. Steam-Startoptionen setzen
Trage in Steam unter *Eigenschaften → Allgemein → Startoptionen* für GTA V Enhanced ein:

```bash
DISABLE_LSFGVK=1 DISABLE_VKBASALT=1 VK_LOADER_LAYERS_DISABLE="*lsfg*,*vkbasalt*" VKD3D_DEBUG=warn DXVK_NVAPI_DRIVER_VERSION=62070 PROTON_ENABLE_NVAPI=1 PROTON_ENABLE_NGX_UPDATER=1 VKD3D_CONFIG=dxr,dxr11 WINEDLLOVERRIDES="dxgi=n,b;cabinet=n,b;d3dcompiler_47=n,b;msdmo=n,b;nvngx=n,b" %command% -nobattleye
```

> **Hinweis zu `WINEDLLOVERRIDES`:**
> - `dxgi=n,b` lädt ReShade (`dxgi.dll`)
> - `cabinet=n,b` lädt unseren `cabinet.dll`-Proxy für `AuraV.bank`
> - `msdmo=n,b` lädt den ASI-Loader von AuraV
> - `d3dcompiler_47=n,b` & `nvngx=n,b` sorgen für Shader-Kompilierung und DLSS/Streamline

*(Falls du ein detailliertes Wine-Log für die Fehlersuche brauchst, kannst du temporär `PROTON_LOG=1` voranstellen – im Normalbetrieb weglassen, da das Log sehr groß wird.)*

---

## Empfohlene Einstellungen (RTX 4090 @ 2560×1440)

### GTA V Grafikmenü
- **NVIDIA DLSS:** `Quality` (oder `Balanced` für mehr FPS)
- **Raytracing (GI, Reflections, Shadows):** `An`

### AuraV (`F10` / `AuraV/AuraV.ini`)
- `denoiser=1` (Ray Reconstruction), `restir=1`, `restir_gi=1`, `radiance_cache=1` **anlassen**.
- Falls die FPS zusammen mit DLSS 5 zu stark sinken: `rays_per_pixel=1` und `bounces=2` setzen (großer FPS-Gewinn bei nahezu gleicher Optik).

### ReShade (`Einfg`) & DLSS 5 Neural Rendering
- **ReShade `Start`-Tab (`.fx`-Shader):** Alles **ausgeschaltet lassen**! Shader wie `SSR`, `RadiantGI`, `MXAO` oder `Bloom` würden nur FPS kosten und AuraVs natives Path Tracing verfälschen.
- **Reiter `DLSS 5 Neural Rendering`:**
  - **Ansatzpunkt:** `Nach Upscaling`
  - **HDR-Modus:** `Auto (empfohlen)`
  - **Durchläufe (`NRPasses`):** `1`
  - **Unabhängige Haut (`NRSkinIndependent`):** `An`
  - **UI-Korrektur (`NRUICorrection`):** `Aus`
  - **Detailstabilität (`NRDetailStability`):** `Immer` (oder `Auto (empfohlen)`)
  - **Lokaler Ton / Struktur & Änderungsstärke:** Bei Path Tracing oft zwischen `0.65` und `0.85` natürlicher als `1.00`.
  - Mit **F6** lässt sich DLSS 5 jederzeit live im Spiel an- und ausschalten.

---

## Fehlerbehebung

- **AuraV startet kein Path Tracing (`AuraV/AuraV.log` -> `path tracing failed to start`):**
  - Prüfen, ob im GTA-V-Menü *NVIDIA DLSS* und *Raytracing* aktiviert sind.
  - Prüfen, ob `VKD3D_CONFIG=dxr,dxr11` und `DXVK_NVAPI_DRIVER_VERSION=62070` gesetzt sind.
  - Prüfen, ob `nvngx_dlssd.dll` aus dem AuraV-Archiv stammt.
- **F10 öffnet nur den Mauszeiger oder Absturz beim Welt-Load:**
  - Prüfen, ob `cabinet=n,b` in `WINEDLLOVERRIDES` steht und `cabinet.dll` aus diesem Repo neben `GTA5_Enhanced.exe` liegt.
  - Zum Debuggen `cabinet.dll` mit `DEBUG_LOG=1 ./build.sh` bauen und `cabinet_proxy.log` im Spielverzeichnis prüfen.
- **ReShade zeigt „NR-Laufzeit nicht installiert“:**
  - `nvngx_dlssnr.dll` fehlt neben `GTA5_Enhanced.exe` / `renodx-dlss5.addon64`.
- **ReShade öffnet sich nicht (`Einfg`):**
  - Prüfen, ob `dxgi=n,b` in `WINEDLLOVERRIDES` steht und `ReShade.log` im Spielverzeichnis prüfen.
