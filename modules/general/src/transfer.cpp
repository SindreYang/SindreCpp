#include <sindre/general/network.h>

#include <atomic>
#include <fstream>
#include <limits>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace sindre::general::network {
namespace {

template <class T>
Result<T> failure(NetworkErrc code, std::string message, std::string context) {
    return Result<T>::failure(make_error_code(code), std::move(message), std::move(context));
}

std::filesystem::path make_temporary_path(const std::filesystem::path &destination) {
    static std::atomic<std::uint64_t> counter{0};
    const auto parent = destination.parent_path().empty() ? std::filesystem::path(".") : destination.parent_path();
    const auto name = destination.filename().wstring() + L".sindre-download-" +
        std::to_wstring(++counter) + L".part";
    return parent / name;
}

bool replace_file(const std::filesystem::path &temporary, const std::filesystem::path &destination,
                  std::error_code &error) noexcept {
#if defined(_WIN32)
    if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) return true;
    error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
    return false;
#else
    std::filesystem::rename(temporary, destination, error);
    return !error;
#endif
}

class TemporaryFileCleanup {
public:
    explicit TemporaryFileCleanup(std::filesystem::path path) : path_(std::move(path)) {}
    ~TemporaryFileCleanup() {
        if (!keep_) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }
    void keep() noexcept { keep_ = true; }
private:
    std::filesystem::path path_;
    bool keep_ = false;
};

} // namespace

Result<std::uint64_t> download(const Url &target,
                                   const std::filesystem::path &destination,
                                   TransferOptions options) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        if (destination.empty()) return failure<std::uint64_t>(
            NetworkErrc::invalid_request, "Download destination is empty", "network.download");
        const auto temporary = make_temporary_path(destination);
        TemporaryFileCleanup cleanup(temporary);
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return failure<std::uint64_t>(
            NetworkErrc::file_io_failed, "Cannot open download temporary file", "network.download");

        std::uint64_t received = 0;
        std::uint64_t total_hint = 0;
        bool file_write_failed = false;
        bool callback_failed = false;
        auto request_options = options.request;
        request_options.on_body = [&](const char *data, std::size_t length) {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                if (request_options.token.is_cancelled()) return false;
                output.write(data, static_cast<std::streamsize>(length));
                if (!output) {
                    file_write_failed = true;
                    return false;
                }
                received += static_cast<std::uint64_t>(length);
                if (options.progress) {
#if !defined(SINDRE_NO_EXCEPTIONS)
                    try {
#endif
                        options.progress(received, total_hint);
#if !defined(SINDRE_NO_EXCEPTIONS)
                    } catch (...) {
                        callback_failed = true;
                        return false;
                    }
#endif
                }
                return true;
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (...) {
                file_write_failed = true;
                return false;
            }
#endif
        };
        request_options.on_progress = [&](std::uint64_t current, std::uint64_t total) {
            total_hint = total;
            if (options.request.on_progress) {
#if !defined(SINDRE_NO_EXCEPTIONS)
                try {
#endif
                    options.request.on_progress(current, total);
#if !defined(SINDRE_NO_EXCEPTIONS)
                } catch (...) {
                    callback_failed = true;
                    return;
                }
#endif
            }
        };
        auto response = get(target, std::move(request_options));
        output.flush();
        if (!output) file_write_failed = true;
        output.close();
        if (!response) {
            if (file_write_failed) return failure<std::uint64_t>(
                NetworkErrc::file_io_failed, "Writing download temporary file failed", "network.download");
            if (callback_failed) return failure<std::uint64_t>(
                NetworkErrc::callback_failed, "Download callback failed", "network.download");
            return Result<std::uint64_t>::failure(response.error().with_context("network.download"));
        }
        if (file_write_failed) return failure<std::uint64_t>(
            NetworkErrc::file_io_failed, "Writing download temporary file failed", "network.download");
        if (callback_failed) return failure<std::uint64_t>(
            NetworkErrc::callback_failed, "Download callback failed", "network.download");
        auto success = response.value().require_success();
        if (!success) return Result<std::uint64_t>::failure(success.error().with_context("network.download"));
        std::error_code replace_error;
        if (!replace_file(temporary, destination, replace_error)) return failure<std::uint64_t>(
            NetworkErrc::file_io_failed, "Cannot atomically replace download destination: " + replace_error.message(), "network.download");
        cleanup.keep();
        const auto total = response.value().content_length.value_or(0);
        if (options.progress) {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                options.progress(received, total);
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (...) {
                return failure<std::uint64_t>(NetworkErrc::callback_failed, "Download progress callback failed", "network.download");
            }
#endif
        }
        return Result<std::uint64_t>::success(received);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::uint64_t>(NetworkErrc::file_io_failed, error.what(), "network.download");
    } catch (...) {
        return failure<std::uint64_t>(NetworkErrc::file_io_failed, "Unknown download failure", "network.download");
    }
#endif
}

Result<std::uint64_t> download(const Url &target,
                               const std::filesystem::path &destination,
                               Progress progress,
                               std::chrono::milliseconds timeout,
                               CancellationToken token) noexcept {
    TransferOptions options;
    options.progress = std::move(progress);
    options.request.timeout.total = timeout;
    options.request.token = std::move(token);
    return download(target, destination, std::move(options));
}

