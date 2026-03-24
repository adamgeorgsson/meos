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

## Legacy MeOS (Windows GUI)

### Prerequisites

- Windows 10/11 x64
- CMake 3.20+
- Visual Studio 2019/2022 with C++ workload
- OpenSSL 1.1 (install via `choco install openssl --version=1.1.1.2100`)

### Build

```powershell
cmake -S code -B code/build -A x64
cmake --build code/build --config Release
```

Output: `code/build/Release/MeOS.exe`

The build automatically copies data files from `code/Lists/` to the output directory via a CMake POST_BUILD step.

### Runtime DLLs

Copy the following alongside `MeOS.exe` before running:
- `code/dll64/libharu.dll`
- `code/dll64/libmysql.dll`
- OpenSSL: `libssl-1_1-x64.dll`, `libcrypto-1_1-x64.dll`

## CI/CD

GitHub Actions workflows run automatically on every push and pull request.

- **C++ CI** (`.github/workflows/cpp.yml`) — builds and tests on Linux and Windows, runs clang-tidy on Linux
- **Frontend CI** (`.github/workflows/frontend.yml`) — lint, test, and build the React frontend
