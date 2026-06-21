# Testing

X-TODO has one automated test target today:

```text
xtodo_model_tests
```

It covers the pure `TodoModel` layer: active and completed partitioning,
multi-level item trees, subtree completion and restore, drag reordering,
multi-list isolation, list rename, list delete, legacy import normalization,
and invalid-index no-op behavior.

## Run with CMake

The desktop app is Win32 and Direct2D only. The model tests do not depend on
Win32 APIs, so they can run on Windows and Linux.

```bash
cmake -S . -B build-tests -DXTODO_BUILD_APP=OFF -DXTODO_BUILD_TESTS=ON
cmake --build build-tests --config Release --parallel
ctest --test-dir build-tests --output-on-failure -C Release
```

`XTODO_BUILD_TESTS` defaults to `OFF`, so a normal app build keeps the existing
behavior:

```powershell
cmake -B build -A x64
cmake --build build --config MinSizeRel
```

On non-Windows systems, `XTODO_BUILD_APP` is forced off because the app target
links Win32, Direct2D, and DirectWrite libraries.

## Minimal Local Fallback

If CMake is not available, the model tests can still be compiled directly with
a C++17 compiler:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc \
  tests/todo_model_tests.cpp src/TodoModel.cpp \
  -o /tmp/xtodo_model_tests
/tmp/xtodo_model_tests
```

This fallback verifies the same executable test source. It does not verify
CMake wiring.

## CI Coverage

The `build` workflow runs `xtodo_model_tests` on `ubuntu-latest` and
`windows-latest`. The release path waits for both the model tests and the
Windows app build before creating tags or publishing release artifacts.
