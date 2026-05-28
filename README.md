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
cmake --preset default     # Configure (Debug) — also installs npm build dependency
cmake --build --preset default  # Builds C++ and runs React production build (if npm is available)
```

The CMake build automatically runs `npm run build` in `src/ui/web/` (if npm is found) and places the output in `src/ui/web/dist/`. The C++ server serves files from the `web/` directory at runtime; copy (or symlink) `src/ui/web/dist/` to `web/` next to the executable.

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

## Legacy MeOS (Windows GUI)

### Prerequisites

- Windows 10/11 x64
- CMake 3.20+
- Visual Studio 2019/2022 with C++ workload
- vcpkg (set `VCPKG_INSTALLATION_ROOT` to your vcpkg installation directory)

### Build

```powershell
cmake -S code -B code/build -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build code/build --config Release
```

vcpkg automatically installs all dependencies from `code/vcpkg.json` during configure:
`zlib`, `minizip`, `libpng`, `libharu`, `libmysql`, `openssl`, `restbed`.

Output: `code/build/Release/MeOS.exe`

The build automatically copies data files from `code/Lists/` to the output directory via a CMake POST_BUILD step.

### Runtime DLLs

All required DLLs are provided by vcpkg. Copy everything from
`code/build/vcpkg_installed/x64-windows/bin/` alongside `MeOS.exe` before running.

For redistribution, also include the MSVC runtime DLLs (`MSVCP140.dll`,
`VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`) from your Visual Studio installation.

## CI/CD

GitHub Actions workflows run automatically on every push and pull request.

- **C++ CI** (`.github/workflows/cpp.yml`) — builds and tests on Linux and Windows, runs clang-tidy on Linux
- **Frontend CI** (`.github/workflows/frontend.yml`) — lint, test, and build the React frontend
- **Legacy MeOS CI** (`.github/workflows/build-legacy.yml`) — builds the Windows GUI application using CMake + vcpkg on `legacy-build-*` branches
