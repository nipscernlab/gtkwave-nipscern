# GTKWave

GTKWave is a fully featured GTK+ based wave viewer for Unix and Win32 which reads FST, and GHW files as well as standard Verilog VCD/EVCD files and allows their viewing.

## Building GTKWave from source

### Installing dependencies

Debian, Ubuntu:

```sh
apt install build-essential meson gperf flex desktop-file-utils libgtk-3-dev libgtk-4-dev \
            libbz2-dev libjudy-dev libgirepository1.0-dev libjson-glib-dev
```

Fedora:

```sh
dnf install meson gperf flex glib2-devel gcc gcc-c++ gtk3-devel gtk4-devel \
            gobject-introspection-devel desktop-file-utils tcl
```

macOS:

```sh
brew install desktop-file-utils shared-mime-info       \
             gobject-introspection gtk-mac-integration \
             meson ninja pkg-config gtk+3 gtk4 json-glib
```

### Building GTKWave


```sh
git clone "https://github.com/gtkwave/gtkwave.git"
cd gtkwave
meson setup build && cd build && meson install
```

### Running GTKWave
```sh
gtkwave [path to a .vcd, .fst, .ghw dump file or a .gtkw savefile]
```
For more information about available command line parameters refer to the built in-help (`gtkwave --help`) or the [`gtkwave` man page](https://gtkwave.github.io/gtkwave/man/gtkwave.1.html).

## Local customizations

This fork has the following changes on top of upstream:

1. **New CLI flag `-Z` / `--zoom-fit`** — forces an initial Zoom Fit (same as the toolbar button) as soon as the main loop starts, so the full waveform is visible on launch.
   - `src/main.c`: registers the option in `long_options` / `getopt_long`, adds `case 'Z'` that sets `GLOBALS->do_initial_zoom_fit`, and schedules `service_zoom_fit` via `g_idle_add(initial_zoom_fit_idle_cb, ...)` immediately before `gtk_main()`.

2. **Dark theme for the signal-name panel** — the existing `-6` / `--dark` flag only set `gtk-application-prefer-dark-theme`, which did not affect the cairo-drawn signal list. A dark palette is now applied when `GLOBALS->use_dark` is set.
   - `lib/libgtkwave/src/gw-color-theme.{c,h}`: new `gw_signal_list_colors_new_dark()` (background `#2a2a2a`, text `#e6e6e6`, row `#333333`, selection colors preserved).
   - `src/signal_list.c`: in `draw()`, picks the dark palette when `GLOBALS->use_dark` is set (mirrors the existing `black_and_white` branch).

3. **SST (Signal Search Tree) panel completely removed** — the hierarchy tree on the left side is no longer constructed; the main window contains only the signal list and the waveform area.
   - `src/main.c`: the block that built `toppanedwindow` / `sstpane` / `expanderwindow` is replaced with `NULL` assignments; the matching `gtk_paned_pack*` for the SST is removed. The notebook page already had a `toppanedwindow ? toppanedwindow : panedwindow` fallback, so it now uses `panedwindow` directly.
   - `src/globals.c`: the reload-time SST rebuild is guarded with `if (GLOBALS->expanderwindow)` so it becomes a no-op.

4. **New CLI flag `-L` / `--left-justify`** — left-justifies the signal names in the signal panel (instead of the default right-justified layout). This exposes the existing `left_justify_sigs` rcvar (also reachable from the **Edit → Set Left Justify Signals** menu entry) as a command-line option.
   - `src/main.c`: registers the option in `long_options` / `getopt_long`, adds `case 'L'` that sets `GLOBALS->left_justify_sigs`.

5. **Child processes spawned with `CREATE_NO_WINDOW` on Windows** — when `gtkwave.exe` is itself a GUI-subsystem binary (see customization 6 below), any console-subsystem child it spawns would otherwise pop up a stray `cmd`-like window for a fraction of a second. The `CreateProcess` flags at the three call sites below were changed from `0` to `CREATE_NO_WINDOW` so the children stay invisible:
   - `src/pipeio.c:81-82` — `pipeio_create()`, used for process filters such as `comp2gtkw` (the `^>1 ...exe` lines in `.gtkw` savefiles).
   - `src/menu.c:2407` — `menu_new_viewer_cleanup()`, the "open a second viewer" helper that re-executes `gtkwave.exe`.
   - `src/main.c:2431-2432` — `activate_stems_reader()`, the rtlbrowse child launched from a `.stems` file.
   - `windows.h` and the related symbols are already pulled in via the existing `STARTUPINFO` usage near `pipeio.c:33`, so no new `#include` is needed.

6. **Windows GUI subsystem** — `gtkwave.exe` is linked with `-Wl,--subsystem,windows` so no console window pops up alongside the GUI when launched on Windows. Together with customization 5, this gives a fully GUI-only experience for both `gtkwave` and its child processes.
   - `src/meson.build`: adds `win_subsystem: 'windows'` to the `gtkwave` executable target (passing the flag via raw `link_args` doesn't work — Meson appends `-Wl,--subsystem,console` *after* user link args, and ld respects the last `--subsystem`).

### Recommended invocation

To launch GTKWave with dark theme, initial zoom-fit, left-justified signal names, and no SST (matches the current visualization):

```sh
gtkwave --dark --zoom-fit --left-justify path/to/dump.vcd path/to/save.gtkw
```

Short form:

```sh
gtkwave -6 -Z -L dump.vcd save.gtkw
```

On Windows the installed tree at `C:\packs\gtkwave-bin\` ships a launcher that sets the DLL search path:

```cmd
C:\packs\gtkwave-bin\gtkwave.cmd --dark --zoom-fit --left-justify dump.vcd save.gtkw
```

### Building on Windows (MSYS2 MINGW64)

The upstream README only covers Linux/macOS. On Windows with MSYS2 at `C:\packs\msys64`, install the toolchain inside an MSYS2 MINGW64 shell:

```sh
pacman -S --needed mingw-w64-x86_64-{toolchain,gtk3,gtk4,pkgconf,glib2,gobject-introspection,gperf,meson,ninja,desktop-file-utils} flex
```

Then configure, build and install (Python's `Path.home()` used by meson needs `USERPROFILE` exported when invoked from non-MSYS2 shells):

```sh
export USERPROFILE="C:\\Users\\<you>"
meson setup --prefix=C:/packs/gtkwave-bin -Djudy=disabled build
meson install -C build
```

Incremental rebuilds: `meson install -C build` (or just `meson compile -C build` to build without installing).
