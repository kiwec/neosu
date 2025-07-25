AC_PREREQ([2.69])
AC_INIT([neosu], [[39.00]], [], [], [https://github.com/kiwec/neosu]) # don't report bugs to McKay :)
AC_CONFIG_SRCDIR([src/Engine/Engine.cpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([src/config.h])
AC_PREFIX_DEFAULT([dist]) # default `make install`s to ./dist/bin/ (modify with --prefix=)

AS_IF([test "$CI_DEVBUILD" = "1"], [
    AC_DEFINE([CI_DEVBUILD], [1], [Defined if this is an in-between-releases build.])
])

AC_DEFINE_UNQUOTED([NEOSU_VERSION], [PACKAGE_VERSION], [Defined to the version of neosu being built.])

_tstamp="\""$(TZ=UTC date +%y%m%d%H%M)"\""
AC_DEFINE_UNQUOTED([BUILD_TIMESTAMP], [$_tstamp], [Defined to the UTC YYMMDDHHMM of the last run of ./configure. Used for differentiating dev builds.])

# shut up wine (i need to find what part of configure is doing the "checking if we are cross compiling" check and remove it)
export DISPLAY= WAYLAND_DISPLAY= WINEDLLOVERRIDES="mscoree=;mshtml=;winex11.drv=;winewayland.drv=" WINEDEBUG=-all

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_USE_SYSTEM_EXTENSIONS

AC_DEFUN([AX_COUNT_CPUS], [
    AC_CHECK_PROG([GETCONF], [getconf], [getconf])
    AS_IF([test -n "$GETCONF"],
        [n_cpus=$($GETCONF _NPROCESSORS_ONLN 2>/dev/null)],
        [n_cpus=4])
    AS_IF([test -z "$n_cpus" -o "$n_cpus" -le "0"], [n_cpus=4])
    AC_SUBST([CPU_COUNT], [$n_cpus])
])
AX_COUNT_CPUS

export MAKEFLAGS="-j$CPU_COUNT"
AC_SUBST([MAKEFLAGS])

# no-define needed to avoid setting e.g. VERSION which conflicts with McEngine stuff
# dist-xz for `make dist` tarball target
AM_INIT_AUTOMAKE([foreign subdir-objects no-define dist-xz parallel-tests])
AM_SILENT_RULES([yes])

AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_GREP
AC_PROG_SED

AC_DEFUN([AC_PROG_WGET], [
    AC_CHECK_PROG([WGET], [wget], [wget])
    if test -z "$WGET"; then
        AC_MSG_ERROR([wget not found - required for downloading dependencies])
    fi
])
AC_DEFUN([AC_PROG_GIT], [
    AC_CHECK_PROG([GIT], [git], [git])
    if test -z "$GIT"; then
        AC_MSG_ERROR([git not found - required for downloading dependencies])
    fi
])
AC_DEFUN([AC_PROG_TAR], [
    AC_CHECK_PROG([TAR], [tar], [tar])
    if test -z "$TAR"; then
        AC_MSG_ERROR([tar not found - required for extracting dependencies])
    fi
])
AC_DEFUN([AC_PROG_PKGCONF], [
    AC_CHECK_PROG([PKGCONF], [pkgconf], [pkgconf])
    if test -z "$PKGCONF"; then
        AC_MSG_ERROR([pkgconf not found - required for finding include/library paths])
    fi
])
AC_DEFUN([AC_PROG_CMAKE], [
    AC_CHECK_PROG([CMAKE], [cmake], [cmake])
    if test -z "$CMAKE"; then
        AC_MSG_ERROR([cmake not found - required for building dependencies])
    fi
])
AC_DEFUN([AC_PROG_MESON], [
    AC_CHECK_PROG([MESON], [meson], [meson])
    if test -z "$MESON"; then
        AC_MSG_ERROR([meson not found - required for building dependencies])
    fi
])
AC_DEFUN([AC_PROG_NINJA], [
    AC_CHECK_PROG([NINJA], [ninja], [ninja])
    if test -z "$NINJA"; then
        AC_MSG_ERROR([ninja not found - required for building dependencies])
    fi
])
AC_DEFUN([AC_PROG_UNZIP], [
    AC_CHECK_PROG([UNZIP], [unzip], [unzip])
    if test -z "$UNZIP"; then
        AC_MSG_ERROR([unzip not found - required for extracting dependencies])
    fi
])
AC_DEFUN([AC_PROG_PATCHELF], [
    AC_CHECK_PROG([PATCHELF], [patchelf], [patchelf])
    if test -z "$PATCHELF"; then
        AC_MSG_ERROR([patchelf not found - required for fixing up dependencies])
    fi
])
# needed for building libjpeg (if building it is required)
AC_DEFUN([AC_PROG_NASM], [
    AC_CHECK_PROG([NASM], [nasm], [nasm])
    if test -z "$NASM"; then
        AC_MSG_ERROR([nasm not found - required for building libjpeg])
    fi
])

# needed for dep resolver
AC_PROG_WGET
#AC_PROG_GIT
AC_PROG_TAR
AC_PROG_PKGCONF
AC_PROG_NINJA
AC_PROG_UNZIP
AC_PROG_MESON
AC_PROG_CMAKE

AC_CHECK_PROG([BEAR], [bear], [bear])
AC_SUBST([BEAR])

AC_CHECK_PROG([WINE], [wine], [wine]) # this feels stupid, but i wanna make a shortcut from outside a windows environment to add "-sound xyz" for non-default audio subsystems
AC_SUBST([WINE])

AC_ARG_ENABLE([optimize],
    AS_HELP_STRING([--enable-optimize], [Optimize for speed (default: yes)]))
AC_ARG_ENABLE([native],
    AS_HELP_STRING([--enable-native], [Compile for -march=native (default: no)])) # currently broken
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debug build (default: no)]))
AC_ARG_ENABLE([strip],
    AS_HELP_STRING([--enable-strip], [Strip the final binary (default: no for debug, yes otherwise)]))
AC_ARG_ENABLE([asan],
    AS_HELP_STRING([--enable-asan], [Enable ASan/UBSan (turns debug on as well) (default: no)]))
AC_ARG_ENABLE([tsan],
    AS_HELP_STRING([--enable-tsan], [Enable TSan/UBSan (turns debug on as well) (default: no)]))
AC_ARG_ENABLE([clang],
    AS_HELP_STRING([--enable-clang], [Use clang/clang++ instead of gcc/g++ (default: no)]))
AC_ARG_ENABLE([omp],
    AS_HELP_STRING([--enable-omp], [Use and link with OpenMP for faster parallel processing (default: no)]))
AC_ARG_ENABLE([ime],
    AS_HELP_STRING([--enable-ime], [Enable (SDL) IME support on Windows/Linux Wayland (default: yes)]))
AC_ARG_ENABLE([lto],
    AS_HELP_STRING([--enable-lto], [Enable Link Time Optimization (default: auto, use if functional)]))
AC_ARG_ENABLE([system-deps],
    AS_HELP_STRING([--enable-system-deps], [Prefer system libraries over locally built ones (not recommended! If you do this, also recommend using --disable-static) (default: no)]))
AC_ARG_ENABLE([static],
    AS_HELP_STRING([--enable-static], [Enable static linking where possible (default: yes, --disable-static implies shared)]))

AC_ARG_WITH([renderer],
    AS_HELP_STRING([--with-renderer={opengl,opengles,dx11}], [The (single) renderer backend to build with (default: opengl)]))

AC_ARG_WITH([audio],
    AS_HELP_STRING([--with-audio={bass,soloud,sdl}], [The (comma separated list of) audio backend(s) to build with (default: bass)]))

AC_ARG_WITH([mimalloc],
    AS_HELP_STRING([--with-mimalloc], [Use mimalloc instead of the default memory allocator (faster) (default: on if not debug and not winx86)]))

# setup defaults
AS_IF([test "x$enable_static" != "xno"], [
    enable_static=yes
    disable_static=no
    enable_shared=no
    disable_shared=yes

    PKGTYPE="--static"
    STATICBUILD=yes
], [
    enable_static=no
    disable_static=yes
    enable_shared=yes
    disable_shared=no
])

# compatibility, disable unless explicitly requested
AS_IF([test "x$enable_native" != "xyes"], [
    enable_native=no
    disable_native=yes

    DISABLE_NATIVE=yes
], [
    enable_native=yes
    disable_native=no
])

AC_SUBST([DISABLE_NATIVE])
AC_SUBST([PKGTYPE])
AC_SUBST([STATICBUILD])

AS_IF([test "x$enable_system_deps" != "xyes"], [
    enable_system_deps=no
    disable_system_deps=yes
], [
    enable_system_deps=yes
    disable_system_deps=no
])

AS_IF([test "x$enable_ime" != "xno"], [
    enable_ime=yes
    disable_ime=no
    AC_DEFINE([MCENGINE_FEATURE_IMESUPPORT], [1], [Defined if an IME may be used.])
], [
    enable_ime=no
    disable_ime=yes

    NOIME="yes"
])

AC_SUBST([NOIME])

AS_IF([test "$host_os" = "mingw32"], [
    NEOSU_CPPFLAGS="$NEOSU_CPPFLAGS -D_UNICODE -DUNICODE"
    NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -D_UNICODE -DUNICODE -municode -fvisibility=hidden"
    NEOSU_LDFLAGS="$NEOSU_LDFLAGS -municode"
    AS_IF([test "x$enable_clang" = "xyes"], [
        LLVMMINGW=1
        AC_SUBST([LLVMMINGW])
        CXXFLAGS="$CXXFLAGS --target=$host_cpu-w64-windows-gnu"
        CFLAGS="$CFLAGS --target=$host_cpu-w64-windows-gnu"
    ])
],[
    AC_PROG_PATCHELF # only need this for linux
])

renderer=opengl
AS_VAR_SET_IF([with_renderer],
    [ac_save_IFS=$IFS
        IFS=' ,'
        set x $with_renderer
        IFS=$ac_save_IFS
        shift
        for rend
        do
            case $rend in
            opengles*) AC_DEFINE([MCENGINE_FEATURE_GLES32], [1], [Defined if the OpenGL ES 3.2 backend is to be used (WASM).]) renderer=opengles32 ;;
            dx11) AC_DEFINE([MCENGINE_FEATURE_DIRECTX11], [1], [Defined if the DX11 backend is to be used.]) renderer=dx11 ;;
            esac
        done])

