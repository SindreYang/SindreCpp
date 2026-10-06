# General module guidance

General is the foundation module. It owns Result/Error, cancellation and
async/concurrency primitives in `include/general/core/async.hpp`.
`general/index.hpp` is the complete General aggregate. Users should prefer the
purpose-oriented aggregates: `utility.hpp`, `text.hpp`, `data.hpp`,
`serialization.hpp`, `filesystem.hpp`, `network.hpp`, `runtime.hpp`,
`host.hpp`, `observability.hpp`, and `bindings.hpp`.

The old `platform.hpp` and `deploy.hpp` classifications are intentionally gone.
Operating-system differences belong in implementation details and CMake; URL,
transfer, Crashpad, and diagnostics are grouped by the user task they serve.

Do not include old root paths such as `sindrecpp/json.hpp` or
`sindrecpp/general.hpp`; use `general/core/...` or
`general/index.hpp`. JSON/HTTP/CLI/Log/Re2/Crashpad/Python are
General options and must remain unavailable when their option is disabled.
