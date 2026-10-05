# Open Inventor on libx11-compat

[Open Inventor](https://github.com/aumuell/open-inventor) (SGI's 2.1 scene-graph
toolkit, the codebase behind the *Inventor Mentor* and *Inventor Toolmaker*
books) builds and renders against libx11-compat, including its Motif viewers
(`SoXt`). `mk/open-inventor.mk` drives the build, on Linux with `GLX=1`.

## What it links against

Every X11, Motif and OpenGL dependency comes from this tree; the host only
supplies libjpeg, freetype, iconv and the Mesa/GLVND runtime:

| Open Inventor needs | Provided by |
|---|---|
| Xlib, Xt | `libX11-compat`, `libXt-compat` |
| XInput (`SoXtSpaceball`) | `libXi-compat` (`compat/xi-compat.c`): reports no extra devices |
| Motif (`SoXt` viewers, editors) | the in-tree Motif build (`mk/motif.mk`), not a system `libXm` |
| GLX | `libx11-compat` (`src/glx.c`), including the pbuffer-backed `glXCreateGLXPixmap` used by `SoOffscreenRenderer` |
| OpenGL | GLVND's gl-only `libOpenGL`, dispatching to the Mesa desktop compatibility context libx11-compat creates on EGL (the direct path, no gl4es) |
| GLU (tessellator, NURBS) | [mesa/glu](https://gitlab.freedesktop.org/mesa/glu) 9.0.3, compiled here against `libOpenGL` |

Open Inventor's CMake build finds its dependencies with `find_package()`, which
searches the host first. `scripts/open-inventor-cache.cmake` (passed with
`cmake -C`) pins each X11/Motif/OpenGL result to the in-tree library and points
the include directories at a staged sysroot (`build/open-inventor/sysroot`,
which holds `X11/`, `Xm/` and `GL/`). `check-open-inventor` then resolves the
dependencies of `libInventor`, `libInventorXt` and two examples with `ldd`. It
fails if any X11, Motif or GLU library resolves outside `build/`, or if
`libGL.so` or `libGLX` appears at all. `libGL.so` is excluded because it carries
GLVND's own `glX*`, which would shadow libx11-compat's.

## Build and run

Host packages (Debian/Ubuntu): the Motif chain (`autoconf automake libtool bison
flex`), plus `cmake g++ libjpeg-dev libfreetype-dev libopengl-dev libegl-dev
libgl1-mesa-dri fontconfig`, and `xvfb` for headless runs.

```sh
make GLX=1 open-inventor                     # Motif, GLU, Open Inventor + examples
scripts/run-open-inventor.sh                 # list the built programs
scripts/run-open-inventor.sh 02.4.Examiner   # open a Mentor example on screen
scripts/run-open-inventor.sh 02.4.Examiner --snapshot out.png   # headless
make GLX=1 check-open-inventor               # link audit + headless renders
```

Open Inventor's font library opens fonts by PostScript-style name
(`Times-Roman`, `Helvetica-Bold`, the `Utopia-Regular` fallback, ...) under
`FL_FONT_PATH`, and no package installs files under those names.
`make open-inventor` therefore links each name to the closest host font
(`fc-match`) in `build/open-inventor/fonts`, and `run-open-inventor.sh` points
`FL_FONT_PATH` there. Without it, `SoText2`/`SoText3` draw nothing.

`run-open-inventor.sh` selects GLVND's `libEGL.so.1` as the EGL provider
(`LIBX11_COMPAT_EGL`). GLVND has to be the provider: the `gl*` from `libOpenGL`
only reach the context when GLVND's libEGL made it current. Headless snapshots
also default to `EGL_PLATFORM=surfaceless`.

## Your own programs

Drop a source file in `examples/inventor/` and build it with the same headers,
flags and libraries as the Mentor examples:

```sh
cp my-scene.c++ examples/inventor/
make GLX=1 open-inventor-examples            # builds build/open-inventor/examples/my-scene
scripts/run-open-inventor.sh my-scene
```

`examples/inventor/inventor-shapes.c++` is a small starting point: three lit
shapes on a turntable in an `SoXtExaminerViewer`. The default language mode is
`-std=c++98`, matching Open Inventor's own build; set `OI_USER_STD=c++11` (or
later) for newer code.

Write programs against the SGI Open Inventor 2.1 API with the `SoXt` (Motif)
component library. Code written for Coin3D's extensions, or for `SoQt`,
`SoWin` or `SoGtk`, needs porting first.

## Patches

`compat/open-inventor-patches/` holds four small changes, applied on checkout:

1. `0001` adds `#include <GL/gl.h>` next to `#include <GL/glx.h>` in the
   headers that relied on Mesa's `glx.h` pulling in `gl.h`. libx11-compat's
   `GL/glx.h` deliberately does not, so GLES clients need no desktop GL headers.
