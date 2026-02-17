# Peraviz DMX Input (Art-Net RX)

## Build flag

Enable DMX input support for the Godot native module with:

```cmake
-DPERAVIZ_ENABLE_DMX=ON
```

The option is declared in `peraviz/native/CMakeLists.txt` and defaults to `ON`.
When `OFF`, no DMX sources or DMX-specific link libraries are compiled.
Disable it explicitly with `-DPERAVIZ_ENABLE_DMX=OFF`.
If your build directory was configured earlier with DMX disabled, reconfigure with `-DPERAVIZ_ENABLE_DMX=ON`.

## Current support

Implemented in this phase:

- Art-Net ArtDmx packet reception (UDP 6454)
- Multi-universe cache with polling APIs for runtime UI
- Non-blocking background receiver thread

Not implemented yet:

- sACN / E1.31
- ArtPoll / ArtPollReply discovery
- ArtSync
- RDM
- Any TX path

## Quick test

1. Build Peraviz native with `PERAVIZ_ENABLE_DMX=ON`.
2. Start Peraviz.
3. Enable DMX in the runtime HUD using the `DMX ON/OFF` toggle.
4. Use QLC+ (or similar) to send Art-Net ArtDmx on universe 0.
5. Confirm the DMX status line updates packets per second and active universes.
