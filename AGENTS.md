# SindreCpp repository guidance

## Architecture

The public library is split into five modules: `General`, `AI`, `GUI`,
`Utils2d`, and `Utils3d`. Each module owns its public headers, optional source
files, CMake target, tests, and module documentation under `modules/<name>/`.

Public targets are `SindreCpp::General`, `SindreCpp::Ai`,
`SindreCpp::Gui`, `SindreCpp::Utils2d`, `SindreCpp::Utils3d`, and the aggregate
`SindreCpp::SindreCpp`. Do not reintroduce standalone public targets for old
integrations such as Json, Http, Log, or Utils_py; those are General options.

Every module exposes `include/sindrecpp/<module>/index.hpp`. Use that index for
module-level includes. General exposes purpose-oriented aggregates such as
`general/text.hpp`, `general/filesystem.hpp`, `general/network.hpp`, and
`general/runtime.hpp`; `general/core/` contains the reusable low-level APIs.

## Contracts

- C++17 is the baseline.
- Public operations return `sindrecpp::general::Result<T>` where failure can
  carry `code`, `message`, and `context`.
- Optional third-party integrations are enabled by CMake and their compile
  definitions must travel through the owning module target.
- AI execution must preserve cancellation, deadline/timeout, progress, and
  Result-based error semantics. Backend exceptions must not cross the public
  boundary.
- Keep platform-specific behavior inside General's implementation details and
  CMake; expose capabilities through purpose-oriented headers such as
  `general/filesystem.hpp`, `general/network.hpp`, and `general/host.hpp`.

## Build and test

Configure from the repository root. Enable only the modules required by the
consumer, for example `-DSINDRECPP_WITH_GENERAL=ON -DSINDRECPP_WITH_AI=ON`.
Prefer `cmake --preset windows-clang` or `cmake --preset linux-clang`; these use
`build_win`/`build_linux` and place generated files in the corresponding `bin/`.
`SINDRECPP_BUILD_TESTS` controls module tests; each module CMake file registers
tests with a `sindrecpp.<module>` prefix. AI runtime fixtures live in
`modules/ai/tests/models`.

On Windows, keep runtime DLLs beside the generated executable. Module CMake
files use the common runtime-copy helper; do not reintroduce system PATH-only
workarounds for a dependency that can be copied locally.

When changing public headers, build at least General and the changed module,
then run the registered tests. Do not infer AI typed-tensor support from a
successful model load alone; run int32, float16, int64, and bool fixtures when
the backend is available.

## Documentation rule

Before changing a module's public contract, update that module's `docs/` files
and its `AGENTS.md` if build or dependency behavior changes. Keep examples and
test names aligned with the module target names.
