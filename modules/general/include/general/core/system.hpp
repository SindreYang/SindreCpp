#pragma once

#include <general/core/async.hpp>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <cstdint>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace sindrecpp::general::system {

inline ::sindrecpp::general::Result<std::string> environment(std::string_view name) noexcept {
    if (name.empty()) return ::sindrecpp::general::Result<std::string>::failure(
        std::make_error_code(std::errc::invalid_argument), "Environment name is empty", "system.environment");
#if defined(_WIN32)
    char *value = nullptr; std::size_t size = 0;
    if (_dupenv_s(&value, &size, std::string(name).c_str()) != 0 || !value)
        return ::sindrecpp::general::Result<std::string>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Environment variable is missing", "system.environment");
    std::string result(value, size ? size - 1 : 0); std::free(value);
    return ::sindrecpp::general::Result<std::string>::success(std::move(result));
#else
    if (const char *value = std::getenv(std::string(name).c_str()))
        return ::sindrecpp::general::Result<std::string>::success(value);
    return ::sindrecpp::general::Result<std::string>::failure(
        std::make_error_code(std::errc::no_such_file_or_directory), "Environment variable is missing", "system.environment");
#endif
}

inline ::sindrecpp::general::Result<void> set_environment(std::string_view name, std::string_view value) noexcept {
    if (name.empty()) return ::sindrecpp::general::Result<void>::failure(
        std::make_error_code(std::errc::invalid_argument), "Environment name is empty", "system.set_environment");
#if defined(_WIN32)
    if (_putenv_s(std::string(name).c_str(), std::string(value).c_str()) != 0)
#else
    if (::setenv(std::string(name).c_str(), std::string(value).c_str(), 1) != 0)
#endif
        return ::sindrecpp::general::Result<void>::failure(
            std::make_error_code(std::errc::permission_denied), "Cannot set environment variable", "system.set_environment");
    return ::sindrecpp::general::Result<void>::success();
}

struct Information {
    std::string os;
    std::string architecture;
    std::string compiler;
    std::uint32_t cpu_count = 0;
    std::uint64_t memory_bytes = 0;
};
inline Information information() {
    Information result;
#if defined(_WIN32)
    result.os = "Windows";
#elif defined(__linux__)
    result.os = "Linux";
#elif defined(__APPLE__)
    result.os = "macOS";
#else
    result.os = "Unknown";
#endif
#if defined(_M_X64) || defined(__x86_64__)
    result.architecture = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    result.architecture = "arm64";
#else
    result.architecture = "unknown";
#endif
#if defined(__clang__)
    result.compiler = "clang";
#elif defined(_MSC_VER)
    result.compiler = "msvc";
#elif defined(__GNUC__)
    result.compiler = "gcc";
#else
    result.compiler = "unknown";
#endif
    result.cpu_count = std::thread::hardware_concurrency();
#if defined(_WIN32)
    MEMORYSTATUSEX memory{}; memory.dwLength = sizeof(memory);
    if (::GlobalMemoryStatusEx(&memory)) result.memory_bytes = memory.ullTotalPhys;
#else
    const auto pages = ::sysconf(_SC_PHYS_PAGES);
    const auto page_size = ::sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) result.memory_bytes = static_cast<std::uint64_t>(pages) * page_size;
#endif
    return result;
}

} // namespace sindrecpp::general::system
