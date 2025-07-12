# minimize_test
Simple test for setting the minimize area with the wlr-foreign-toplevel-management protocol.

This works by creating a parent and child process and the parent setting a minimize area for the child.

Requirements:
 - meson for building
 - GTK+ 3.24
 - wayland-client and wayland-scanner

Building and running:
```
meson setup -Dbuildtype=debug build
ninja -C build
build/mt
```



