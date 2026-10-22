# conV

A fast, portable file format converter with a retro-futuristic interface. One small native C++20 / Qt 6 binary, resizable, Wayland-first, themeable from the command line with live reload.

| Tab | Converts | Powered by |
| --- | --- | --- |
| Photo | PNG, JPG, WebP, AVIF, HEIC, GIF, BMP, TIFF, ICO, JXL, SVG | ImageMagick |
| Video | MP4, MKV, WebM, MOV, AVI, FLV, WMV, GIF and more | FFmpeg |
| Audio | MP3, WAV, FLAC, OGG, Opus, M4A, AAC, WMA, AIFF | FFmpeg |
| Docs | Markdown, HTML, DOCX, ODT, EPUB, RST, LaTeX, TXT, Org | Pandoc |

conV looks the tools up on your `PATH` at startup. If one is missing, only that tab is disabled and it says what to install.

## Build and run

Requires CMake 3.21+, a C++20 compiler, and Qt 6.5+ (Core, Gui, Widgets, DBus; Test for the test suite, Wayland plugin for Wayland). On Arch: `cmake ninja gcc qt6-base qt6-wayland`.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/conV
```

Files can be dropped on the window, added with **Add files** (**Ctrl+O**, through your system's configured file chooser), or passed on the command line: `conV photo.heic clip.mov`.

## Using it

1. Pick a tab (**Ctrl+1…4**) and add files.
2. Choose an output format and, where it applies, quality or bitrate.
3. Save beside the originals, or choose a folder.
4. **Convert** (**Ctrl+Enter**). Files run one at a time; **Cancel** stops the current file and skips the rest.

The source file is never an output target. If the destination has the same format and folder, the result is named `name-converted.ext`. If a result already exists, an in-window prompt offers **Replace**, **Keep both**, or **Cancel**. Each result is written to a hidden temporary file and moved into place only on success, so a failed or cancelled run never damages an existing file. A failed file shows a **Details** button with the tool's output.

Every dialog is drawn inside the window over a blurred backdrop; conV never opens a second popup window (the file chooser is the one exception: it is your system's own, reached through the XDG Desktop Portal).

## Theming

Colors live in `~/.config/conV/theme.json`. Every theme command rewrites that file atomically and every open window re-themes immediately, without losing its queue.

```bash
conV theme list
conV theme preset amber            # phosphor, amber, ice, paper
conV theme set 'accent=#ff8800' 'background=#e00b120e'
conV theme show
conV theme follow ~/palettes/mine  # track a palette and re-theme whenever it changes
conV theme import palette.json
conV theme export mine.json
```

Add `--dry-run` to any change to preview the JSON without writing. `--config-dir DIR` (or `CONV_CONFIG_DIR`) relocates everything.

**Palette keys:** `background`, `surface`, `surface_alt`, `text`, `muted`, `accent`, `accent_text`, `border`, `line`, `hover`, `active`.

**Opacity.** The background may be `#RRGGBB` or `#AARRGGBB` (Qt order, alpha first): `#e00e1e36` is the color `#0e1e36` at 88% opacity. `Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)` is read the same way. The window is translucent where the compositor supports it; where it does not, only the alpha is dropped and the RGB channels are untouched. All other colors are opaque.

**Following a palette.** `theme follow PATH` accepts a QML color file (`"#RRGGBB"`, `"#AARRGGBB"` or `Qt.rgba(...)` properties), a JSON palette, or a directory containing `Colors.qml`, `palette.json`, or `colors.json`. The path is remembered as written, so a symlink that is later pointed somewhere else, or a file that is rewritten in place or replaced atomically, is picked up automatically and `theme.json` is updated to match.

All icons are vector shapes painted in the theme's colors; the launcher icon (`~/.config/conV/icon.svg`) is regenerated from the theme too. The UI is flat: solid colors, hard edges, chamfered corners, no gradients.

## Install

```bash
scripts/install-desktop.sh [--follow PATH|none] [--prefix DIR] [--config-dir DIR]
```

Installs the binary, a launcher entry (`~/.local/bin/applications/io.github.conv.desktop`, linked into `~/.local/share/applications`), registers conV in `~/.config/apps.list` if present, and optionally follows a palette. Existing files are backed up first, and `--dry-run` shows the plan.

## Tests

```bash
cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build
ctest --test-dir build --output-on-failure
CONV_SHOT_DIR=/tmp/conv-shots ctest --test-dir build -R ui   # also writes screenshots
```

Covers alpha parsing (hex and `Qt.rgba`), save/reload stability, symlink retargeting and in-place palette rewrites, command construction, output-collision rules, real conversions, the in-window collision prompt, and the CLI.
