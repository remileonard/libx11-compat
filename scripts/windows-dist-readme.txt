libx11-compat for Windows (x86-64) - example programs
=====================================================

Unmodified Xlib programs running natively on Windows through libx11-compat
(an in-process Xlib on top of SDL3). No X server is needed.

Programs
  2048.exe        the 2048 game (arrow keys, Esc quits)
  paint.exe       a small paint program
  life.exe        Conway's Game of Life
  clock.exe       analog clock
  catclock.exe    the classic Kit-Cat clock
  mandel.exe      interactive Mandelbrot viewer
  moire.exe       animated moire patterns
  processing.exe  Processing-style showcase
  clipboard.exe   clipboard probe (SDL clipboard)
  x11perf.exe     the X.Org x11perf benchmark (run from a console)

Run any .exe directly (double-click, or from cmd/PowerShell to see console
output). Keep all the files in the same folder: the programs load
libX11-compat.dll, SDL3.dll, SDL3_ttf.dll, libwinpthread-1.dll and
libgcc_s_seh-1.dll from there.

Not yet included: GLX/OpenGL clients, Xt/Athena/Motif programs and Open
Inventor (see docs/OPEN-INVENTOR.md in the source tree for the roadmap).

Build from source (on Linux, with the MinGW-w64 cross compiler):
  make WINDOWS=1 windows-dist      # -> build/win64/dist/libx11-compat-win64.zip

-----------------------------------------------------------------------------
Version française

Des programmes Xlib non modifiés qui tournent nativement sous Windows grâce à
libx11-compat (une Xlib en processus au-dessus de SDL3), sans serveur X.

Lancez n'importe quel .exe directement (double-clic, ou depuis cmd/PowerShell
pour voir la sortie console). Gardez tous les fichiers dans le même dossier :
les programmes y chargent libX11-compat.dll, SDL3.dll, SDL3_ttf.dll,
libwinpthread-1.dll et libgcc_s_seh-1.dll.

Pas encore inclus : les clients GLX/OpenGL, les programmes Xt/Athena/Motif et
Open Inventor (voir la feuille de route dans docs/OPEN-INVENTOR.md).
