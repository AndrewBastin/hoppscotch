# Hoppscotch C Core

High-performance C rewrite of core Hoppscotch functionality for improved performance and resource efficiency.

## Overview

This package provides a native C implementation of Hoppscotch's HTTP client and testing capabilities. The C core is designed to be fast, memory-efficient, and suitable for integration into various environments including CLI tools, desktop applications, and embedded systems.

## Prerequisites

### Required System Dependencies

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libcurl4-openssl-dev \
    libcjson-dev \
    clang-format
```

#### Linux (Fedora/RHEL/CentOS)
```bash
sudo dnf install -y \
    gcc \
    gcc-c++ \
    make \
    cmake \
    libcurl-devel \
    cjson-devel \
    clang-tools-extra
```

#### macOS
```bash
# Install Homebrew if not already installed
# https://brew.sh

brew install cmake curl cjson clang-format
```

### Dependency Details

- **CMake** (≥ 3.15): Build system generator
- **libcurl** (≥ 7.0): HTTP client library for making requests
- **cJSON** (optional): JSON parsing and generation (recommended for future features)
- **clang-format** (optional): Code formatting tool

## Building

### Using pnpm (Recommended)

From the repository root:

```bash
# Configure and build
pnpm --filter @hoppscotch/c-core build

# Build debug version
pnpm --filter @hoppscotch/c-core build:debug

# Run tests
pnpm --filter @hoppscotch/c-core test

# Format code
pnpm --filter @hoppscotch/c-core format

# Check code formatting
pnpm --filter @hoppscotch/c-core format:check

# Clean build artifacts
pnpm --filter @hoppscotch/c-core clean
```

### Using CMake Directly

From this package directory:

```bash
# Configure (Release)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Configure (Debug)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Release

# Run tests
cd build && ctest --output-on-failure

# Install (optional)
sudo cmake --install build
```

## Project Structure

```
hoppscotch-c-core/
├── CMakeLists.txt          # Main CMake configuration
├── package.json            # npm/pnpm package definition
├── README.md              # This file
├── .clang-format          # Code style configuration
├── .gitignore             # Git ignore rules
├── include/               # Public header files
│   └── hoppscotch.h      # Main library header with module documentation
├── src/                   # Source files
│   └── main.c            # Main executable entry point
├── tests/                 # Test suite
│   ├── CMakeLists.txt    # Test configuration
│   └── test_basic.c      # Basic functionality tests
└── build/                 # Build artifacts (gitignored)
```

## Current Implementation

This is a **minimal scaffold** with the following components:

- ✅ Basic project structure (CMake, pnpm integration)
- ✅ Placeholder executable (`hoppscotch-c`)
- ✅ Library initialization/cleanup functions
- ✅ Version information API
- ✅ Basic test suite
- ✅ libcurl integration

### Running the Executable

After building:

```bash
./build/hoppscotch-c
```

Output:
```
Hoppscotch C Core - Version 0.1.0
=====================================

✓ Library initialized successfully
✓ libcurl version: 8.x.x
...
```

## Future Module Layout

The `include/hoppscotch.h` header documents the planned architecture:

1. **HTTP Client** - Request/response handling, multiple HTTP methods
2. **Authentication** - Basic, Bearer, OAuth 2.0, API keys
3. **Request Chain** - Sequential execution, dependencies, variable substitution
4. **Environment** - Variable management and interpolation
5. **Collections** - Collection parsing, import/export
6. **Testing/Assertions** - Response validation, assertions
7. **Utilities** - String manipulation, JSON, encoding
8. **Error Handling** - Comprehensive error reporting

## Development

### Code Style

This project uses `clang-format` with the configuration in `.clang-format`. Format your code before committing:

```bash
pnpm --filter @hoppscotch/c-core format
```

### Adding New Source Files

1. Add `.c` files to `src/`
2. Add `.h` files to `include/`
3. Update `CMakeLists.txt` to include new source files in `HOPPSCOTCH_SOURCES`

### Adding Tests

1. Create test files in `tests/`
2. Update `tests/CMakeLists.txt` to include new test sources
3. Follow the pattern in `test_basic.c` for consistency

## Integration with Root Package

The root `package.json` includes convenience scripts:

```bash
# From repository root
pnpm c:configure     # Configure CMake build
pnpm c:build         # Build the C core
pnpm c:test          # Run C tests
```

## Troubleshooting

### libcurl not found

**Error**: `Could not find CURL`

**Solution**: Install libcurl development package:
- Ubuntu/Debian: `sudo apt-get install libcurl4-openssl-dev`
- macOS: `brew install curl`

### CMake version too old

**Error**: `CMake 3.15 or higher is required`

**Solution**: Update CMake:
- Ubuntu: Download from https://cmake.org/download/
- macOS: `brew upgrade cmake`

### clang-format not found

**Error**: `clang-format: command not found`

**Solution**: Install clang-format:
- Ubuntu/Debian: `sudo apt-get install clang-format`
- macOS: `brew install clang-format`

## License

MIT License - see the root LICENSE file for details.

## Contributing

This is part of the Hoppscotch monorepo. Follow the contribution guidelines in the root repository.
