if not exist _cmake_win64 mkdir _cmake_win64
#set MONICA_DIR=%cd%
cd _cmake_win64
#set CMAKE_PREFIX_PATH=%MONICA_DIR%\..\vcpkg\installed\x64-windows\share\unofficial-sodium
cmake -G "Visual Studio 17 2022" .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static 
cd ..
