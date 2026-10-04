#pragma once

#if !defined(SINDRECPP_WITH_HTTP)
#error "Enable SINDRECPP_WITH_HTTP and link SindreCpp::Http before including this header."
#endif

#include <httplib.h>

namespace sindrecpp::general::http {

using Client = httplib::Client;
using Server = httplib::Server;
using Request = httplib::Request;
using Response = httplib::Response;
using Result = httplib::Result;
namespace native = httplib;

} // namespace sindrecpp::general::http