AS_IF([test "$renderer" = "opengl"], [
    AC_DEFINE([MCENGINE_FEATURE_OPENGL], [1], [Defined if the OpenGLLegacy backend is to be used.])
])

AUDBACKENDLIST=""

with_bass=unset
AS_VAR_SET_IF([with_audio],
    [ac_save_IFS=$IFS
        IFS=' ,'
        set x $with_audio
        IFS=$ac_save_IFS
        shift
        for aud
        do
            case $aud in
            soloud*) with_soloud=yes ;;
            bass*) with_bass=yes ;;
            esac
        done])

AS_IF([test "$with_bass" = "yes" || { test "$with_bass" = "unset" && ! { test "$with_soloud" = "yes"; } ; } ], [
    AUDBACKENDLIST="${AUDBACKENDLIST} BASS"
    with_bass=yes
    AC_DEFINE([MCENGINE_FEATURE_BASS], [1], [Define if the BASS audio backend is desired.])
], [
    with_bass=no
])
AS_IF([test "$with_soloud" = "yes"], [
    AS_IF([test "$with_bass" = "yes"], [
        make_audio_shortcuts=yes
    ])
    AUDBACKENDLIST="${AUDBACKENDLIST} SoLoud"
    AC_DEFINE([MCENGINE_FEATURE_SOLOUD], [1], [Define if the SoLoud audio backend is desired.])
])

AC_SUBST([AUDBACKENDLIST])

AS_IF([test "x$enable_asan" = "xyes" || test "x$enable_tsan" = "xyes"], [
    AS_IF([test "x$enable_asan" = "xyes"], [
        CXXFLAGS="$CXXFLAGS -fsanitize=address,undefined"
        CFLAGS="$CFLAGS -fsanitize=address,undefined"
    ], [
        CXXFLAGS="$CXXFLAGS -fsanitize=undefined,thread"
        CFLAGS="$CFLAGS -fsanitize=undefined,thread"
    ])

    AS_IF([test -z "$LLVMMINGW" && test -n "$enable_clang"],[
        LDFLAGS="$LDFLAGS"
        CFLAGS="$CFLAGS -fsanitize-address-use-after-scope -fsanitize-address-use-after-return=always"
        CXXFLAGS="$CXXFLAGS -fsanitize-address-use-after-scope -fsanitize-address-use-after-return=always"
    ])
    enable_debug="yes"
    ENABLE_ASAN=$enable_asan
    AC_SUBST([ENABLE_ASAN])
])

