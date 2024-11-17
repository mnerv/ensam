# ensam

`ensam` is a learning project on how to parse MIDI files and play them. 

## Requirements

  - [cmake](https://cmake.org/)


## Development

There are different ways to setup this project, using `ninja` or Visual Studio.

### Ninja

To use `miniaudio` to play audio on host machine use the flag `-DUSE_MINIAUDIO=ON`.

```sh
cmake -S . -Bbuild -GNinja -DCMAKE_BUILD_TYPE=Debug
```

Use `ninja` to build.


```sh
ninja -C build
```

Symlink `compile_commands.json` to root directory for `ccls`/`clangd`.

```sh
ln -sfn ./build/compile_commands.json .
```

**Windows (requires admin)**

```sh
New-Item -ItemType SymbolicLink -Path "compile_commands.json" -Target "./build/compile_commands.json"
```

### Visual Studio

```sh
cmake -S . -Bbuild
```

Open either the visual studio project in the build directory or use

```sh
cmake --open build
```

