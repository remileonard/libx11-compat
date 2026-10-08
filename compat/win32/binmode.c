/*
 * Link into a Windows program to give it UNIX file semantics: the C runtime
 * then opens files and the standard streams in binary mode by default, with no
 * CR/LF translation and no stop at ^Z. Text mode corrupts the binary files
 * old UNIX code reads with plain fopen(path, "r") (Open Inventor's binary .iv
 * models).
 *
 * MinGW's own binmode.o is meant to do this, but some MinGW-w64 releases ship
 * it empty. The executable's _fmode is what the startup code hands to the C
 * runtime, so this object must be linked into the .exe itself, not a DLL.
 * It deliberately avoids <stdlib.h>, which turns _fmode into a macro.
 */
int _fmode = 0x8000; /* _O_BINARY */
