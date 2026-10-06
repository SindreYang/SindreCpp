# General dependencies

General is the foundation module. Its dependency-free Result, text, path,
filesystem and async APIs do not require any entry in this directory.

| Feature | Dependency | Source | Version | Default |
| --- | --- | --- | --- | --- |
| pointer | [CsPointer](https://github.com/copperspice/cs_pointer) | Git | `pointer-1.0.1` | off |
| string | [CsString](https://github.com/copperspice/cs_string) | Git | `string-1.4.1` | off |
| logging | [spdlog](https://github.com/gabime/spdlog) | Git | `v1.17.0` | off |
| HTTP | [cpp-httplib](https://github.com/yhirose/cpp-httplib) | Git | `v0.56.0` | off |
| JSON/config | [simdjson](https://github.com/simdjson/simdjson) | Git | `v4.6.11` | off |
| CLI | [argparse](https://github.com/p-ranav/argparse) | Git | `v3.2` | off |
| Python | [pybind11](https://github.com/pybind/pybind11) | Git | `v3.1.0` | off |

RE2, Crashpad, zlib and Python development files are package/SDK dependencies;
they are not downloaded automatically by SindreCpp.
