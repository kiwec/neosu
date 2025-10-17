# neosu

![Linux build](https://github.com/kiwec/neosu/actions/workflows/linux-multiarch.yml/badge.svg) ![Windows build](https://github.com/kiwec/neosu/actions/workflows/win-multiarch.yml/badge.svg) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/kiwec/neosu)

This is a third-party fork of McKay's [McOsu](https://store.steampowered.com/app/607260/McOsu/).

If you need help, contact `kiwec` or `spec.ta.tor` on Discord, either by direct message or on [the neosu server](https://discord.com/invite/YWPBFSpH8v).

### Building

The recommended way to build (and the way releases are made) is using gcc/gcc-mingw.

- For all *nix systems, run `./autogen.sh` in the top-level folder (once) to generate the build files.
- Create and enter a build subdirectory; e.g. `mkdir build && cd build`
- On Linux, for Linux -> run `../configure`, then `make install`
  - This will build and install everything under `./dist/bin-$arch`, configurable with the `--prefix` option to `configure`
- On Linux/WSL, for Windows -> run ` ../configure --host=x86_64-w64-mingw32`, then `make install`

For an example of a GCC (Linux) build on Debian, see the [Linux](https://github.com/kiwec/neosu/blob/master/.github/workflows/linux-multiarch.yml) Actions workflow (and [associated](https://github.com/kiwec/neosu/blob/master/.github/workflows/docker/Dockerfile) [scripts](https://github.com/kiwec/neosu/blob/master/.github/workflows/docker/build.sh)).

For an example of a MinGW-GCC build on Arch Linux, see the [Windows](https://github.com/kiwec/neosu/blob/master/.github/workflows/win-multiarch.yml) Actions workflow.

These should help with finding a few obscure autotools-related packages that you might not have installed.

---

For debugging convenience, you can also do an **MSVC** build with **CMake** on **Windows**, by running `buildwin64.bat` in `cmake-win`. For this to work properly, a couple prerequisites you'll need:

- The [NASM](https://nasm.us/) assembler installed/in your PATH (it's searched for under `C:/Program Files/NASM` by default otherwise); used for bundling binary resources into the executable
- You'll probably also need `python3` + `pip` set up on your system, so that a newer version of `pkg-config` can be installed (automatic if you have `pip`); used for building/finding dependencies before the main project is built
