# XenoFlow

A DOCA Flow application for load balancing and flow management on NVIDIA BlueField DPUs. It also includes a web-based dashboard for monitoring and managing flows.

## Building

### Local Build

This project uses the Meson build system. To build locally:

```bash
meson setup build
ninja -C build
```

XenoFlow will be built in the `build` directory.
It can be run as follows:

```bash
sudo build/xeno_flow
```