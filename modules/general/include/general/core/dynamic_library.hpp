#pragma once

#include <general/core/async.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace sindrecpp::general::dynamic_library {

class Library {
public:
    Library() = default;
    Library(const Library &) = delete;
    Library &operator=(const Library &) = delete;
    Library(Library &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Library &operator=(Library &&other) noexcept {
        if (this != &other) { close(); handle_ = std::exchange(other.handle_, nullptr); }
        return *this;
    }
    ~Library() { close(); }

    static ::sindrecpp::general::Result<Library> open(const std::filesystem::path &path) noexcept {
        Library result;
#if defined(_WIN32)
        result.handle_ = ::LoadLibraryW(path.c_str());
        if (!result.handle_)
#else
        result.handle_ = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!result.handle_)
#endif
            return ::sindrecpp::general::Result<Library>::failure(
                std::make_error_code(std::errc::no_such_file_or_directory), "Cannot load dynamic library",
                "dynamic_library.open");
        return ::sindrecpp::general::Result<Library>::success(std::move(result));
    }

    template <class Function>
    ::sindrecpp::general::Result<Function *> symbol(std::string_view name) const noexcept {
        if (!handle_ || name.empty())
            return ::sindrecpp::general::Result<Function *>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid library or symbol",
                "dynamic_library.symbol");
#if defined(_WIN32)
        auto value = ::GetProcAddress(handle_, std::string(name).c_str());
#else
        auto value = ::dlsym(handle_, std::string(name).c_str());
#endif
        if (!value)
            return ::sindrecpp::general::Result<Function *>::failure(
                std::make_error_code(std::errc::function_not_supported), "Symbol not found",
                "dynamic_library.symbol");
        return ::sindrecpp::general::Result<Function *>::success(reinterpret_cast<Function *>(value));
    }

private:
    void close() noexcept {
        if (!handle_) return;
#if defined(_WIN32)
        ::FreeLibrary(handle_);
#else
        ::dlclose(handle_);
#endif
        handle_ = nullptr;
    }
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void *handle_ = nullptr;
#endif
};

} // namespace sindrecpp::general::dynamic_library
