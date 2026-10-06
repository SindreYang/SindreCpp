#pragma once

#include <general/core/codec.hpp>
#include <general/core/async.hpp>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <random>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sindrecpp::general::temp {

class File {
public:
    File() = default;
    File(const File &) = delete;
    File &operator=(const File &) = delete;
    File(File &&other) noexcept : path_(std::move(other.path_)), remove_(std::exchange(other.remove_, false)) {}
    File &operator=(File &&other) noexcept {
        if (this != &other) { cleanup(); path_ = std::move(other.path_); remove_ = std::exchange(other.remove_, false); }
        return *this;
    }
    ~File() { cleanup(); }

    static ::sindrecpp::general::Result<File> create(std::string_view prefix = "sindrecpp-",
                                                     std::string_view suffix = ".tmp") noexcept {
#if defined(SINDRECPP_NO_EXCEPTIONS)
        File result;
        const auto directory = std::filesystem::temp_directory_path();
        std::random_device entropy;
        for (int attempt = 0; attempt != 32; ++attempt) {
            const auto name = std::string(prefix) + ::sindrecpp::general::codec::hex(
                (static_cast<std::uint64_t>(entropy()) << 32) ^ entropy() ^ static_cast<std::uint64_t>(attempt)) + std::string(suffix);
            result.path_ = directory / name;
#if defined(_WIN32)
            const auto handle = ::CreateFileW(result.path_.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                              CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
            if (handle != INVALID_HANDLE_VALUE) { ::CloseHandle(handle); result.remove_ = true; return ::sindrecpp::general::Result<File>::success(std::move(result)); }
#else
            const int handle = ::open(result.path_.string().c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
            if (handle >= 0) { ::close(handle); result.remove_ = true; return ::sindrecpp::general::Result<File>::success(std::move(result)); }
#endif
        }
        return ::sindrecpp::general::Result<File>::failure(
            std::make_error_code(std::errc::file_exists), "Cannot create unique temporary file", "temp.create");
    }
#else
        try {
            File result;
            const auto directory = std::filesystem::temp_directory_path();
            std::random_device entropy;
            for (int attempt = 0; attempt != 32; ++attempt) {
                const auto name = std::string(prefix) + ::sindrecpp::general::codec::hex(
                    (static_cast<std::uint64_t>(entropy()) << 32) ^ entropy() ^
                    static_cast<std::uint64_t>(attempt)) + std::string(suffix);
                result.path_ = directory / name;
#if defined(_WIN32)
                const auto native = result.path_.wstring();
                const auto handle = ::CreateFileW(native.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                                  CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
                if (handle != INVALID_HANDLE_VALUE) {
                    ::CloseHandle(handle);
                    result.remove_ = true;
                    return ::sindrecpp::general::Result<File>::success(std::move(result));
                }
#else
                const auto native = result.path_.string();
                const int handle = ::open(native.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
                if (handle >= 0) {
                    ::close(handle);
                    result.remove_ = true;
                    return ::sindrecpp::general::Result<File>::success(std::move(result));
                }
#endif
            }
            return ::sindrecpp::general::Result<File>::failure(
                std::make_error_code(std::errc::file_exists), "Cannot create unique temporary file", "temp.create");
        } catch (const std::exception &error) {
            return ::sindrecpp::general::Result<File>::failure(
                std::make_error_code(std::errc::io_error), error.what(), "temp.create");
        } catch (...) {
            return ::sindrecpp::general::Result<File>::failure(
                std::make_error_code(std::errc::io_error), "Unknown temporary file failure", "temp.create");
        }
    }
#endif

    const std::filesystem::path &path() const noexcept { return path_; }
    void keep() noexcept { remove_ = false; }

private:
    void cleanup() noexcept { if (remove_ && !path_.empty()) std::filesystem::remove(path_); }
    std::filesystem::path path_;
    bool remove_ = false;
};

} // namespace sindrecpp::general::temp
