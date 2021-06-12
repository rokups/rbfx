#!/usr/bin/env bash

# build.sh <action> ...
# ci_action:       dependencies|generate|build|install
# ci_platform:     windows|linux|macos|android|ios|web
# ci_arch:         x86|x64|arm|arm64
# ci_compiler:     msvc|gcc|gcc-*|clang|clang-*|mingw
# ci_build_type:   dbg|rel
# ci_lib_type:     lib|dll
# ci_source_dir:   source code directory
# ci_build_dir:    cmake cache directory
# ci_sdk_dir:      sdk installation directory

ci_action=$1; shift;
ci_cmake_params_user="$@"

# default values
ci_compiler=${ci_compiler:-"default"}
ci_build_type=${ci_build_type:-"rel"}
ci_lib_type=${ci_lib_type:-"dll"}

# fix paths on windows by replacing \ with /.
ci_source_dir=$(echo $ci_source_dir | tr "\\" "/" 2>/dev/null)
ci_build_dir=$(echo $ci_build_dir   | tr "\\" "/" 2>/dev/null)
ci_sdk_dir=$(echo $ci_sdk_dir       | tr "\\" "/" 2>/dev/null)
ci_source_dir=${ci_source_dir%/};   # remove trailing slash if any

echo "ci_action=$ci_action"
echo "ci_platform=$ci_platform"
echo "ci_arch=$ci_arch"
echo "ci_compiler=$ci_compiler"
echo "ci_build_type=$ci_build_type"
echo "ci_lib_type=$ci_lib_type"
echo "ci_source_dir=$ci_source_dir"
echo "ci_build_dir=$ci_build_dir"
echo "ci_sdk_dir=$ci_sdk_dir"

declare -A types=(
    [dbg]='Debug'
    [rel]='Release'
)

declare -A android_types=(
    [dbg]='assembleDebug'
    [rel]='assembleRelease'
)

generators_windows_mingw=('-G' 'MinGW Makefiles')
generators_windows=('-G' 'Visual Studio 16 2019')
generators_uwp=('-G' 'Visual Studio 16 2019' '-DCMAKE_SYSTEM_NAME=WindowsStore' '-DCMAKE_SYSTEM_VERSION=10.0')
generators_linux=('-G' 'Ninja')
generators_web=('-G' 'Ninja')
generators_macos=('-G' 'Xcode' '-T' 'buildsystem=1')
generators_ios=('-G' 'Xcode' '-T' 'buildsystem=1')

toolchains_ios=(
    '-DCMAKE_TOOLCHAIN_FILE=cmake/Toolchains/IOS.cmake'
    '-DPLATFORM=SIMULATOR64'
    '-DDEPLOYMENT_TARGET=11'
)
toolchains_web=(
    '-DCMAKE_TOOLCHAIN_FILE=cmake/Toolchains/Emscripten.cmake'
    '-DURHO3D_PROFILING=OFF'
)

lib_types_lib=('-DBUILD_SHARED_LIBS=OFF')
lib_types_dll=('-DBUILD_SHARED_LIBS=ON')

quirks_mingw=(
    '-DURHO3D_PROFILING=OFF'
    '-DURHO3D_CSHARP=OFF'
)
quirks_ios=(
    '-DURHO3D_CSHARP=OFF'
)
quirks_android=(
    '-DURHO3D_CSHARP=OFF'
)
quirks_web=(
    '-DURHO3D_PROFILING=OFF'
    '-DURHO3D_CSHARP=OFF'
)
quirks_dll=('-DURHO3D_CSHARP=ON')
quirks_windows_msvc_x64=('-A' 'x64')
quirks_windows_msvc_x86=('-A' 'Win32')
quirks_uwo_msvc_x64=${quirks_windows_msvc_x64[*]}
quirks_uwo_msvc_x86=${quirks_windows_msvc_x86[*]}
quirks_uwp_msvc_arm=('-A' 'ARM')
quirks_uwp_msvc_arm64=('-A' 'ARM64')
quirks_clang=('-DTRACY_NO_PARALLEL_ALGORITHMS=ON')                  # Includes macos and ios
quirks_macos_x86=('-DCMAKE_OSX_ARCHITECTURES=i386')
quirks_macos_x64=('-DCMAKE_OSX_ARCHITECTURES=x86_64')
quirks_linux_x86=(
    '-DCMAKE_C_FLAGS=-m32'
    '-DCMAKE_CXX_FLAGS=-m32'
)
quirks_linux_x64=(
    '-DCMAKE_C_FLAGS=-m64'
    '-DCMAKE_CXX_FLAGS=-m64'
)

# Find msbuild.exe
MSBUILD=msbuild
if [[ "$ci_platform" == "windows" || "$ci_platform" == "uwp" ]];
then
    MSBUILD=$(vswhere -products '*' -requires Microsoft.Component.MSBuild -property installationPath -latest)
    MSBUILD=$(echo $MSBUILD | tr "\\" "/" 2>/dev/null)    # Fix slashes
    MSBUILD=$(echo $MSBUILD | sed "s/://" 2>/dev/null)    # Remove :
    MSBUILD="/$MSBUILD/MSBuild/Current/Bin/MSBuild.exe"
fi

