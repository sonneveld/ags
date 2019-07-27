
set(PHYSFS_ARCHIVE_ZIP TRUE CACHE BOOL "need zip")
set(PHYSFS_BUILD_STATIC TRUE CACHE BOOL "static")
set(PHYSFS_BUILD_SHARED FALSE CACHE BOOL "no shared")
set(PHYSFS_BUILD_TEST FALSE CACHE BOOL "no tests")
set(PHYSFS_TARGETNAME_UNINSTALL uninstall_physfs CACHE STRING "avoid conflicts")
set(PHYSFS_TARGETNAME_DIST dist_physfs CACHE STRING "avoid conflicts")

add_subdirectory(libsrc/physfs-3.0.2 EXCLUDE_FROM_ALL)

add_library(physfs-interface INTERFACE)
target_include_directories(physfs-interface INTERFACE libsrc/physfs-3.0.2/src)
target_link_libraries(physfs-interface INTERFACE physfs-static)

add_library(PhysicsFS::PhysicsFS ALIAS physfs-interface)
