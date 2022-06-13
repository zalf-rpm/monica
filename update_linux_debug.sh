mkdir -p _cmake_linux_debug
cd  _cmake_linux_debug
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_CAPNPROTO=ON
cd ..