# Quickshot

Quickshot je Windows program za snimak dela ekrana. Radi iz system tray-a: prečica otvori zatamnjeni overlay, izabere se region, po želji se doda crtež ili tekst, pa se snimak kopira, sačuva, odštampa ili pošalje na upload.

Autor: **Ivan Stojmenovic**

Trenutna verzija u kodu: **1.2.1**

## Šta program radi

- Snimak regiona na jednom ili više monitora
- Olovka, linija, strelica, pravougaonik, elipsa, marker, tekst (bold, italic, underline, veličina)
- Undo / redo
- Copy u clipboard, Save kao PNG ili JPEG, štampa
- Opcioni upload (podrazumevano `https://0x0.st`) i kopiranje linka
- Globalne prečice, autostart sa Windowsom
- Osam jezika interfejsa: engleski, srpski, nemački, francuski, španski, italijanski, ruski, hrvatski
- Provera ažuriranja preko JSON manifesta

Program nema glavni prozor. Posle starta ostaje ikona u notification area.

## Zahtevi

- Windows 10 ili noviji, 64-bit
- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/community/) sa workload-om **Desktop development with C++**
- MSVC toolset **v143**
- Windows 10 SDK (dolazi uz taj workload)

To je dovoljno da se projekat prevede. Quickshot ne vuče NuGet pakete, vcpkg, Qt, ni bilo koju drugu C++ biblioteku.

