Open Inventor for Windows (32-bit) on libx11-compat
===================================================

SGI Open Inventor 2.1 with its Motif viewers (SoXt), built from the
unmodified UNIX sources and running natively on Windows: Xlib, Xt and Motif
come from libx11-compat (an in-process Xlib on top of SDL3), OpenGL from the
system opengl32.dll. No X server and no Windows port of the toolkit
(SoWin) are involved.

Programs
  The 66 examples of "The Inventor Mentor", e.g.
  02.1.HelloCone.exe     a red cone in a render area
  02.4.Examiner.exe      the same cone in the examiner viewer (drag to rotate)
  06.2.Simple3DText.exe  3D text
  07.1.BasicTexture.exe  a texture-mapped cube
  13.7.Rotor.exe         an animated windmill
  15.1.ConeRadius.exe    a dragger driving the scene

Run any .exe from this folder (double-click, or from cmd to see console
output). Keep the folder layout: the programs load their DLLs from here,
read example models and textures from data\ and fonts from
share\inventor\fonts\ relative to the current directory.

Some examples are console programs, need command-line arguments, or need
overlay planes / color-index visuals, which are not available.

These are 32-bit programs on purpose: the original sources keep pointers in
`long`, which stays 32 bits on 64-bit Windows; 32-bit Windows matches the
UNIX systems the code was written for.

Fonts: Liberation and DejaVu, under their own licenses
(share\inventor\fonts\LICENSE-*).

Build from source (on Linux, with the 32-bit MinGW-w64 cross compiler):
  make WINDOWS=1 WINDOWS_ARCH=i686 open-inventor-dist

-----------------------------------------------------------------------------
Version française

Open Inventor 2.1 de SGI, avec ses visualiseurs Motif (SoXt), compilé à
partir des sources UNIX non modifiées et exécuté nativement sous Windows :
Xlib, Xt et Motif viennent de libx11-compat (une Xlib en processus au-dessus
de SDL3), OpenGL du opengl32.dll du système. Ni serveur X, ni portage
Windows du toolkit (SoWin).

Lancez n'importe quel .exe depuis ce dossier (double-clic, ou depuis cmd pour
voir la sortie console). Gardez l'arborescence : les programmes chargent
leurs DLL depuis ce dossier, lisent les modèles et textures dans data\ et
les polices dans share\inventor\fonts\, relativement au dossier courant.

Ce sont volontairement des programmes 32 bits : le code d'origine range des
pointeurs dans des `long`, qui restent sur 32 bits en Windows 64 bits ;
Windows 32 bits correspond aux systèmes UNIX pour lesquels il a été écrit.
