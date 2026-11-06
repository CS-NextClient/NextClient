vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO wolfpld/tracy
    REF "v${VERSION}"
    SHA512 53912d7563e595812b37bc55fd40508cfd8e5c42d48d957a73b6b7d18bf1287b3f795c10c9a986bf7b906d5b5bebe13b02216e563e794d0a82b2783e8ce5510b
    PATCHES query-lifecycle.patch manual-lifetime.patch queue-memory.patch cpu-zone-recovery.patch
)

set(VCPKG_LIBRARY_LINKAGE dynamic)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DTRACY_STATIC=OFF
        -DTRACY_LTO=OFF
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
        -DTRACY_ENABLE=ON
        -DTRACY_ON_DEMAND=ON
        -DTRACY_NO_CRASH_HANDLER=ON
        -DTRACY_MANUAL_LIFETIME=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_Git=ON
)
vcpkg_cmake_install()
file(INSTALL
    "${SOURCE_PATH}/public/client/TracyQueueMemory.hpp"
    "${SOURCE_PATH}/public/client/TracyQueueVector.hpp"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include/tracy/client")
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME Tracy CONFIG_PATH "lib/cmake/Tracy")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