Za installer je potreban i [Inno Setup 6](https://jrsoftware.org/isinfo.php). Za ponovno crtanje toolbar ikona treba Python 3 i Pillow. Ni jedno ni drugo nije potrebno za običan build.

## Kako preuzeti

Instalacija Gita, ako već nije na mašini: <https://git-scm.com/download/win>

```bat
git clone https://github.com/ivke013/Quickshot.git
cd Quickshot
```

U Visual Studio: **File → Open → Project/Solution** i izabrati `Quickshot.sln` iz korena repozitorijuma.

## Kako prevesti i pokrenuti

1. Otvoriti `Quickshot.sln`.
2. Konfiguracija: **Release**, platforma: **x64**.
3. **Build → Build Solution**.
4. Pokrenuti `build\x64\Release\Quickshot.exe`.

Isti rezultat iz Developer PowerShell-a:

```bat
msbuild Quickshot.sln /p:Configuration=Release /p:Platform=x64
build\x64\Release\Quickshot.exe
```

Debug build ide u `build\x64\Debug\`. Folder `build\` se ne commituje.

Posle prvog pokretanja pored exe-a nastaje `settings.ini`. Taj fajl je i konfiguracija instalirane i portable verzije. Stariji `%APPDATA%\MyScreenShot\settings.ini` se jednom prekopira ako lokalni fajl još ne postoji.

Podrazumevana prečica za region je **Shift+F11**. Ceo ekran se čuva na **Shift+Print Screen**. Obe se menjaju u Options.

## Instalacija za krajnjeg korisnika

`scripts\make-release.ps1` prevede Release|x64, spakuje portable zip i, ako je Inno Setup instaliran, napravi setup:

```bat
powershell -ExecutionPolicy Bypass -File scripts\make-release.ps1
```

Rezultat:

- `dist\Quickshot-1.2.1-portable-x64.zip`
- `dist\Quickshot-Setup-1.2.1.exe`

Setup se instalira u `%LOCALAPPDATA%\Programs\Quickshot` i ne traži administratorska prava. Verzija u `installer\Quickshot.iss` mora da prati `APP_VERSION_W` u `src\Resource.h`.

## Biblioteke

Sve dolazi iz Windows SDK-a. Linkuje se direktno iz koda (`#pragma comment(lib, ...)`):

| Biblioteka | Čemu služi |
|---|---|
| `gdiplus.lib` | Crtanje, PNG/JPEG čuvanje |
| `windowscodecs.lib` | WIC, učitavanje PNG ikona |
| `shell32.lib` | Tray ikona, folderi, ShellExecute |
| `comctl32.lib` | Common Controls 6 (tabovi, SysLink) |
| `comdlg32.lib` | Izbor boje i Save dijalog |
| `ole32.lib` | COM, koji GDI+ i clipboard koriste |
| `winhttp.lib` | Upload i provera ažuriranja |

Manifest traži Common Controls 6 i Per-Monitor V2 DPI.

Van exe-a, samo za održavanje:

| Alat | Čemu služi |
|---|---|
| Inno Setup 6 | `installer\Quickshot.iss` |
| Python 3 + [Pillow](https://python-pillow.org/) | `scripts\gen_icons.py` regeneriše toolbar PNG |
| Python 3 | `scripts\gen_loc.py` regeneriše `src\Loc.cpp` |

## Struktura

```
Quickshot.sln          solution, otvara se ovaj fajl
src\                   C++ izvor, resursi, manifest, vcxproj
assets\icons\          ikone aplikacije i tray-a
assets\toolbar\32\     toolbar glyph, 32×32
assets\toolbar\64\     toolbar glyph, 64×64
installer\             Inno Setup skripta i podrazumevani settings.ini
scripts\               release, prevodi, generisanje ikona
examples\              primer JSON manifesta za update
build\                 izlaz prevodioca (nije u gitu)
```

Moduli u `src\`:

| Modul | Uloga |
|---|---|
| `Quickshot.cpp` | Ulaz, skriveni prozor, message loop |
| `TrayIcon` | Ikona u notification area i meni |
| `HotkeyManager` | `RegisterHotKey` |
| `ScreenCapture` | Snimak virtuelnog desktopa |
| `CaptureOverlay` | Fullscreen overlay, selekcija, toolbar |
| `Annotation` | Crteži i undo/redo stek |
| `Exporter` | Clipboard, PNG/JPEG, štampa |
| `Uploader` | HTTP upload |
| `UpdateChecker` | Manifest i zamena exe-a |
| `Settings` | `settings.ini` pored exe-a |
| `OptionsDialog` | Options |
| `Loc` | Prevodi |
| `AboutDialog` | O programu |

## Principi

**Samo Win32.** Interfejs, snimak i crtež idu preko Win32 API-ja i GDI+. Nema UI frameworka koji bi morao da se instalira pored programa.

**Tray, ne glavni prozor.** Proces drži skriveni `HWND` zbog poruka, hotkey-eva i tray-a. Overlay postoji samo dok traje snimak.

**Jedan fajl podešavanja pored programa.** Nema upisa u `%APPDATA%` za novo instaliranje. Portable folder i Setup dele isti model, pa se program može prekopirati bez instalera.

**Mali moduli, direktni pozivi.** Svaki `.cpp` pokriva jedan posao. Nema plugin sistema ni apstraktnog frameworka. To drži projekat čitljivim u jednom Visual Studio solutionu.

**Izlaz prevodioca nije izvor.** `OutDir` je `build\<platform>\<config>\`. U git ulaze izvor, ikone, installer i skripte.

**Tekst za korisnika je u jednoj tabeli.** `scripts\gen_loc.py` je izvor prevoda. Posle izmene treba ponovo generisati `src\Loc.cpp`, a ne ručno prepravljati tu tabelu.

**Unicode i DPI.** Izvor je UTF-8 (`/utf-8`), prozori su wide API, manifest je Per-Monitor V2. Toolbar ima 32 i 64 px glyph pa se na 100% i 150% ne rasteže ista slika.

**Jezik standarda je C++17.** Koriste se standardni kontejneri (`std::wstring`, `std::vector`). Vlasništvo GDI objekata je eksplicitno, bez pametnih pokazivača oko `HBITMAP` gde bi to sakrilo `DeleteObject`.

## Podešavanja i update

`installer\settings.ini.default` je početni fajl. Bitna polja:

- `[General]` obaveštenja, kursor, jezik, autostart
- `[Hotkeys]` modifikatori i virtuelni kod tastera
- `[Formats]` PNG ili JPEG, šablon imena, folder
- `[Cloud]` upload URL
- `[Proxy]` opcioni HTTP proxy
- `[Update]` URL manifesta

Primer manifesta je u `examples\update-manifest.example.json`:

```json
{
  "version": "1.2.1",
  "url": "https://YOUR-HOST/releases/Quickshot.exe",
  "notes": "Bug fixes and improvements"
}
```

`version` mora biti novija od `APP_VERSION_W` da bi program ponudio zamenu. URL treba da vodi na novi `Quickshot.exe`.

## Autor

Ivan Stojmenovic

Copyright (c) 2026 Ivan Stojmenovic
