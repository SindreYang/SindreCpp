#pragma once

#include <general/core/async.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <cstdint>
#include <iterator>
#include <string>

#if defined(SINDRECPP_WITH_HTTP)
#include <general/core/http.hpp>
#endif

namespace sindrecpp::general::transfer {

using Progress = std::function<void(std::uint64_t current, std::uint64_t total)>;

#if defined(SINDRECPP_WITH_HTTP)
inline ::sindrecpp::general::Result<std::uint64_t> download(
    std::string host, int port, std::string remote_path, const std::filesystem::path &destination,
    ::sindrecpp::general::http::RequestOptions options = {}, Progress progress = {}) noexcept {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    {
#else
    try {
#endif
        if (host.empty() || port < 1 || port > 65535 || remote_path.empty())
            return ::sindrecpp::general::Result<std::uint64_t>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid download request", "transfer.download");
        if (remote_path.front() != '/') remote_path.insert(remote_path.begin(), '/');
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) return ::sindrecpp::general::Result<std::uint64_t>::failure(
            std::make_error_code(std::errc::permission_denied), "Cannot open download destination", "transfer.download");
        ::sindrecpp::general::http::Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        std::uint64_t size = 0;
        auto response = client.Get(remote_path.c_str(), [&](const char *data, std::size_t length) {
            if (options.token.cancelled()) return false;
            output.write(data, static_cast<std::streamsize>(length));
            if (!output) return false;
            size += static_cast<std::uint64_t>(length);
            if (progress) progress(size, 0);
            return true;
        });
        if (!response) return ::sindrecpp::general::Result<std::uint64_t>::failure(
            std::make_error_code(std::errc::connection_refused), "HTTP download failed", "transfer.download");
        if (response->status < 200 || response->status >= 300)
            return ::sindrecpp::general::Result<std::uint64_t>::failure(
                std::make_error_code(std::errc::protocol_error), "HTTP download failed", "transfer.download");
        if (progress) progress(size, size);
        return ::sindrecpp::general::Result<std::uint64_t>::success(size);
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<std::uint64_t>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "transfer.download");
    }
#else
    }
#endif
}

inline ::sindrecpp::general::Result<int> upload(
    std::string host, int port, std::string remote_path, const std::filesystem::path &source,
    std::string content_type = "application/octet-stream",
    ::sindrecpp::general::http::RequestOptions options = {}, Progress progress = {}) noexcept {
#if defined(SINDRECPP_NO_EXCEPTIONS)
    {
#else
    try {
#endif
        std::ifstream input(source, std::ios::binary);
        if (!input) return ::sindrecpp::general::Result<int>::failure(
            std::make_error_code(std::errc::no_such_file_or_directory), "Cannot open upload source", "transfer.upload");
        std::error_code file_error;
        const auto file_size = std::filesystem::file_size(source, file_error);
        if (file_error) return ::sindrecpp::general::Result<int>::failure(
            file_error, "Cannot determine upload size", "transfer.upload");
        const auto total = static_cast<std::uint64_t>(file_size);
        input.close();
        if (progress) progress(0, total);
        if (remote_path.empty() || remote_path.front() != '/') remote_path.insert(remote_path.begin(), '/');
        ::sindrecpp::general::http::Client client(host, port);
        client.set_connection_timeout(options.connect_timeout_seconds);
        client.set_read_timeout(options.read_timeout_seconds);
        client.set_write_timeout(options.write_timeout_seconds);
        auto response = client.Post(remote_path.c_str(), static_cast<std::size_t>(total),
            [&](std::size_t offset, std::size_t length, ::httplib::DataSink &sink) {
                if (options.token.cancelled()) return false;
                std::ifstream file(source, std::ios::binary);
                if (!file) return false;
                file.seekg(static_cast<std::streamoff>(offset));
                std::string chunk(length, '\0');
                file.read(chunk.data(), static_cast<std::streamsize>(length));
                const auto read = static_cast<std::size_t>(file.gcount());
                if (read == 0 && offset < total) return false;
                sink.write(chunk.data(), read);
                if (progress) progress(static_cast<std::uint64_t>(offset + read), total);
                return true;
            }, content_type.c_str());
        if (!response) return ::sindrecpp::general::Result<int>::failure(
            std::make_error_code(std::errc::connection_refused), "HTTP upload failed", "transfer.upload");
        if (response->status < 200 || response->status >= 300)
            return ::sindrecpp::general::Result<int>::failure(
                std::make_error_code(std::errc::protocol_error), "HTTP upload failed", "transfer.upload");
        if (progress) progress(total, total);
        return ::sindrecpp::general::Result<int>::success(response->status);
#if !defined(SINDRECPP_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return ::sindrecpp::general::Result<int>::failure(
            std::make_error_code(std::errc::io_error), error.what(), "transfer.upload");
    }
#else
    }
#endif
}
#else
inline ::sindrecpp::general::Result<std::uint64_t> download(
    std::string, int, std::string, const std::filesystem::path &, Progress = {}) noexcept {
    return ::sindrecpp::general::Result<std::uint64_t>::failure(
        std::make_error_code(std::errc::function_not_supported), "HTTP support is not enabled", "transfer.download");
}
#endif

} // namespace sindrecpp::general::transfer
