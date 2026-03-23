# meos
MeOS - A Much Easier Orienteering System

Source code for the MeOS project (www.melin.nu/meos)

## C++ Backend

### Prerequisites

- CMake 3.28+
- Ninja build system
- C++17 compiler (GCC 13+ or Clang 16+)
- vcpkg (set `VCPKG_ROOT` environment variable)

### Build

```bash
export VCPKG_ROOT=~/vcpkg  # adjust to your vcpkg installation
cmake --preset default     # Configure (Debug)
cmake --build --preset default
```

### Test

```bash
ctest --test-dir build --output-on-failure
```

### Coverage

```bash
cmake --preset default -DMEOS_ENABLE_COVERAGE=ON
cmake --build --preset default
ctest --test-dir build
# .gcno/.gcda files in build/ for gcov/lcov
```

## Frontend (Web UI)

### Prerequisites

- Node.js 18+ and npm

### Build & Run

```bash
cd src/ui/web
npm ci
npm run build      # Production build → src/ui/web/dist/
npm run dev        # Development server with HMR
```

### Quality Checks

```bash
npm run lint       # ESLint
npm run typecheck  # TypeScript type checking
npm test           # Vitest unit tests
npm run test:coverage  # Tests with v8 coverage
```

## Legacy MeOS (code/ directory)

The `code/` directory contains the original MeOS application and is built with CMake.

### Prerequisites

- Windows (MSVC required — legacy code is Windows-only)
- CMake 3.16+
- Visual Studio 2019 or 2022 (x64 toolchain)
- OpenSSL 1.1 (install via Chocolatey: `choco install openssl --version=1.1.1.2100`)

### Build

```powershell
# Configure (x64, Visual Studio generator)
cmake -S code -B code/build -A x64 -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"

# Build Release
cmake --build code/build --config Release --parallel

# Output: code/build/Release/MeOS.exe
```

### Runtime DLLs

Copy the following DLLs next to `MeOS.exe` before running:

- `code/dll64/libharu.dll`
- `code/dll64/libmysql.dll`
- `C:/Program Files/OpenSSL-Win64/libssl-1_1-x64.dll`
- `C:/Program Files/OpenSSL-Win64/libcrypto-1_1-x64.dll`
- VC++ runtime DLLs (`MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`)

## CI/CD

GitHub Actions workflows run automatically on every push and pull request.

- **C++ CI** (`.github/workflows/cpp.yml`) — builds and tests on Linux and Windows, runs clang-tidy on Linux
- **Frontend CI** (`.github/workflows/frontend.yml`) — lint, test, and build the React frontend
- **Legacy Build CI** (`.github/workflows/build-legacy.yml`) — builds the legacy `code/` directory with CMake on Windows
