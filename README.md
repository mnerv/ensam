# midip

`midip` is a learning project on how to parse MIDI files and play them.
Motivation for this was playing audio on an Axis camera with piezoelectric
speaker.

## Requirements

  - [cmake](https://cmake.org/)


## Development

To use `miniaudio` to play audio on host machine use the flag `-DUSE_MINIAUDIO=ON`.

```sh
cmake -S . -Bbuild -GNinja -DUSE_MINIAUDIO=ON
```

Use `ninja` to build.


```sh
ninja -C build
```

