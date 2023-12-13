# Environment IBL Precomputation Tool

## Building

Requires a C++ compiler that is C++20 standard compliant and CMake. Example from within the repository's root:
```
mkdir build && cd build && cmake .. && cmake --build .
```

The glsl folder should have been automatically copied to the directory of the program:
```bash
...
├── build
│   ├── glsl
│   │   ├── common.frag
│   │   ├── brdf.vert
│   │   └── ...
│   ├── iblenv
...
```