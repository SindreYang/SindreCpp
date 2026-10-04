#pragma once

#include <sindrecpp/core.hpp>

#if defined(SINDRECPP_WITH_POINTER)
#include <sindrecpp/pointer.hpp>
#endif
#if defined(SINDRECPP_WITH_STRING)
#include <sindrecpp/string.hpp>
#endif
#if defined(SINDRECPP_WITH_LOG)
#include <sindrecpp/log.hpp>
#endif
#if defined(SINDRECPP_WITH_HTTP)
#include <sindrecpp/http.hpp>
#endif
#if defined(SINDRECPP_WITH_JSON)
#include <sindrecpp/json.hpp>
#endif
#if defined(SINDRECPP_WITH_CLI)
#include <sindrecpp/cli.hpp>
#endif

namespace sindrecpp::general {

// General-purpose modules are exposed through their focused namespaces.
// This header is the stable domain entry point.

} // namespace sindrecpp::general
