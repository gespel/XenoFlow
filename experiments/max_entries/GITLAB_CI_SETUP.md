# GitLab CI/CD Setup Guide for XenoFlow

This guide explains how to set up and use the GitLab CI/CD pipeline to build the XenoFlow executable with DOCA Flow on Ubuntu.

## Overview

The `.gitlab-ci.yml` configuration file defines a CI/CD pipeline that:
- Uses Ubuntu 22.04 as the build environment
- Installs necessary build dependencies (Meson, Ninja, cJSON)
- Builds the XenoFlow executable using DOCA Flow libraries
- Stores the compiled executable as an artifact

## Prerequisites

### GitLab Runner Requirements

The GitLab runner executing this pipeline must have:

1. **NVIDIA BlueField DPU Hardware**: DOCA SDK requires NVIDIA Data Processing Unit hardware
2. **DOCA SDK Installed**: The DOCA libraries and headers must be available at `/opt/mellanox/doca`
3. **Ubuntu 22.04**: The pipeline is configured for Ubuntu 22.04 (can be adjusted for other versions)

### Installing DOCA SDK on the Runner

To set up a GitLab runner with DOCA SDK:

1. **Install Ubuntu 22.04** on a system with NVIDIA BlueField DPU

2. **Add NVIDIA DOCA Repository**:
   ```bash
   # Download the DOCA repository package (adjust version as needed)
   wget https://linux.mellanox.com/public/repo/doca/<version>/ubuntu22.04/x86_64/doca-repo.deb
   sudo dpkg -i doca-repo.deb
   sudo apt-get update
   ```

3. **Install DOCA SDK**:
   ```bash
   sudo apt-get install -y doca-all doca-tools
   ```

4. **Install GitLab Runner**:
   ```bash
   curl -L "https://packages.gitlab.com/install/repositories/runner/gitlab-runner/script.deb.sh" | sudo bash
   sudo apt-get install gitlab-runner
   ```

5. **Register the Runner** with appropriate tags:
   ```bash
   sudo gitlab-runner register
   # When prompted for tags, include: doca,ubuntu
   ```

## Pipeline Configuration

### Stages

- **build**: Compiles the XenoFlow executable

### Build Job (`build:ubuntu`)

The build job performs the following steps:

1. **Before Script**:
   - Updates package lists
   - Installs build dependencies (Meson, Ninja, GCC, pkg-config, cJSON)
   - Displays environment information

2. **Script**:
   - Verifies DOCA SDK is installed
   - Configures the build using Meson
   - Compiles the project
   - Lists build artifacts

3. **Artifacts**:
   - Stores the `xeno_flow` executable
   - Stores `compile_commands.json` for IDE integration
   - Artifacts expire after 1 week

### Runner Tags

The pipeline uses the following tags to select appropriate runners:
- `doca`: Indicates the runner has DOCA SDK installed
- `ubuntu`: Indicates the runner uses Ubuntu OS

## Using the Pipeline

### Automatic Triggers

The pipeline runs automatically on:
- All branch pushes
- Merge request events
- Tag creation

### Manual Execution

To manually trigger the pipeline:
1. Go to your GitLab project's CI/CD → Pipelines
2. Click "Run Pipeline"
3. Select the branch and click "Run Pipeline"

### Downloading Artifacts

After a successful build:
1. Go to CI/CD → Pipelines
2. Click on the completed pipeline
3. Click the "Download" button next to the build job
4. Extract the `xeno_flow` executable from the archive

## Customization

### Adjusting Ubuntu Version

To use a different Ubuntu version, modify the `image` field:
```yaml
build:ubuntu:
  image: ubuntu:20.04  # or ubuntu:24.04
```

### Adding Tests

To add a test stage, uncomment and modify the test job template in `.gitlab-ci.yml`:
```yaml
test:ubuntu:
  stage: test
  image: ubuntu:22.04
  dependencies:
    - build:ubuntu
  script:
    - ./builddir/xeno_flow --help
  tags:
    - doca
    - ubuntu
```

### Modifying Build Options

To change Meson build options, modify the setup command:
```yaml
- meson setup $BUILD_DIR --buildtype=release
```

## Troubleshooting

### DOCA SDK Not Found

**Error**: `WARNING: DOCA SDK not found at /opt/mellanox/doca`

**Solution**: Ensure DOCA SDK is installed on the GitLab runner. Verify by checking:
```bash
ls -la /opt/mellanox/doca
pkg-config --list-all | grep doca
```

### Missing Dependencies

**Error**: `dependency 'doca-flow' not found`

**Solution**: Install the complete DOCA SDK package:
```bash
sudo apt-get install doca-all doca-tools
```

### Runner Not Picking Up Job

**Issue**: Pipeline stays in "pending" state

**Solution**: 
- Verify runner tags match the job requirements (`doca`, `ubuntu`)
- Check runner status in Settings → CI/CD → Runners
- Ensure the runner is not paused

### Build Failures

**Issue**: Compilation errors

**Solution**:
- Check that all source files are committed to the repository
- Verify DOCA SDK version compatibility
- Review the build logs for specific error messages

## Additional Resources

- [DOCA SDK Documentation](https://docs.nvidia.com/doca/)
- [GitLab CI/CD Documentation](https://docs.gitlab.com/ee/ci/)
- [Meson Build System](https://mesonbuild.com/)

## Support

For issues specific to:
- **DOCA SDK**: Contact NVIDIA Support or visit NVIDIA Developer Forums
- **GitLab CI/CD**: Consult GitLab documentation or your GitLab administrator
- **XenoFlow**: Open an issue in the project repository