ac_cv_env_PKG_CONFIG_value="$PKG_CONFIG"
ac_cv_env_PKG_CONFIG_set="set"

PKG_CONFIG_PATH="$PWD/build/deps/lib/pkgconfig:$PWD/build/deps/lib64/pkgconfig"
PKG_CONFIG_PATH_CUSTOM="$PKG_CONFIG_PATH"
AC_SUBST([PKG_CONFIG_PATH])
AC_SUBST([PKG_CONFIG_PATH_CUSTOM])

AS_IF([test "x$enable_clang" = "xyes"], [
    CC=clang
    CXX=clang++
    CXXFLAGS="$CXXFLAGS -Qunused-arguments -fuse-ld=lld -Wno-c2x-extensions"
    CFLAGS="$CFLAGS -Qunused-arguments -fuse-ld=lld"
    LDFLAGS="$LDFLAGS -fuse-ld=lld"
    CLANG=yes
    AC_SUBST([CLANG])
],[
    CC=gcc
    CXX=g++
    NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -fimplicit-constexpr"
])

AS_IF([test "$host_cpu" = "x86_64" && test -z "$CLANG"], [
    # this is only possible on x86_64, and not clang
    # faster tls access
    CFLAGS="$CFLAGS -mtls-dialect=gnu2"
    CXXFLAGS="$CXXFLAGS -mtls-dialect=gnu2"
])

AS_IF([[case "$host_cpu" in
  i[[3456789]]86*) host_cpu=i686 ;;
esac]])

AS_IF([test "$host_os" = "linux-gnu"], [
    AS_IF([test "$host_cpu" = "i686"], [
        install -Dm755 /dev/stdin $PWD/build/$target-$CC <<EOF
#!/usr/bin/env sh
$CC -m32 "\$@"
EOF
        install -Dm755 /dev/stdin $PWD/build/$target-$CXX <<EOF
#!/usr/bin/env sh
$CXX -m32 "\$@"
EOF
        AS_IF([ "x$enable_clang" = "xyes" ], [
            NEOSU_LDFLAGS="$NEOSU_LDFLAGS -latomic"
        ],[
            # ld complains about libcrypto.a(libcrypto-lib-cast-586.o): warning: relocation against `CAST_S_table1' in read-only section `.text'
            NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -fno-PIE -no-pie"
            NEOSU_LDFLAGS="$NEOSU_LDFLAGS -no-pie"
        ])
        AC_SUBST([NASMOBJ],[elf32])
    ],[
        install -Dm755 /dev/stdin $PWD/build/$target-$CC <<EOF
#!/usr/bin/env sh
$CC "\$@"
EOF
        install -Dm755 /dev/stdin $PWD/build/$target-$CXX <<EOF
#!/usr/bin/env sh
$CXX "\$@"
EOF
        AC_SUBST([NASMOBJ],[elf64])
    ])

    install -Dm755 /dev/stdin $PWD/build/pkg-config <<__EOF__
#!/usr/bin/env sh
PKG_CONFIG_PATH="$PWD/build/deps/lib/pkgconfig:$PWD/build/deps/lib64/pkgconfig" $PKGCONF --personality=$host_cpu-pc-linux-gnu "$PKGTYPE" "\$@"
__EOF__

    CC="$PWD/build/$target-$CC"
    CXX="$PWD/build/$target-$CXX"
    ac_tool_prefix=
    CMAKE_SYSTEM_NAME="Linux"
    SYSNAME="linux"
    DLLEXT=".so"
    LIBPREFIX="lib"

    AC_DEFINE([MCENGINE_PLATFORM_LINUX], [1], [Define if compiling for Linux.])
],[
    # TODO: don't hardcode mingw paths here
    AS_IF([test "$build_os" = "linux-gnu"], [
        AC_SUBST([PKG_CONFIG_SYSTEM_LIBRARY_PATH],["/usr/$host_cpu-w64-mingw32/lib"])
        AC_SUBST([PKG_CONFIG_SYSTEM_INCLUDE_PATH],["/usr/$host_cpu-w64-mingw32/include"])
        install -Dm755 /dev/stdin $PWD/build/pkg-config <<__EOF__
#!/usr/bin/env sh
PKG_CONFIG_LIBDIR="/usr/$host_cpu-w64-mingw32/lib/pkgconfig" PKG_CONFIG_SYSTEM_INCLUDE_PATH="/usr/$host_cpu-w64-mingw32/include" \
PKG_CONFIG_SYSTEM_LIBRARY_PATH="/usr/$host_cpu-w64-mingw32/lib" PKG_CONFIG_PATH="$PWD/build/deps/lib/pkgconfig:$PWD/build/deps/lib64/pkgconfig" \
$PKGCONF --personality=$host_cpu-w64-mingw32 $PKGTYPE "\$@"
__EOF__
    ], [
        # WIP: building in msys2
        install -Dm755 /dev/stdin $PWD/build/pkg-config <<__EOF__
#!/usr/bin/env sh
PKG_CONFIG_PATH="$PWD/build/deps/lib/pkgconfig:$PWD/build/deps/lib64/pkgconfig" \
$host_cpu-w64-mingw32-pkg-config $PKGTYPE "\$@"
__EOF__
    ])

    AC_CHECK_TOOL([WINDRES], [windres], [windres])

    CMAKE_SYSTEM_NAME="Windows"
    CMAKERCOPT="-DCMAKE_RC_COMPILER=$WINDRES"
    SYSNAME="windows"
    DLLEXT=".dll"
    LIBPREFIX=""

    AS_IF([test "$host_cpu" = "i686"], [ # uber compatibility configuration, works on vista (winxp should work, but curl's openssl depends on vista+, can be fixed in the future)
        d3dcompiler_lib="-ld3dcompiler_43"
        ubercompat=on
        with_mimalloc="no"
        LDFLAGS="$LDFLAGS -m32"
        WINPTHREAD_LDFLAGS="-lwinpthread"
        # vista (winxp should work, but curl's openssl depends on vista+, can be fixed in the future)
        CPPFLAGS="$CPPFLAGS -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -D_WIN32_WINDOWS=0x0600 -D_WIN32_IE=0x0600"
        CFLAGS="$CFLAGS -m32 -fno-omit-frame-pointer -momit-leaf-frame-pointer -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -D_WIN32_WINDOWS=0x0600 -D_WIN32_IE=0x0600 -mfpmath=sse"
        CXXFLAGS="$CXXFLAGS -m32 -fno-omit-frame-pointer -momit-leaf-frame-pointer -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -D_WIN32_WINDOWS=0x0600 -D_WIN32_IE=0x0600 -mfpmath=sse"
        AC_SUBST([NASMOBJ],[win32])
    ], [
        d3dcompiler_lib="-ld3dcompiler_47"
        # win10
        CPPFLAGS="$CPPFLAGS -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_WINDOWS=0x0A00 -D_WIN32_IE=0x0A00"
        CFLAGS="$CFLAGS -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_WINDOWS=0x0A00 -D_WIN32_IE=0x0A00"
        CXXFLAGS="$CXXFLAGS -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_WINDOWS=0x0A00 -D_WIN32_IE=0x0A00"
        #WINPTHREAD_LDFLAGS="-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic" # figure out how to make this not overwrite the neosu resources
        WINPTHREAD_LDFLAGS="-lwinpthread"
        AC_SUBST([NASMOBJ],[win64])
    ])

    AC_DEFINE([MCENGINE_PLATFORM_WINDOWS], [1], [Define if compiling for Windows.])
])

