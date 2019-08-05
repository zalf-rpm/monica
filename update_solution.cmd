if not exist _cmake_win32 mkdir _cmake_win32
cd  _cmake_win32
cmake -G "Visual Studio 16 2019" -A Win32 .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows-static
cd ..
