# Quasicrystal

This is a cute visualization of a quasicrystal using interfering plane waves. It
uses multithreaded software rendering and SSE instructions and is able to do
realtime rendering (at least on reasonable resolutions and number of planes :-).

![Crystal using N=5 Waves](/crystal_5.gif?raw=true)
![Crystal using N=7 Waves](/crystal_7.gif?raw=true)
![Crystal using N=11 Waves](/crystal_11.gif?raw=true)

![Crystal using N=23 Waves](/crystal_23.gif?raw=true)

## Commandline Help


```
Usage: crystal [ARGS]...

Options:
  -v          Enable verbose output
  -n NWAVES   Number of WAVES in the crystal
  -c NCOS     Size of cos(x) lookup table
  -j WORKERS  Use WORKERS number of threads
  -s WxH      Framebuffer size, W pixels wide and H pixels tall
  -C N        Capture only: save N frames without opening a window
```

## Building

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
