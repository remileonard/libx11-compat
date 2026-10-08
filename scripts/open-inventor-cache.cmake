# CMake initial cache (cmake -C) for building Open Inventor against the libx11-compat stack
# (mk/open-inventor.mk). Every X11, Motif and OpenGL result Open Inventor's
# find_package() calls would otherwise search the host for is pinned here to
# the in-tree build, so the host libX11/libXt/libXi/libGL (and any system
# Motif) can never end up on the link line next to the compat libraries:
#
#   X11, Xt, Xi  -> libX11-compat / libXt-compat / libXi-compat
#   Motif (Xm)   -> the in-tree Motif build (mk/motif.mk)
#   OpenGL       -> GLVND libOpenGL (gl* only) + libx11-compat (glX*), the
#                   direct desktop-GL path; no libGL.so, no libGLX, no gl4es
#   GLU          -> in-tree mesa/glu linked against libOpenGL
#
# Headers come from a merged sysroot (X11/, Xm/, GL/) the makefile stages.
# libjpeg, freetype and iconv are not part of the X stack and still resolve
# from the host. Inputs, passed with -D: LIBX11_COMPAT_LIBDIR (the build/
# directory), LIBX11_COMPAT_SYSROOT (staged include root), LIBX11_COMPAT_GLU
# (libGLU.so), LIBX11_COMPAT_OPENGL (libOpenGL.so); they must precede -C on
# the cmake command line so they are defined when this script runs.
#
# Windows (LIBX11_COMPAT_WINDOWS=ON, a MinGW-w64 cross build): the compat
# libraries are linked through their import libraries (.dll.a), Motif is
# libXm.dll, LIBX11_COMPAT_OPENGL / _GLU are the system opengl32 / glu32,
# and the find modules in scripts/open-inventor-cmake-windows/ replace CMake's
# UNIX-only FindX11/FindMotif. LIBX11_COMPAT_MOTIF names the Motif library
# and LIBX11_COMPAT_ICONV the POSIX shim archive that provides iconv.

foreach(var LIBX11_COMPAT_LIBDIR LIBX11_COMPAT_SYSROOT LIBX11_COMPAT_GLU
            LIBX11_COMPAT_OPENGL)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "open-inventor-cache: ${var} is not set")
    endif()
endforeach()

set(_lib "${LIBX11_COMPAT_LIBDIR}")
set(_inc "${LIBX11_COMPAT_SYSROOT}")
set(_so ".so")
set(_motif "${_lib}/libXm.so")
if(LIBX11_COMPAT_WINDOWS)
    set(_so ".dll.a")
    set(_motif "${LIBX11_COMPAT_MOTIF}")
    get_filename_component(_modules
        "${CMAKE_CURRENT_LIST_DIR}/open-inventor-cmake-windows" ABSOLUTE)
    set(CMAKE_MODULE_PATH "${_modules}" CACHE PATH "" FORCE)
    # iconv comes from the POSIX shims (compat/win32/iconv.c).
    set(Iconv_IS_BUILT_IN FALSE CACHE BOOL "" FORCE)
    set(Iconv_INCLUDE_DIR "${LIBX11_COMPAT_ICONV_INCLUDE}" CACHE PATH "" FORCE)
    set(Iconv_LIBRARY "${LIBX11_COMPAT_ICONV}" CACHE FILEPATH "" FORCE)
    # CMake's MinGW defaults, plus Winsock for select() (SoSensorManager).
    set(_winlibs "-lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32")
    string(APPEND _winlibs " -loleaut32 -luuid -lcomdlg32 -ladvapi32 -lws2_32")
    set(CMAKE_C_STANDARD_LIBRARIES "${_winlibs}" CACHE STRING "" FORCE)
    set(CMAKE_CXX_STANDARD_LIBRARIES "${_winlibs}" CACHE STRING "" FORCE)
endif()

# FindX11: core, Xt and Xi are the X11:: targets Open Inventor links.
set(X11_INCLUDE_DIR "${_inc}" CACHE PATH "" FORCE)
set(X11_X11_INCLUDE_PATH "${_inc}" CACHE PATH "" FORCE)
set(X11_X11_LIB "${_lib}/libX11-compat${_so}" CACHE FILEPATH "" FORCE)
set(X11_Xt_INCLUDE_PATH "${_inc}" CACHE PATH "" FORCE)
set(X11_Xt_LIB "${_lib}/libXt-compat${_so}" CACHE FILEPATH "" FORCE)
set(X11_Xi_INCLUDE_PATH "${_inc}" CACHE PATH "" FORCE)
set(X11_Xi_LIB "${_lib}/libXi-compat${_so}" CACHE FILEPATH "" FORCE)
# Not linked by Open Inventor, but FindX11 folds them into X11_LIBRARIES and its
# own probes; pin them too so no host X library is ever picked up.
set(X11_Xext_INCLUDE_PATH "${_inc}" CACHE PATH "" FORCE)
set(X11_Xext_LIB "${_lib}/libXext-compat${_so}" CACHE FILEPATH "" FORCE)
set(X11_ICE_INCLUDE_PATH "${_inc}" CACHE PATH "" FORCE)
set(X11_ICE_LIB "${_lib}/libICE-compat${_so}" CACHE FILEPATH "" FORCE)
set(X11_SM_INCLUDE_PATH "${_inc}" CACHE PATH "" FORCE)
set(X11_SM_LIB "${_lib}/libSM-compat${_so}" CACHE FILEPATH "" FORCE)

# FindMotif.
set(MOTIF_INCLUDE_DIR "${_inc}" CACHE PATH "" FORCE)
set(MOTIF_LIBRARIES "${_motif}" CACHE FILEPATH "" FORCE)

# FindOpenGL (GLVND layout): OpenGL::GL = OpenGL::OpenGL + OpenGL::GLX.
set(OpenGL_GL_PREFERENCE GLVND CACHE STRING "" FORCE)
set(OPENGL_INCLUDE_DIR "${_inc}" CACHE PATH "" FORCE)
set(OPENGL_GLX_INCLUDE_DIR "${_inc}" CACHE PATH "" FORCE)
set(OPENGL_opengl_LIBRARY "${LIBX11_COMPAT_OPENGL}" CACHE FILEPATH "" FORCE)
set(OPENGL_glx_LIBRARY "${_lib}/libX11-compat${_so}" CACHE FILEPATH "" FORCE)
if(LIBX11_COMPAT_WINDOWS)
    # opengl32; scripts/open-inventor-cmake-windows/FindOpenGL.cmake adds glX*.
    set(OPENGL_gl_LIBRARY "${LIBX11_COMPAT_OPENGL}" CACHE FILEPATH "" FORCE)
else()
    set(OPENGL_gl_LIBRARY "" CACHE FILEPATH "" FORCE)
endif()
set(OPENGL_glu_LIBRARY "${LIBX11_COMPAT_GLU}" CACHE FILEPATH "" FORCE)
set(OPENGL_GLU_INCLUDE_DIR "${_inc}" CACHE PATH "" FORCE)

# Runtime: resolve the compat libraries from the build tree.
get_filename_component(_glu_dir "${LIBX11_COMPAT_GLU}" DIRECTORY)
set(CMAKE_BUILD_RPATH "${_lib};${_glu_dir}" CACHE STRING "" FORCE)
