# XenoFlow

A DOCA Flow application for load balancing and flow management on NVIDIA BlueField DPUs.

## Building

### Local Build

This project uses the Meson build system. To build locally:

```bash
meson setup builddir
meson compile -C builddir
```

### GitLab CI/CD Build

This project includes a GitLab CI/CD pipeline that automatically builds the executable on Ubuntu with DOCA Flow support.

For detailed setup instructions, see [GITLAB_CI_SETUP.md](GITLAB_CI_SETUP.md).

**Quick Start:**
- The pipeline runs automatically on pushes, merge requests, and tags
- Requires a GitLab runner with DOCA SDK installed
- Built executables are stored as artifacts for 1 week

## Requirements

- DOCA SDK (doca-common, doca-flow, doca-dpdk-bridge, doca-argp)
- DPDK (libdpdk)
- cJSON library
- Meson build system
- NVIDIA BlueField DPU hardware (for runtime)