AC_SUBST([WINPTHREAD_LDFLAGS])

PKG_CONFIG="$PWD/build/pkg-config"
AC_SUBST([PKG_CONFIG])
AC_SUBST([CMAKE_SYSTEM_NAME])
AC_SUBST([CMAKERCOPT])
AC_SUBST([SYSNAME])
AC_SUBST([DLLEXT])
AC_SUBST([LIBPREFIX])

AC_PROG_CC([$CC])
AS_IF([[case "$CC" in
  *clang) CXX=$CC++ ;;
  *gcc) CXX=${CC%gcc}g++ ;;
esac]])
AC_PROG_CXX([$CXX])

AC_CHECK_TOOL([STRIP], [strip], [strip])

AC_CHECK_PROG([CCACHE], [ccache], [ccache])

# we already set $CC and $CXX to absolute paths in the build/$arch-pc-linux-gnu-{$CC,$CXX} wrappers
# for linux, so AC_PATH_PROG doesn't work for that (and isn't needed anyways)
# this is done mostly for mingw targets so that configure --host=x86_64-w64-mingw32 --enable-clang
# saves the full path to the llvm-mingw compiler, in case $PATH changes for future compiles
AS_IF([test "$host_os" = "linux-gnu"],[
    AS_VAR_SET([CC_REAL], [$CC])
    AS_VAR_SET([CXX_REAL], [$CXX])
],[
    AC_PATH_PROG([CC_REAL], [$CC])
    AC_PATH_PROG([CXX_REAL], [$CXX])
])

install -Dm755 /dev/stdin $PWD/build/cc <<EOF
#!/usr/bin/env sh
$CCACHE $CC_REAL "\$@"
EOF
install -Dm755 /dev/stdin $PWD/build/cxx <<EOF
#!/usr/bin/env sh
$CCACHE $CXX_REAL "\$@"
EOF

export CC="$PWD/build/cc"
export CXX="$PWD/build/cxx"

AC_SUBST([CC])
AC_SUBST([CXX])

AS_IF([test "$host_cpu" = "x86_64"], [COMMON_FLAGS="$COMMON_FLAGS -mcmodel=small"])

CPPFLAGS="$CPPFLAGS -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64"
COMMON_FLAGS="$COMMON_FLAGS -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 -ffunction-sections -fdata-sections -fwrapv -fno-strict-aliasing -fno-semantic-interposition -ftls-model=initial-exec"
CXXFLAGS="$CXXFLAGS $COMMON_FLAGS -fno-exceptions" # -fno-rtti
CFLAGS="$CFLAGS $COMMON_FLAGS"
AS_IF([! { test "x$enable_clang" = "xyes" && test "$host_os" = "linux-gnu" ; } && { test -z "$ubercompat" || test "x$enable_static" = "xyes" ; }], [
    LDFLAGS="$LDFLAGS -static-libstdc++ -static-libgcc"
])

AC_LANG_PUSH([C++])
AS_IF([test "x$enable_native" != "xno"], [
    CXXFLAGS="$CXXFLAGS -march=native -mtune=native"
    CFLAGS="$CFLAGS -march=native -mtune=native"
],[
    AS_IF([test -n "$ubercompat"], [
        CXXFLAGS="$CXXFLAGS -march=pentium4 -mtune=generic"
        CFLAGS="$CFLAGS -march=pentium4 -mtune=generic"
    ], [
        CXXFLAGS="$CXXFLAGS -march=${host_cpu//_/-} -mtune=generic"
        CFLAGS="$CFLAGS -march=${host_cpu//_/-} -mtune=generic"
    ])
])
# use c/c++23 because we can
AX_CHECK_COMPILE_FLAG([-std=gnu++23], [
    AX_CHECK_COMPILE_FLAG([-std=gnu++26], [
        NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -std=gnu++26"
        ], [
        NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -std=gnu++23"
    ])
], [
    AC_MSG_ERROR([Compiler does not support gnu++23])
])

AC_CHECK_TOOL([LD], [ld], [AC_MSG_ERROR([ld not found])])
AS_IF([test "x$enable_clang" = "xyes" && test "$host_os" = "linux-gnu"], [
    AC_CHECK_PROG([AR], [llvm-ar], [llvm-ar], [AC_MSG_ERROR([llvm-ar not found])])
    AC_CHECK_PROG([RANLIB], [llvm-ranlib], [llvm-ranlib], [AC_MSG_ERROR([llvm-ranlib not found])])
], [
    AC_CHECK_TOOL([AR], [ar], [AC_MSG_ERROR([ar not found])])
    AC_CHECK_TOOL([RANLIB], [ranlib], [AC_MSG_ERROR([ranlib not found])])
])

AS_IF([test "$host_os" = "linux-gnu"],[
    AC_CHECK_HEADERS([X11/Xlib.h GL/gl.h GL/glu.h], [],
        [AC_MSG_ERROR([Missing required X11/OpenGL development headers])])
])

