if not exist _cmake_vs2019_win64 mkdir _cmake_vs2019_win64
cd _cmake_vs2019_win64
cmake -G "Visual Studio 16 2019" -A x64 .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cd ..