# mesh-generator
An SDF-based mesh generation tool.

## Build Steps

```
git submodule update --init
meson setup build
meson compile -C build
./build/mesh_generator
```

## Usage

Enter an SDF in the SDF field, and adjust the window options for the range you want to view.
Clicking and holding anywhere outside the option window will allow you to look around with the mouse,
and while holding the mouse button, move using WASD (+ Space to move up and Shift to move down).

## Example SDFs

TODO
