#pragma once

#include <sindrecpp/core.hpp>
#include <sindrecpp/pointer.hpp>
#include <sindrecpp/string.hpp>

#if defined(SINDRECPP_WITH_LOG)
#include <sindrecpp/log.hpp>
#endif
#if defined(SINDRECPP_WITH_GUI)
#include <sindrecpp/gui.hpp>
#endif
#if defined(SINDRECPP_WITH_PYTHON)
#include <sindrecpp/python.hpp>
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
#if defined(SINDRECPP_WITH_MATH)
#include <sindrecpp/math.hpp>
#endif
