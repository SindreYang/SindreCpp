#pragma once

#include <general/core/async.hpp>
#include <general/core/pointer.hpp>
#include <general/core/string.hpp>
#include <general/core/codec.hpp>
#include <general/core/ranges.hpp>
#include <general/core/scopeguard.hpp>
#include <general/core/version.hpp>

#if defined(SINDRECPP_WITH_LOG)
#include <general/core/log.hpp>
#endif
#if defined(SINDRECPP_WITH_HTTP)
#include <general/core/http.hpp>
#endif
#if defined(SINDRECPP_WITH_JSON)
#include <general/core/json.hpp>
#include <general/core/config.hpp>
#endif
#if defined(SINDRECPP_WITH_CLI)
#include <general/core/cli.hpp>
#endif
#if defined(SINDRECPP_WITH_RE2)
#include <general/core/re2.hpp>
#endif
#if defined(SINDRECPP_WITH_CRASHPAD)
#include <general/core/crashpad.hpp>
#endif
#if defined(SINDRECPP_WITH_UTILS_PY)
#include <general/core/python.hpp>
#endif