AS_IF([test "x$enable_debug" = "xyes"], [
    AS_IF([test "x$with_mimalloc" != "xyes"], [with_mimalloc=no])
    AS_IF([test "$host_os" = "mingw32"],[
        CXXFLAGS="$CXXFLAGS -g -Og"
        CFLAGS="$CFLAGS -g -Og"
    ], [
        CXXFLAGS="$CXXFLAGS -ggdb3 -gdwarf-5 -Og -fasynchronous-unwind-tables"
        CFLAGS="$CFLAGS -ggdb3 -gdwarf-5 -Og -fasynchronous-unwind-tables"
    ])
    ENABLE_DEBUG=$enable_debug
    NEOSU_CPPFLAGS="$NEOSU_CPPFLAGS -D_DEBUG"
    AS_IF([test -z "$CLANG"], [
        NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -Wcast-align=strict -Wpacked-not-aligned"
        # CPPFLAGS="$CPPFLAGS -D_GLIBCXX_DEBUG"
        # CXXFLAGS="$CXXFLAGS -D_GLIBCXX_DEBUG"
    ])
    AS_IF([test "$SYSNAME" = "windows"], [
        AS_IF([test "x$enable_clang" = "xyes"], [
            LDFLAGS="$LDFLAGS -Xlinker /subsystem:console"
        ], [
            NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -mconsole"
            NEOSU_LDFLAGS="$NEOSU_LDFLAGS -mconsole"
        ])
    ])
    AC_SUBST([ENABLE_DEBUG])
    AS_IF([test "x$enable_strip" != "xyes"], [enable_strip=no]) # disable stripping unless its explicitly requested for debug (why tho?)
], [
    AS_IF([test "x$enable_optimization" != "xno"], [
        CPPFLAGS="$CPPFLAGS -DNDEBUG -D_NDEBUG"
        CXXFLAGS="${CXXFLAGS//-g -O2/} -O3 -g0" # where do those -g -O2 flags even come from...
        CFLAGS="${CFLAGS//-g -O2/} -O3 -g0"

        AS_IF([test "x$enable_clang" != "xyes"], [
            CXXFLAGS="$CXXFLAGS -fipa-pta"
            CFLAGS="$CFLAGS -fipa-pta"
        ])
    ])
    AS_IF([test "x$enable_strip" != "xno"], [
        NEOSU_LDFLAGS="$NEOSU_LDFLAGS -Wl,-s,--gc-sections" # strip and GC
    ], [
        # if "don't strip" was explicitly requested, add some debug info to the binary
        AS_IF([test "x$enable_strip" = "xno"], [
            CXXFLAGS="$CXXFLAGS -g"
            CFLAGS="$CFLAGS -g"
            NEOSU_LDFLAGS="$NEOSU_LDFLAGS -Wl,--no-gc-sections"
        ])
    ])
])

AS_IF([test "x$enable_omp" = "xyes"], [
    AS_IF([test "x$enable_clang" = "xyes"], [
        NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -fopenmp-optimistic-collapse"
    ])
    NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -fopenmp"
])

AS_IF([test "x$enable_lto" != "xno"], [
    AX_CHECK_COMPILE_FLAG([-flto=thin], [
        AX_CHECK_LINK_FLAG([-flto=thin -fno-fat-lto-objects -fwhole-program-vtables], [
            CXXFLAGS="$CXXFLAGS -flto=thin -fno-fat-lto-objects -fwhole-program-vtables"
            CFLAGS="$CFLAGS -flto=thin -fno-fat-lto-objects -fwhole-program-vtables"
            LDFLAGS="$LDFLAGS -flto=thin -fno-fat-lto-objects -fwhole-program-vtables"
            mkdir -p $PWD/.ltocache-clang
            AX_CHECK_LINK_FLAG([-Wl,--thinlto-cache-dir=$PWD/.ltocache-clang], [
                CXXFLAGS="$CXXFLAGS -Wl,--thinlto-cache-dir=$PWD/.ltocache-clang"
                CFLAGS="$CFLAGS -Wl,--thinlto-cache-dir=$PWD/.ltocache-clang"
                LDFLAGS="$LDFLAGS -Wl,--thinlto-cache-dir=$PWD/.ltocache-clang"
            ])
        ])], [
        # gcc
        AX_CHECK_LINK_FLAG([-flto=auto -ffat-lto-objects -fuse-linker-plugin -fdevirtualize-at-ltrans], [
            CXXFLAGS="$CXXFLAGS -flto=auto -ffat-lto-objects -fuse-linker-plugin -fdevirtualize-at-ltrans"
            CFLAGS="$CFLAGS -flto=auto -ffat-lto-objects -fuse-linker-plugin -fdevirtualize-at-ltrans"
            LDFLAGS="$LDFLAGS -flto=auto -ffat-lto-objects -fuse-linker-plugin -fdevirtualize-at-ltrans"
            mkdir -p $PWD/.ltocache-gcc
            AX_CHECK_LINK_FLAG([-flto-incremental=$PWD/.ltocache-gcc], [
                CXXFLAGS="$CXXFLAGS -flto-incremental=$PWD/.ltocache-gcc"
                CFLAGS="$CFLAGS -flto-incremental=$PWD/.ltocache-gcc"
                LDFLAGS="$LDFLAGS -flto-incremental=$PWD/.ltocache-gcc"
            ])
        ])
    ])
])

NEOSU_CPPFLAGS="$NEOSU_CPPFLAGS -DCJSON_HIDE_SYMBOLS -DGLM_ENABLE_EXPERIMENTAL"
NEOSU_CXXFLAGS="$NEOSU_CXXFLAGS -Wall -Wno-sign-compare -Wunused-parameter -Wdeprecated-declarations -Wno-error=deprecated-declarations -pthread"
NEOSU_LDFLAGS="$NEOSU_LDFLAGS -pthread"
AC_SUBST([NEOSU_CPPFLAGS])
AC_SUBST([NEOSU_CXXFLAGS])
AC_SUBST([NEOSU_LDFLAGS])

# sanity check for pre-bundled assets and required files before checking for external dependencies
AC_MSG_CHECKING([for src/Makefile.sources])
AS_IF([test -f "$srcdir/src/Makefile.sources"], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([src/Makefile.sources not found. Run ./autogen.sh first.])
])

ASSETS_DIR=assets
AC_SUBST([ASSETS_DIR])

AUDIO_CFLAGS=""
AUDIO_LIBS=""

AC_SUBST([AUDIO_CFLAGS])
AC_SUBST([AUDIO_LIBS])

# the source file is also added through autogen.sh to Makefile.sources
BUNDLED_INCLUDES="-I\$(srcdir)/libraries"
BUNDLED_INCLUDES="$BUNDLED_INCLUDES -I\$(srcdir)/libraries/delegatesv2 -I\$(srcdir)/libraries/glad -I\$(srcdir)/libraries/glad_glx -I\$(srcdir)/libraries/glad_wgl"
AC_SUBST([BUNDLED_INCLUDES])

AC_MSG_CHECKING([for shaders in $srcdir/$ASSETS_DIR/shaders])
AS_IF([test -d "$srcdir/$ASSETS_DIR/shaders"], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([neosu shader assets are missing!])
])

