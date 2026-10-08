# FindX11 for the Windows (MinGW-w64) build of Open Inventor against
# libx11-compat (mk/open-inventor.mk). CMake's own FindX11 only runs on UNIX;
# this one turns the paths scripts/open-inventor-cache.cmake pins into the
# X11:: targets Open Inventor links.

set(X11_FOUND TRUE)
set(X11_INCLUDE_DIR "${X11_X11_INCLUDE_PATH}")
set(X11_LIBRARIES "${X11_X11_LIB}")
foreach(_part X11 Xt Xi Xext ICE SM)
    if(_part STREQUAL "X11")
        set(_target X11::X11)
    else()
        set(_target X11::${_part})
    endif()
    if(X11_${_part}_LIB AND NOT TARGET ${_target})
        add_library(${_target} UNKNOWN IMPORTED)
        set_target_properties(${_target} PROPERTIES
            IMPORTED_LOCATION "${X11_${_part}_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${X11_X11_INCLUDE_PATH}")
        set(X11_${_part}_FOUND TRUE)
    endif()
endforeach()
if(TARGET X11::Xt)
    set_property(TARGET X11::Xt APPEND PROPERTY INTERFACE_LINK_LIBRARIES X11::X11)
endif()
if(TARGET X11::Xi)
    set_property(TARGET X11::Xi APPEND PROPERTY INTERFACE_LINK_LIBRARIES X11::X11)
endif()
