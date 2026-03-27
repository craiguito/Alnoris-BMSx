Build this Qt desktop shell after installing a Qt6 C++ toolchain and CMake.

Expected workflow:

```powershell
cmake -S desktop_cpp -B build/desktop_cpp
cmake --build build/desktop_cpp
.\build\desktop_cpp\Debug\alnoris_desktop.exe
```

The Qt app invokes the Python simulation core with:

```powershell
python -m backend.sim_core.cli
```

The Python side reads JSON from stdin and returns JSON on stdout.