AC_MSG_CHECKING([for fonts in $srcdir/$ASSETS_DIR/fonts])
AS_IF([test -d "$srcdir/$ASSETS_DIR/fonts"], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([neosu fonts are missing!])
])

AC_MSG_CHECKING([for default material assets in $srcdir/$ASSETS_DIR/materials])
AS_IF([test -d "$srcdir/$ASSETS_DIR/materials"], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([neosu material assets are missing!])
])

AC_DEFUN([PKG_CHECK_AUTO], [
    AS_IF([test "x$enable_static" = "xyes"],
        [PKG_CHECK_MODULES_STATIC([$1], [$2], [$3], [$4])],
        [PKG_CHECK_MODULES([$1], [$2], [$3], [$4])])
])

AC_SUBST([DEPS_PREFIX], ['${abs_top_builddir}/build/deps'])
AC_SUBST([DEPS_CACHE], ['${abs_top_srcdir}/build-aux/cache'])
AC_SUBST([BUILD_MISCDIR], ['${abs_top_srcdir}/build-aux/misc'])
AC_SUBST([bindir], [$bindir-$host_cpu])

AS_IF([test "$renderer" != "dx11"], [
    AS_IF([test "$SYSNAME" = "linux"], [
        PKG_CHECK_AUTO([GLU], [glu], [have_glu=yes], [AC_MSG_ERROR([Missing glu libs, required for OpenGL])])
        PKG_CHECK_AUTO([GL], [gl], [have_gl=yes], [AC_MSG_ERROR([Missing gl libs, required for OpenGL])])
        PKG_CHECK_AUTO([X11], [x11], [have_x11=yes], [AC_MSG_ERROR([Missing x11 libs, required for OpenGL on Linux])])
        PKG_CHECK_AUTO([XI], [xi], [have_xi=yes], [AC_MSG_ERROR([Missing xi (xinput) libs, required for OpenGL on Linux])])
        PKG_CHECK_AUTO([XRANDR], [xrandr], [have_xrandr=yes], [AC_MSG_ERROR([Missing xrandr libs, required for Linux])])
        PKG_CHECK_AUTO([EGL], [egl], [have_egl=yes], [AC_MSG_ERROR([Missing egl libs, required for OpenGL on Linux])])
    ], [
        have_glu="not required"
        have_gl="not required"
        have_x11="not required"
        have_xi="not required"
        have_xrandr="not required"
        have_egl="not required"
    ])
],[
    AS_IF([test "$SYSNAME" = "linux"], [
        PKG_CHECK_AUTO([DXGI], [dxvk-dxgi], [have_dxgi=yes], [have_dxgi=no])
        PKG_CHECK_AUTO([D3D11], [dxvk-d3d11], [have_d3d11=yes], [have_d3d11=no])
        PKG_CHECK_AUTO([VKD3D], [libvkd3d-utils], [have_vkd3d=yes], [have_vkd3d=no])
        AS_IF([test "x$have_dxgi" != "xyes" || test "x$have_d3d11" != "xyes" || test "x$have_vkd3d" != "xyes"], [
            AC_MSG_ERROR([Missing dxvk-native or libvkd3d-utils libraries for D3D11 on Linux])
        ])
        FORCE_SDL_DYNAMIC=1 # dxvk double-loads the wrong library version if we statically link to sdl, so until i (if ever) build dxvk in-tree
                            # it needs to be forced dynamic
    ],[

        AC_SUBST([D3D11_CFLAGS], [""])
        AC_SUBST([D3D11_LIBS], [["-ld3d11 $d3dcompiler_lib -ldxgi"]])
        AC_SUBST([DXGI_CFLAGS], [""])
        AC_SUBST([DXGI_LIBS], [["-ldxgi"]])
        AC_SUBST([VKD3D_CFLAGS], [""])
        AC_SUBST([VKD3D_LIBS], [[""]])
    ])
])

SDL3_MIN_VERSION=3.4.0 # needs SDL_SetRelativeMouseTransform, not in any current (3.2.x) releases
SDL3_BUILD_VERSION=c8e2d13173d6ddbaf18316a376f40cdca42e4679 # git
FREETYPE2_MIN_VERSION=2.10.0
FREETYPE2_BUILD_VERSION=2.13.3
LIBJPEG_MIN_VERSION=3.0.0
LIBJPEG_BUILD_VERSION=3.1.0
LIBPNG_MIN_VERSION=1.6.38
LIBPNG_BUILD_VERSION=1.6.48
ZLIB_MIN_VERSION=1.3
ZLIB_BUILD_VERSION=2.2.4 # zlib-ng
BZIP2_MIN_VERSION=1.0.8
BZIP2_BUILD_VERSION=1.0.8
FMT_MIN_VERSION=11.0.0
FMT_BUILD_VERSION=35dcc58263d6b55419a5932bd6b0b3029a0a8c00
LZMA_MIN_VERSION=5.4.7
LZMA_BUILD_VERSION=5.8.1
LIBARCHIVE_MIN_VERSION=3.6.2
LIBARCHIVE_BUILD_VERSION=3.8.1
CURL_MIN_VERSION=8.5.0
CURL_BUILD_VERSION=8.15.0
MPG123_MIN_VERSION=1.30.0
MPG123_BUILD_VERSION=ec6ead5fc1102d64efbb6a04865f59454198c3fc # git mirror, latest as of June 16th, 2025 (next version after 1.33.0)

# always build these from source if we want to use them
AC_SUBST([SOLOUD_VERSION], [1.1.4]) # my fork
#AC_SUBST([SOLOUD_VERSION], [0aa5d49e7117a15554c22ed9f060207bc9a34c45])
AC_SUBST([SOLOUD_PREFIX],[$DEPS_PREFIX])
AC_SUBST([SOUNDTOUCH_VERSION], [2.4.0])
AC_SUBST([SOUNDTOUCH_PREFIX],[$DEPS_PREFIX])
AC_SUBST([SDL3_MIXER_VERSION], [b8017938de8b31c1962c2c8044a127519c2c1cdf])
AC_SUBST([SDL3_MIXER_PREFIX],[$DEPS_PREFIX])
AC_SUBST([MIMALLOC_VERSION], [639b73ecf9608d9c3078cb9dee5b7c2f46639828])
AC_SUBST([MIMALLOC_PREFIX],[$DEPS_PREFIX])

AC_SUBST([FFMPEG_VERSION], [7.1]) # pre-built, used for soloud

