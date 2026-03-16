# meos
MeOS - A Much Easier Orienteering System

Source code for the MeOS project (www.melin.nu/meos)

## Legacy MeOS Windows Application

The legacy Win32 MeOS application lives in `code/` and builds with CMake + MSVC on Windows.

### Prerequisites

- Visual Studio 2022 (with C++ desktop workload)
- CMake 3.20+
- vcpkg (for zlib, minizip, and other managed dependencies)
- OpenSSL 1.1.x (via Chocolatey or installed manually)

### Build

```powershell
# Install OpenSSL 1.1 (once)
choco install openssl --version=1.1.1.2100 -y

# Clone and bootstrap vcpkg (once)
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && .\bootstrap-vcpkg.bat -disableMetrics && cd ..

# Configure — vcpkg manifest mode installs zlib, minizip, etc. from vcpkg.json
cmake -S code -B code/build `
  -G "Visual Studio 17 2022" -A x64 `
  -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64" `
  -DCMAKE_TOOLCHAIN_FILE="./vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_MANIFEST_DIR="."

# Build
cmake --build code/build --config Release --parallel
```

Output: `code/build/Release/MeOS.exe`. Copy runtime DLLs from `code/dll64/` alongside the executable.

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

## CI/CD

GitHub Actions workflows run automatically on every push and pull request.

- **C++ CI** (`.github/workflows/cpp.yml`) — builds and tests on Linux and Windows, runs clang-tidy on Linux
- **Frontend CI** (`.github/workflows/frontend.yml`) — lint, test, and build the React frontend