function action-dependencies() {
    if [[ "$ci_platform" == "linux" ]];
    then
        # Linux dependencies
        dev_packages=(
            libgl1-mesa-dev libxcursor-dev libxi-dev libxinerama-dev libxrandr-dev libxrender-dev libxss-dev
            libasound2-dev libpulse-dev libibus-1.0-dev libdbus-1-dev libreadline6-dev libudev-dev uuid-dev libtbb-dev
            libvulkan-dev
        )

        if [[ "$ci_arch" != "x64" ]];
        then
            dev_packages[${#dev_packages[@]}]="binutils-multiarch"
            dev_packages[${#dev_packages[@]}]="binutils-multiarch-dev"
            dev_packages[${#dev_packages[@]}]="build-essential"
        fi

        # Per-arch compilers
        case "$ci_arch" in
            arm)
                sudo dpkg --add-architecture armhf
                dev_packages=("${dev_packages[@]/%/:armhf}")
                dev_packages[${#dev_packages[@]}]="crossbuild-essential-armhf"
                ;;
            arm64)
                sudo dpkg --add-architecture arm64
                sudo apt-get update
                dev_packages=("${dev_packages[@]/%/:arm64}")
                dev_packages[${#dev_packages[@]}]="crossbuild-essential-arm64"
                ;;
            x86)
                sudo dpkg --add-architecture i386
                dev_packages=("${dev_packages[@]/%/:i386}")
                dev_packages[${#dev_packages[@]}]="crossbuild-essential-i386"
                ;;
        esac

        # Common dependencies
        sudo apt-get update
        sudo apt-get install -y ninja-build ccache xvfb "${dev_packages[@]}"
    elif [[ "$ci_platform" == "web" || "$ci_platform" == "android" ]];
    then
        # Web / android dependencies
        sudo apt-get install -y --no-install-recommends uuid-dev ninja-build ccache
    elif [[ "$ci_platform" == "macos" || "$ci_platform" == "ios" ]];
    then
        # iOS/MacOS dependencies
        brew install pkg-config ccache
    elif [[ "$ci_platform" == "windows" || "$ci_platform" == "uwp" ]];
    then
        # Windows dependencies
        choco install -y ccache vulkan-sdk
        pip install clcache
    fi
}

function action-generate() {
    # Change a default compiler.
    case "$ci_platform-$ci_compiler" in
        linux-clang*)
            export CC=${ci_compiler}            # clang or clang-XX
            export CXX=${ci_compiler}++         # clang++ or clang-XX++
            ;;
        linux-gcc*)
            export CC=${ci_compiler}            # gcc or gcc-XX
            export CXX=${ci_compiler/gcc/g++}   # g++ or g++-XX
            ;;
    esac

    # Generate.
    mkdir $ci_build_dir
    cd $ci_build_dir

    ci_cmake_params=()
    v="generators_${ci_platform}_${ci_compiler}[@]";        ci_cmake_params+=("${!v}")
    if [[ -z "${!v}" ]];
    then
        v="generators_${ci_platform}[@]";                   ci_cmake_params+=("${!v}")
    fi
    v="toolchains_${ci_platform}[@]";                       ci_cmake_params+=("${!v}")
    v="lib_types_${ci_lib_type}[@]";                        ci_cmake_params+=("${!v}")
    v="quirks_${ci_compiler}[@]";                           ci_cmake_params+=("${!v}")
    v="quirks_${ci_lib_type}[@]";                           ci_cmake_params+=("${!v}")
    v="quirks_${ci_platform}_${ci_compiler}_${ci_arch}[@]"; ci_cmake_params+=("${!v}")
    v="quirks_${ci_platform}_${ci_arch}[@]";                ci_cmake_params+=("${!v}")
    v="quirks_${ci_platform}[@]";                           ci_cmake_params+=("${!v}")

    ci_cmake_params+=(
        "-DCMAKE_BUILD_TYPE=${types[$ci_build_type]}"
        "-DCMAKE_INSTALL_PREFIX=$ci_sdk_dir"
    )

    if [[ "$ci_compiler" != "msvc" ]];
    then
        ci_cmake_params+=(
            "-DCMAKE_C_COMPILER_LAUNCHER=ccache"
            "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
        )
    fi

    ci_cmake_params+=(${ci_cmake_params_user[@]})
    ci_cmake_params+=("$ci_source_dir")

    echo "${ci_cmake_params[@]}"
    cmake "${ci_cmake_params[@]}"
}

# Default build path using plain CMake.
function action-build() {
    cd $ci_build_dir
    cmake --build . --config "${types[$ci_build_type]}" && \
    ccache -s
}

# Custom compiler build paths used only on windows.
function action-build-msvc() {
    cd $ci_build_dir
    # Invoke msbuild directly when using msvc. Invoking msbuild through cmake causes some custom target dependencies to not be respected.
    python_path=$(python -c "import os, sys; print(os.path.dirname(sys.executable))")
    "$MSBUILD" "-r" "-p:Configuration=${types[$ci_build_type]}" "-p:TrackFileAccess=false" "-p:CLToolExe=clcache.exe" "-p:CLToolPath=$python_path/Scripts/" *.sln && \
    clcache -s
}

function action-build-mingw() {
    action-build    # Default build using CMake.
}

# Custom platform build paths used only on android.
function action-build-android() {
    cd $ci_source_dir/android
    gradle wrapper                                && \
    ./gradlew "${android_types[$ci_build_type]}"  && \
    ccache --show-stats
}

function action-install() {
    cd $ci_build_dir
    cmake --install . --config "${types[$ci_build_type]}"
}

# Invoke requested action.
action-$ci_action
