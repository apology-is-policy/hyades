# GitHub Actions Workflows

This directory contains CI/CD workflows for the Hyades project.

## Workflows

### `build.yml` - Multi-platform Build Pipeline

Builds the project on all supported platforms:

**Native Builds:**
- **Ubuntu Linux** (latest)
- **macOS** (latest)
- **Windows** (latest)

**WASM Build:**
- Uses Emscripten SDK to compile to WebAssembly
- Builds from `wasm/` directory
- Uploads artifacts for web deployment

**Triggers:**
- Push to `master`, `main`, or `develop` branches
- Pull requests to these branches

**Artifacts:**
- Native executables: `texascii-{os}-Release`
- WASM files: `texascii-wasm` (includes .wasm, .js, .html)
- Artifacts are kept for 7 days

**Optional GitHub Pages Deployment:**
The workflow includes commented-out steps for automatic deployment to GitHub Pages. To enable:
1. Uncomment the `deploy-pages` job at the end of `build.yml`
2. Enable GitHub Pages in your repository settings
3. Set source to "GitHub Actions"

### `test.yml` - Integration Tests

Runs integration tests to verify Cassilda processing:

**Test Coverage:**
- Basic document processing (`test4.cld`, `comprehensive.cld`)
- Library integration (`clean-test.c`, `test-default.c`, `test-verbatim.cld`)
- C file compilation verification (ensures processed files remain valid C)

**Platforms:**
- Ubuntu Linux
- macOS

**Triggers:**
- Same as build workflow

## Customization

### Adding More Test Files

Edit `test.yml` and add more test commands under the test steps:

```yaml
- name: Test your feature
  run: |
    cd tests
    ../build/cassilda process your-test.cld
```

### Changing Build Configuration

To build with different CMake options, edit the "Configure CMake" step in `build.yml`:

```yaml
- name: Configure CMake
  run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DYOUR_OPTION=ON
```

### Modifying Emscripten Version

Change the version in the "Setup Emscripten" step:

```yaml
- name: Setup Emscripten
  uses: mymindstorm/setup-emsdk@v14
  with:
    version: '3.1.51'  # Change this version
```

### Adding Release Automation

Create `.github/workflows/release.yml` to build and attach binaries when you create a GitHub release:

```yaml
name: Release

on:
  release:
    types: [created]

jobs:
  upload-assets:
    # Build and upload to release
    # (Use actions/upload-release-asset)
```

## Status Badges

Add these to your main README.md:

```markdown
[![Build](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml/badge.svg)](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml)
[![Tests](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/test.yml/badge.svg)](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/test.yml)
```

Replace `YOUR_USERNAME` and `YOUR_REPO` with your GitHub username and repository name.

## Local Testing

To test the workflow locally, you can use [act](https://github.com/nektos/act):

```bash
# Install act
brew install act  # macOS
# or: choco install act  # Windows

# Run workflow locally
act -j build-native
act -j build-wasm
```

## Troubleshooting

**Build fails on Windows:**
- Check that paths use forward slashes or `${{ github.workspace }}`
- Windows uses different shell commands (PowerShell vs Bash)

**WASM build fails:**
- Verify Emscripten version compatibility with your CMake setup
- Check that `wasm/CMakeLists.txt` exists and is properly configured

**Tests fail:**
- Verify test files exist in the expected locations
- Check that executables have correct permissions on Unix systems
- Ensure test commands match your actual file structure

## Performance Optimization

**Caching:**
The workflows include caching for Emscripten. To add CMake build caching:

```yaml
- name: Cache CMake build
  uses: actions/cache@v4
  with:
    path: build
    key: ${{ runner.os }}-cmake-${{ hashFiles('**/CMakeLists.txt') }}
```

**Matrix parallelization:**
The build matrix runs all OS builds in parallel, speeding up CI time.
