# FindMotif for the Windows (MinGW-w64) build of Open Inventor: CMake's own
# module only runs on UNIX. MOTIF_INCLUDE_DIR and MOTIF_LIBRARIES are pinned
# by scripts/open-inventor-cache.cmake (the in-tree libXm.dll).

if(MOTIF_INCLUDE_DIR AND MOTIF_LIBRARIES)
    set(MOTIF_FOUND TRUE)
    set(Motif_FOUND TRUE)
endif()
