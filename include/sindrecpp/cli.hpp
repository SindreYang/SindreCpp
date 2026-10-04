#pragma once

#if !defined(SINDRECPP_WITH_CLI)
#error "Enable SINDRECPP_WITH_CLI and link SindreCpp::Cli before including this header."
#endif

#include <argparse/argparse.hpp>

namespace sindrecpp::general::cli {

using ArgumentParser = argparse::ArgumentParser;
namespace native = argparse;

} // namespace sindrecpp::general::cli
