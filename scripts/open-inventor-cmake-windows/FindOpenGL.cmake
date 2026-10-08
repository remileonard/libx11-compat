# FindOpenGL for the Windows (MinGW-w64) build of Open Inventor: gl* from the
# system opengl32 (WGL's compatibility contexts), GLU from glu32, and glX*
# from libx11-compat, which runs GLX on WGL (src/egl-wgl.c). OpenGL::GL
# carries libx11-compat so every target that calls glX* links it, as the
# GLVND OpenGL::GLX target does on Linux.

set(OPENGL_FOUND TRUE)
set(OpenGL_FOUND TRUE)
set(OPENGL_GLU_FOUND TRUE)
set(OPENGL_LIBRARIES "${OPENGL_gl_LIBRARY};${OPENGL_glx_LIBRARY}")
if(NOT TARGET OpenGL::GL)
    add_library(OpenGL::GL UNKNOWN IMPORTED)
    set_target_properties(OpenGL::GL PROPERTIES
        IMPORTED_LOCATION "${OPENGL_gl_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENGL_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${OPENGL_glx_LIBRARY}")
endif()
if(NOT TARGET OpenGL::GLU)
    add_library(OpenGL::GLU UNKNOWN IMPORTED)
    set_target_properties(OpenGL::GLU PROPERTIES
        IMPORTED_LOCATION "${OPENGL_glu_LIBRARY}"
        INTERFACE_LINK_LIBRARIES OpenGL::GL)
endif()