AC_SUBST([DISCORDSDK_VERSION], [2.5.6]) # pre-built

AC_SUBST([BASS_VERSION], [20250721]) # just mark the date so we can invalidate the cache if necessary

AS_IF([test "$SYSNAME" = "linux"], [
    PKG_CHECK_AUTO([BLKID], [blkid], [have_blkid=yes], [AC_MSG_ERROR([Missing blkid, required for linux builds.])])
    AS_IF([test "x$enable_system_deps" != "xno"], [
        PKG_CHECK_AUTO([CURL], [libcurl >= $CURL_MIN_VERSION], [have_curl=yes], [have_curl=no])
    ])
], [
    AC_SUBST([EXTRA_WIN_LIBS], [["-lopengl32 -lcomctl32"]])
    have_blkid="not required"
    have_curl="no"
])

# TODO: add all deps and transitive deps (fun!)
AS_IF([test "x$enable_system_deps" != "xno"], [
    PKG_CHECK_AUTO([SDL3], [sdl3 >= $SDL3_MIN_VERSION], [have_sdl3=yes], [have_sdl3=no])
    PKG_CHECK_AUTO([LIBJPEG], [libturbojpeg >= $LIBJPEG_MIN_VERSION], [have_libjpeg=yes], [have_libjpeg=no])
    PKG_CHECK_AUTO([LIBPNG], [libpng >= $LIBPNG_MIN_VERSION], [have_libpng=yes], [have_libpng=no])
    PKG_CHECK_AUTO([ZLIB], [zlib >= $ZLIB_MIN_VERSION], [have_zlib=yes], [have_zlib=no])
    PKG_CHECK_AUTO([BZIP2], [bzip2 >= $BZIP2_MIN_VERSION], [have_bzip2=yes], [have_bzip2=no])
    PKG_CHECK_AUTO([FREETYPE2], [freetype2 >= $FREETYPE2_MIN_VERSION], [have_freetype2=yes], [have_freetype2=no])
    PKG_CHECK_AUTO([FMT], [fmt >= $FMT_MIN_VERSION], [have_fmt=yes], [have_fmt=no])
    PKG_CHECK_AUTO([LZMA], [liblzma >= $LZMA_MIN_VERSION], [have_liblzma=yes], [have_liblzma=no])
    PKG_CHECK_AUTO([LIBARCHIVE], [libarchive >= $LIBARCHIVE_MIN_VERSION], [have_libarchive=yes], [have_libarchive=no])
    AS_IF([test "$with_soloud" = "yes"], [
        PKG_CHECK_AUTO([MPG123], [libmpg123 >= $MPG123_MIN_VERSION], [have_mpg123=yes], [have_mpg123=no])
    ])
    AC_CHECK_HEADERS([glm/vec3.hpp], [have_glm=yes], [have_glm=no])
])

AS_IF([test "x$have_sdl3" != "xyes"], [have_sdl3="build from source"])
SDL3_VERSION=$SDL3_BUILD_VERSION
SDL3_PREFIX="${DEPS_PREFIX}"
AS_IF([test "$STATICBUILD" = "yes" && test -z "$FORCE_SDL_DYNAMIC"], [
    SDL3_STATIC=1
])
AC_SUBST([SDL3_VERSION])
AC_SUBST([SDL3_PREFIX])
AC_SUBST([SDL3_STATIC])

AS_IF([test "x$have_libjpeg" != "xyes"], [
    have_libjpeg="build from source"
    AC_PROG_NASM
])

LIBJPEG_VERSION=$LIBJPEG_BUILD_VERSION
LIBJPEG_PREFIX="${DEPS_PREFIX}"
AC_SUBST([LIBJPEG_VERSION])
AC_SUBST([LIBJPEG_PREFIX])

AS_IF([test "x$have_libpng" != "xyes"], [have_libpng="build from source"])
LIBPNG_VERSION=$LIBPNG_BUILD_VERSION
LIBPNG_PREFIX="${DEPS_PREFIX}"
AC_SUBST([LIBPNG_VERSION])
AC_SUBST([LIBPNG_PREFIX])

AS_IF([test "x$have_bzip2" != "xyes"], [have_bzip2="build from source"])
BZIP2_VERSION=$BZIP2_BUILD_VERSION
BZIP2_PREFIX="${DEPS_PREFIX}"
AC_SUBST([BZIP2_VERSION])
AC_SUBST([BZIP2_PREFIX])

AS_IF([test "x$have_zlib" != "xyes"], [have_zlib="build from source"])
ZLIB_VERSION=$ZLIB_BUILD_VERSION
ZLIB_PREFIX="${DEPS_PREFIX}"
AC_SUBST([ZLIB_VERSION])
AC_SUBST([ZLIB_PREFIX])

AS_IF([test "x$have_freetype2" != "xyes"], [have_freetype2="build from source"])
FREETYPE2_VERSION=$FREETYPE2_BUILD_VERSION
FREETYPE2_PREFIX="${DEPS_PREFIX}"
AC_SUBST([FREETYPE2_VERSION])
AC_SUBST([FREETYPE2_PREFIX])

AS_IF([test "x$have_fmt" != "xyes"], [have_fmt="build from source"])
FMT_VERSION=$FMT_BUILD_VERSION
FMT_PREFIX="${DEPS_PREFIX}"
AC_SUBST([FMT_VERSION])
AC_SUBST([FMT_PREFIX])

AS_IF([test "x$have_liblzma" != "xyes"], [have_liblzma="build from source"])
LZMA_VERSION=$LZMA_BUILD_VERSION
LZMA_PREFIX="${DEPS_PREFIX}"
AC_SUBST([LZMA_VERSION])
AC_SUBST([LZMA_PREFIX])

AS_IF([test "x$have_libarchive" != "xyes"], [have_libarchive="build from source"])
LIBARCHIVE_VERSION=$LIBARCHIVE_BUILD_VERSION
LIBARCHIVE_PREFIX="${DEPS_PREFIX}"
AC_SUBST([LIBARCHIVE_VERSION])
AC_SUBST([LIBARCHIVE_PREFIX])

AS_IF([test "x$have_curl" != "xyes"], [have_curl="download"])
CURL_VERSION=$CURL_BUILD_VERSION
CURL_PREFIX="${DEPS_PREFIX}"
AC_SUBST([CURL_VERSION])
AC_SUBST([CURL_PREFIX])

