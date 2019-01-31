
set(PHYSFS_ARCHIVE_ZIP TRUE)
set(PHYSFS_BUILD_STATIC TRUE)
set(PHYSFS_BUILD_SHARED FALSE)
set(PHYSFS_BUILD_TEST FALSE)
set(PHYSFS_TARGETNAME_UNINSTALL uninstall_physfs)
set(PHYSFS_TARGETNAME_DIST dist_physfs)

add_subdirectory(libsrc/physfs-3.0.2 EXCLUDE_FROM_ALL)

add_library(physfs-interface INTERFACE)
target_include_directories(physfs-interface INTERFACE libsrc/physfs-3.0.2/src)
target_link_libraries(physfs-interface INTERFACE physfs-static)

add_library(PhysicsFS::PhysicsFS ALIAS physfs-interface)
