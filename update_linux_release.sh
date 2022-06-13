mkdir -p _cmake_linux_release
cd  _cmake_linux_release
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DWITH_CAPNPROTO=ON
cd ..