AS_IF([test "$with_soloud" = "yes"], [
    AS_IF([test "x$have_mpg123" != "xyes"], [have_mpg123="build from source"])
], [
    have_mpg123="not required"
])
MPG123_VERSION=$MPG123_BUILD_VERSION
MPG123_PREFIX="${DEPS_PREFIX}"
AC_SUBST([MPG123_VERSION])
AC_SUBST([MPG123_PREFIX])

AS_IF([test "x$have_glm" != "xyes"], [have_glm="build from source"])
AC_SUBST([GLM_VERSION], [1.0.1])
AC_SUBST([GLM_PREFIX],[$DEPS_PREFIX])
AC_SUBST([GLM_CFLAGS], [""])

AM_CONDITIONAL([BUILD_SDL3], [test "x$have_sdl3" != "xyes"])
AM_CONDITIONAL([BUILD_LIBJPEG], [test "x$have_libjpeg" != "xyes"])
AM_CONDITIONAL([BUILD_LIBPNG], [test "x$have_libpng" != "xyes"])
AM_CONDITIONAL([BUILD_BZIP2], [test "x$have_bzip2" != "xyes"])
AM_CONDITIONAL([BUILD_ZLIB], [test "x$have_zlib" != "xyes"])
AM_CONDITIONAL([BUILD_FREETYPE2], [test "x$have_freetype2" != "xyes"])
AM_CONDITIONAL([BUILD_FMT], [test "x$have_fmt" != "xyes"])
AM_CONDITIONAL([BUILD_LZMA], [test "x$have_liblzma" != "xyes"])
AM_CONDITIONAL([BUILD_LIBARCHIVE], [test "x$have_libarchive" != "xyes"])
AM_CONDITIONAL([BUILD_CURL], [test "x$have_curl" != "xyes"])
AM_CONDITIONAL([BUILD_MPG123], [test "$with_soloud" = "yes" && test "x$have_mpg123" != "xyes"])
AM_CONDITIONAL([BUILD_GLM], [test "x$have_glm" != "xyes"])
AM_CONDITIONAL([BUILD_MIMALLOC], [{ test "x$enable_debug" != "xyes" || test "x$with_mimalloc" = "xyes"; } &&
                                { { test "$SYSNAME" = "windows" && test "$host_cpu" = "i686" && test "x$with_mimalloc" = "xyes" ; } ||
                                  { test "x$with_mimalloc" != "xno" ; };
                                }]) # default off for windows 32bit for compatibility

AM_CONDITIONAL([USE_BASS], [test "$with_bass" = "yes"])
AM_CONDITIONAL([USE_SOLOUD], [test "$with_soloud" = "yes"])
AM_CONDITIONAL([WIN_PLATFORM], [test "$SYSNAME" = "windows"])
AM_CONDITIONAL([WIN32_PLATFORM], [test "$SYSNAME" = "windows" && test "$host_cpu" = "i686"])

AM_CONDITIONAL([STRIPBINS], [test "x$enable_strip" != "xno" && ! { test "$SYSNAME" = "windows" && test "$host_cpu" = "i686" ; } ])

# need dynamic winpthread for old windows 32bit compat builds
# the way im searching for the right .dlls to copy is so disgusting and needs to be fixed somehow
# also need this for 64bit so that the winpthread resources don't override neosu's...
AS_IF([test "$SYSNAME" = "windows"], [
    AS_IF([test "x$enable_static" = "xyes"], [ # still need winpthread dynamic, breaks otherwise for reasons i dont understand
        WINSHARED_DLLS="$($CC -print-prog-name=ld | sed -e 's|/bin/.*||' | tr -d '\n' | find -files0-from - \
            -iregex ".*$host_cpu"'.*libwinpthread-1.dll' \
            -printf '%h/%f ')"
    ], [
        WINSHARED_DLLS="$($CC -print-prog-name=ld | sed -e 's|/bin/.*||' | tr -d '\n' | find -files0-from - \
            '(' -iregex ".*$host_cpu"'.*libwinpthread-1.dll' -o -iregex ".*$host_cpu"'.*libgcc_.*.dll' -o -iregex ".*$host_cpu"'.*libstdc.*.dll' ')' \
            -printf '%h/%f ')"
    ])
])

AC_SUBST([WINSHARED_DLLS])
AC_SUBST([ubercompat])
AM_CONDITIONAL([WINSHARED_DLLS_NEEDED], [test -n "$WINSHARED_DLLS"])

AM_CONDITIONAL([AUDIO_SHORTCUTS], [test -n "$make_audio_shortcuts" && test -n "$WINE" && test "$SYSNAME" = "windows"])

# output makefile
AC_CONFIG_FILES([Makefile])

AC_OUTPUT

AC_MSG_NOTICE([
neosu build configuration:
    Build target:       ${target}
    Install prefix:     ${prefix}
    Renderer:           ${renderer}
    Preprocessor flags: ${CPPFLAGS}
    ccache:             ${CCACHE-no}
    clang:              ${CLANG-no}
    C++ compiler:       ${CXX}
    C++ flags:          ${CXXFLAGS}
    C compiler:         ${CC}
    C flags:            ${CFLAGS}
    Linker flags:       ${LDFLAGS}
    mimalloc:           ${with_mimalloc-yes}
    BASS:               ${with_bass-no}
    SoLoud:             ${with_soloud-no}
    IME enabled:        ${enable_ime-yes}
    LTO:                ${enable_lto-auto}
    OpenMP:             ${enable_omp-no}
    Debug build:        ${enable_debug-no}
    ASan build:         ${enable_asan-no}
    TSan build:         ${enable_tsan-no}
    Native build:       ${enable_native-no}
    System deps:        ${enable_system_deps-yes}
    Static linking:     ${enable_static-no}
    Stripped build:     ${enable_strip-yes}

Dependencies:
    libX11              ${have_x11-no}
    libXi (XInput)      ${have_xi-no}
    xrandr              ${have_xrandr-no}
    libGL               ${have_gl-no}
    libGLU              ${have_glu-no}
    libEGL              ${have_egl-no}
    blkid               ${have_blkid-no}
    sdl3:               ${have_sdl3-build from source}
    libjpeg:            ${have_libjpeg-build from source}
    libpng:             ${have_libpng-build from source}
    bzip2:              ${have_bzip2-build from source}
    zlib:               ${have_zlib-build from source}
    freetype2:          ${have_freetype2-build from source}
    fmt:                ${have_fmt-build from source}
    curl:               ${have_curl-download}
    liblzma:            ${have_liblzma-build from source}
    libarchive:         ${have_libarchive-build from source}
    mpg123:             ${have_mpg123-build from source}
    glm (math)          ${have_glm-build from source}
])