Result<int> upload(const Url &target,
                       const std::filesystem::path &source,
                       std::string content_type,
                       TransferOptions options) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        std::error_code file_error;
        const auto file_size = std::filesystem::file_size(source, file_error);
        if (file_error) return failure<int>(NetworkErrc::file_io_failed, "Cannot determine upload file size", "network.upload");
        if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
            return failure<int>(NetworkErrc::file_io_failed, "Upload file is too large for this platform", "network.upload");
        std::ifstream input(source, std::ios::binary);
        if (!input) return failure<int>(NetworkErrc::file_io_failed, "Cannot open upload source", "network.upload");
        const auto total = static_cast<std::uint64_t>(file_size);
        if (options.progress) {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                options.progress(0, total);
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (...) {
                return failure<int>(NetworkErrc::callback_failed, "Upload progress callback failed", "network.upload");
            }
#endif
        }
        Request request;
        request.method = Method::post;
        request.target = target;
        request.headers.push_back({"Content-Type", std::move(content_type)});
        request.body_size = total;
        request.body_provider = [&](std::uint64_t offset, std::size_t length) -> Result<std::string> {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                if (options.request.token.is_cancelled()) return failure<std::string>(
                    NetworkErrc::operation_cancelled, "Upload cancelled", "network.upload");
                input.clear();
                input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
                if (!input) return failure<std::string>(NetworkErrc::file_io_failed, "Cannot seek upload source", "network.upload");
                std::string chunk(length, '\0');
                input.read(chunk.data(), static_cast<std::streamsize>(length));
                const auto read = static_cast<std::size_t>(input.gcount());
                if (read != length) return failure<std::string>(NetworkErrc::file_io_failed, "Cannot read upload source", "network.upload");
                if (options.progress) {
#if !defined(SINDRE_NO_EXCEPTIONS)
                    try {
#endif
                        options.progress(offset + read, total);
#if !defined(SINDRE_NO_EXCEPTIONS)
                    } catch (...) {
                        return failure<std::string>(NetworkErrc::callback_failed, "Upload progress callback failed", "network.upload");
                    }
#endif
                }
                return Result<std::string>::success(std::move(chunk));
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (const std::exception &error) {
                return failure<std::string>(NetworkErrc::file_io_failed, error.what(), "network.upload");
            } catch (...) {
                return failure<std::string>(NetworkErrc::file_io_failed, "Unknown upload read failure", "network.upload");
            }
#endif
        };
        auto response = Client().request(request, options.request);
        if (!response) return Result<int>::failure(response.error().with_context("network.upload"));
        auto success = response.value().require_success();
        if (!success) return Result<int>::failure(success.error().with_context("network.upload"));
        if (options.progress) {
#if !defined(SINDRE_NO_EXCEPTIONS)
            try {
#endif
                options.progress(total, total);
#if !defined(SINDRE_NO_EXCEPTIONS)
            } catch (...) {
                return failure<int>(NetworkErrc::callback_failed, "Upload progress callback failed", "network.upload");
            }
#endif
        }
        return Result<int>::success(response.value().status);
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<int>(NetworkErrc::file_io_failed, error.what(), "network.upload");
    } catch (...) {
        return failure<int>(NetworkErrc::file_io_failed, "Unknown upload failure", "network.upload");
    }
#endif
}

Result<int> upload(const Url &target,
                   const std::filesystem::path &source,
                   std::string content_type,
                   Progress progress,
                   std::chrono::milliseconds timeout,
                   CancellationToken token) noexcept {
    TransferOptions options;
    options.progress = std::move(progress);
    options.request.timeout.total = timeout;
    options.request.token = std::move(token);
    return upload(target, source, std::move(content_type), std::move(options));
}

Result<std::future<Result<std::uint64_t>>> download_async(
    Url target, std::filesystem::path destination, TransferOptions options) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto future = std::async(std::launch::async,
            [target = std::move(target), destination = std::move(destination), options = std::move(options)]() mutable {
                return download(target, destination, std::move(options));
            });
        return Result<std::future<Result<std::uint64_t>>>::success(std::move(future));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::future<Result<std::uint64_t>>>(NetworkErrc::connection_failed, error.what(), "network.download_async");
    } catch (...) {
        return failure<std::future<Result<std::uint64_t>>>(NetworkErrc::connection_failed, "Unable to start async download", "network.download_async");
    }
#endif
}

Result<std::future<Result<int>>> upload_async(
    Url target, std::filesystem::path source, std::string content_type, TransferOptions options) noexcept {
#if !defined(SINDRE_NO_EXCEPTIONS)
    try {
#endif
        auto future = std::async(std::launch::async,
            [target = std::move(target), source = std::move(source), content_type = std::move(content_type), options = std::move(options)]() mutable {
                return upload(target, source, std::move(content_type), std::move(options));
            });
        return Result<std::future<Result<int>>>::success(std::move(future));
#if !defined(SINDRE_NO_EXCEPTIONS)
    } catch (const std::exception &error) {
        return failure<std::future<Result<int>>>(NetworkErrc::connection_failed, error.what(), "network.upload_async");
    } catch (...) {
        return failure<std::future<Result<int>>>(NetworkErrc::connection_failed, "Unable to start async upload", "network.upload_async");
    }
#endif
}

} // namespace sindre::general::network
