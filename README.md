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

### GitHub Actions Build

This project also includes a GitHub Actions workflow that automatically builds the executable on Ubuntu with DOCA Flow support.

**Features:**
- Automatically installs DOCA SDK from NVIDIA apt repositories
- Runs on Ubuntu 22.04
- Builds on all pushes, pull requests, and can be manually triggered
- Stores build artifacts for 7 days

**Configuration:**
The workflow installs DOCA version 2.9.1 from the official NVIDIA repositories. To use a different version, update the `DOCA_VERSION` and `DOCA_OS_VERSION` environment variables in `.github/workflows/build.yml`.

**Downloading Artifacts:**
1. Go to the Actions tab in GitHub
2. Select a successful workflow run
3. Download the artifacts from the "Artifacts" section

## Requirements

- DOCA SDK (doca-common, doca-flow, doca-dpdk-bridge, doca-argp)
- DPDK (libdpdk)
- cJSON library
- Meson build system
- NVIDIA BlueField DPU hardware (for runtime)