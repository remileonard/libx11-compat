Open Inventor for Windows (32-bit) on libx11-compat
===================================================

SGI Open Inventor 2.1 with its Motif viewers (SoXt), built from the
unmodified UNIX sources and running natively on Windows: Xlib, Xt and Motif
come from libx11-compat (an in-process Xlib on top of SDL3), OpenGL from the
system opengl32.dll. No X server and no Windows port of the toolkit
(SoWin) are involved.

Programs
  Tools and demos (from cmd, in this folder):
  ivview.exe FILE.iv          view a model, e.g.
                              ivview share\inventor\data\models\toys\lavalamp.iv
  SceneViewer.exe [FILE.iv]   the full scene viewer and editor
  gview.exe FILE.iv           scene graph browser, e.g.
                              gview share\inventor\data\demos\windmill.iv
  qmorf.exe FILE.iv FILE.iv   shape morphing, e.g. the three files in
                              share\inventor\data\models\CyberHeads
  maze.exe, drop.exe, noodle.exe, revo.exe, textomatic.exe
                              demos that need no argument
  ivcat, ivinfo, ivfix, ivnorm, ivAddVP, ivperf, ivdowngrade
                              command-line tools for .iv files
  More models: share\inventor\data\models

  The 66 examples of "The Inventor Mentor", e.g.
  02.1.HelloCone.exe     a red cone in a render area
  02.4.Examiner.exe      the same cone in the examiner viewer (drag to rotate)
  06.2.Simple3DText.exe  3D text
  07.1.BasicTexture.exe  a texture-mapped cube
  13.7.Rotor.exe         an animated windmill
  15.1.ConeRadius.exe    a dragger driving the scene

Run any .exe from this folder (double-click, or from cmd after cd-ing into
it, to see console output and pass files). Keep the folder layout: the
programs load their DLLs from here and read models, textures, fonts and help
from data\ and share\inventor\ relative to the current directory.

File names: the programs see Windows paths the UNIX way, as MSYS2 and Cygwin
show them: C:\Users\me\model.iv is /c/Users/me/model.iv, and / lists the
drives. That is what the Motif file dialogs display; their filter also takes a
Windows path (C:\models\*.iv), and so does the command line.

Some examples are console programs, need command-line arguments, or need
overlay planes / color-index visuals, which are not available.

These are 32-bit programs on purpose: the original sources keep pointers in
`long`, which stays 32 bits on 64-bit Windows; 32-bit Windows matches the
UNIX systems the code was written for.

Fonts: Liberation and DejaVu, under their own licenses
(fonts\LICENSE-*, share\inventor\fonts\LICENSE-*). fonts\ holds the X core
fonts (Motif labels, menus, "fixed"); it is looked up next to the .exe, then
C:\Windows\Fonts. Without it Motif reports "FONTLIST_DEFAULT_TAG_STRING ...
Cannot load font".

Build from source (on Linux, with the 32-bit MinGW-w64 cross compiler):
  make WINDOWS=1 WINDOWS_ARCH=i686 open-inventor-dist

-----------------------------------------------------------------------------
Version française

Open Inventor 2.1 de SGI, avec ses visualiseurs Motif (SoXt), compilé à
partir des sources UNIX non modifiées et exécuté nativement sous Windows :
Xlib, Xt et Motif viennent de libx11-compat (une Xlib en processus au-dessus
de SDL3), OpenGL du opengl32.dll du système. Ni serveur X, ni portage
Windows du toolkit (SoWin).

Lancez n'importe quel .exe depuis ce dossier (double-clic, ou depuis cmd
après un cd dans le dossier, pour voir la sortie console et passer des
fichiers). Gardez l'arborescence : les programmes chargent leurs DLL depuis
ce dossier et lisent modèles, textures, polices et aide dans data\ et
share\inventor\, relativement au dossier courant.

Outils et démos : ivview, SceneViewer, gview et qmorf prennent des fichiers
.iv (exemples ci-dessus) ; maze, drop, noodle, revo et textomatic se lancent
sans argument.

Ce sont volontairement des programmes 32 bits : le code d'origine range des
pointeurs dans des `long`, qui restent sur 32 bits en Windows 64 bits ;
Windows 32 bits correspond aux systèmes UNIX pour lesquels il a été écrit.
