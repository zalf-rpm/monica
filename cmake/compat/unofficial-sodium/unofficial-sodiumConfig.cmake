# Stand-in for vcpkg's unofficial-sodium CMake package, for builds that take
# libsodium from elsewhere (the Nix flake). mas_cpp_misc/common/CMakeLists.txt
# asks for `find_package(unofficial-sodium CONFIG)` and links
# `unofficial-sodium::sodium` - names only vcpkg's port provides. Upstream
# libsodium ships just a pkg-config file, so this wraps that in the same target.
#
# Found by adding this directory's parent (cmake/compat) to CMAKE_PREFIX_PATH,
# which flake.nix does. vcpkg builds never see it.

include(CMakeFindDependencyMacro)
find_dependency(PkgConfig)
pkg_check_modules(UNOFFICIAL_SODIUM REQUIRED IMPORTED_TARGET libsodium)

if(NOT TARGET unofficial-sodium::sodium)
  add_library(unofficial-sodium::sodium INTERFACE IMPORTED)
  target_link_libraries(unofficial-sodium::sodium INTERFACE PkgConfig::UNOFFICIAL_SODIUM)
endif()

set(unofficial-sodium_FOUND TRUE)