2. `0002` makes `SoXt` draw new scenes through the back buffer by default. On an
   expose it drew the first frame straight to `GL_FRONT` and only `glFlush`ed
   it, a workaround for old SGI hardware. libx11-compat composites a window when
   it is swapped, so that frame stayed black.
   `setDrawToFrontBufferEnable(TRUE)` still restores the old behavior.
3. `0003` makes the examples read their `.iv`/`.rgb` data from the source tree
   instead of a hard-coded `/usr/share/src/Inventor/examples/data`.
4. `0004` fixes a 64-bit bug in `libimage`, the SGI `.rgb` reader behind
   `SoTexture2`. The on-disk header fields and RLE row tables are 32-bit, but
   were declared `long`. On LP64 that overran the heap: a textured scene
   aborted with "double free or corruption", or drew nothing. Unrelated to
   libx11-compat, but every texture example depends on it.

Two compat-layer changes came out of the port and help any GLX client:
`glXChooseVisual` now rejects a non-zero `GLX_LEVEL` (there are no overlay
planes; `SoXt` used to get an opaque overlay window composited over its scene),
and `glXCreateGLXPixmap` exists.

## Status: Mentor examples

All 66 *Inventor Mentor* C++ examples build, and 54 of them render headless.
That was checked with `scripts/run-open-inventor.sh <name> --snapshot`,
`assert-image-content.py --region` on the GL canvas, and a visual check of every
capture. The renders cover the examiner and render-area viewers, materials and
lights, 2D/3D/beveled text, textures, NURBS curves and trimmed surfaces,
engines and sensors, picking, manipulators, draggers, the Motif material editor
and GL callbacks.

The other 12:

| Example | Why no frame was captured |
|---|---|
| `03.3.Naming`, `09.3.Search`, `09.5.GenSph`, `12.2.NodeSensor` | console programs, no window |
| `05.5.Binding`, `09.1.Print`, `12.1.FieldSensor`, `12.4.TimerSensor` | need command-line arguments |
| `10.2.setEventCB` | empty until points are added with the mouse |
| `17.3.GLFloor` | renders and swaps, but then `sleep()`s without calling Xlib, so the replay-driven snapshot never fires |
| `16.1.Overlay`, `17.1.ColorIndex` | need overlay planes / a color-index visual (see below) |

`check-open-inventor` keeps `02.1.HelloCone` (`SoXtRenderArea`) and
`02.4.Examiner` (`SoXtExaminerViewer` with its Motif decorations) as the CI
render gate.

![Open Inventor Mentor examples rendered through libx11-compat](../assets/open-inventor.png)

## Limitations

- Linux + Mesa only: the direct path needs GLVND `libOpenGL` and Mesa's desktop
  GL over EGL. On macOS the gl4es/ANGLE path would be needed instead (not wired
  up).
- A frame is shown when it is swapped. Single-buffered rendering, or front-buffer
  drawing that only calls `glFlush`, does not reach the screen until the next
  swap.
- No overlay planes and no color-index visuals, so `SoXt` overlay scene graphs
  and color-index rendering (Mentor `16.1.Overlay`, `17.1.ColorIndex`) are not
  drawn.
- Spaceball and dial-box input devices report as absent.

## Roadmap: Windows

The longer-term goal is to run the same old X11/Motif and Open Inventor
applications natively on Windows, with SDL3 as the window and input backend.
Their original sources would need only minimal porting, without a Windows
component library (`SoWin`, or Coin3D's) and with `SoXt` kept as it is. None of
this exists yet. The work, in order:

1. **libx11-compat on MinGW-w64, `SDL_BACKEND=sdl3`.** About 27 files in `src/`
   and `compat/` use POSIX APIs: `pthread` (winpthreads or Win32 threads),
   `dlopen`/`dlsym` (`LoadLibrary`/`GetProcAddress`, also for the EGL loader),
   and `poll`/`select`/`unistd` in the event loop. Produce DLLs plus import
   libraries, and gate the work with a cross-compiled build in CI, run under
   Wine.
2. **A GLX provider on Windows.** The Linux direct path (GLVND `libOpenGL` +
   Mesa EGL) has no Windows counterpart. Candidates:
   - Mesa for Windows (llvmpipe/d3d12) exposing desktop GL through EGL or WGL,
     to keep the legacy GL 1.x surface Open Inventor relies on (`GL_SELECT`,
     feedback, display lists);
   - ANGLE + gl4es, the macOS path (GLES ceiling, see the GLX limitations in
     the README);
   - a WGL backend in `src/glx.c`, with SDL3 owning the window.
3. **Motif (libXm/libMrm) and libXt-compat under MinGW** (autotools in an
   MSYS2 environment, or a CMake/Make rewrite of the build).
4. **Open Inventor**: `libimage`, `libFL` (FreeType and the font directory),
   `libInventor` and `libSoXt`, then the Mentor examples. Validate with the
   same `check-open-inventor` link audit (resolving DLLs instead of `ldd`) and
   headless renders.
