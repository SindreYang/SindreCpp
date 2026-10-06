# General module

## Public surface

- `general/index.hpp`: complete General aggregate.
- `general/utility.hpp`: pointer, scope guard, and ranges helpers.
- `general/text.hpp`: string and optional RE2 helpers.
- `general/data.hpp`: codec and version helpers.
- `general/serialization.hpp`: JSON, configuration, and CLI helpers.
- `general/filesystem.hpp`: paths, file watching, and temporary files.
- `general/network.hpp`: URL, HTTP, upload, and download helpers.
- `general/runtime.hpp`: async, process, and dynamic-library helpers.
- `general/host.hpp`: system, desktop, and startup helpers.
- `general/observability.hpp`: diagnostics, logging, and Crashpad.
- `general/bindings.hpp`: optional Python bindings.

## CMake

Link `SindreCpp::General`. Optional dependencies are enabled with
`SINDRECPP_WITH_LOG`, `SINDRECPP_WITH_HTTP`, `SINDRECPP_WITH_JSON`,
`SINDRECPP_WITH_CLI`, `SINDRECPP_WITH_RE2`, `SINDRECPP_WITH_CRASHPAD`,
`SINDRECPP_WITH_ZLIB`, and `SINDRECPP_WITH_UTILS_PY`.

## Error policy

Public fallible operations return `Result<T>`; inspect the result before using
`value()`. Preserve the original error code and add a stable context such as
`config.file` or `transfer.download` when adapting an error.

## Maintenance

Keep core headers independent of disabled integrations. Add focused tests to
`modules/general/tests/general_tests.cpp` or a new module-local test target.

The public aggregates are the stable discovery surface. `general/core/*.hpp`
remain lower-level headers for implementation and focused tests; new features
should be assigned to a user-purpose aggregate instead of creating another
generic category.
