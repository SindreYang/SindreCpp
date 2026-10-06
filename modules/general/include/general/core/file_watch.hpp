#pragma once

#include <general/core/async.hpp>
#include <general/core/path.hpp>

#include <chrono>
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace sindrecpp::general::file_watch {

struct Event {
    std::filesystem::path path;
    bool exists = false;
};

class Watcher {
    struct Snapshot {
        bool exists = false;
        std::filesystem::file_time_type modified{};
        std::uintmax_t size = 0;
        bool operator==(const Snapshot &other) const {
            return exists == other.exists && modified == other.modified && size == other.size;
        }
    };
    std::filesystem::path path_;
    std::chrono::milliseconds interval_;
    std::function<void(const Event &)> callback_;
    std::thread worker_;
    mutable std::mutex mutex_;
    std::atomic<bool> stopping_{false};
    std::optional<Error> error_;

    Snapshot snapshot() const noexcept {
        std::error_code error;
        Snapshot result;
        result.exists = std::filesystem::is_regular_file(path_, error);
        if (error || !result.exists) return result;
        result.modified = std::filesystem::last_write_time(path_, error);
        if (error) return Snapshot{};
        result.size = std::filesystem::file_size(path_, error);
        if (error) return Snapshot{};
        return result;
    }
    void loop() noexcept {
        auto previous = snapshot();
        while (!stopping_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(interval_);
            const auto current = snapshot();
            if (current == previous) continue;
            previous = current;
#if defined(SINDRECPP_NO_EXCEPTIONS)
            callback_(Event{path_, current.exists});
#else
            try {
                callback_(Event{path_, current.exists});
            } catch (const std::exception &error) {
                std::lock_guard<std::mutex> lock(mutex_);
                error_ = Error{std::make_error_code(std::errc::io_error), error.what(), "file_watch.callback"};
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex_);
                error_ = Error{std::make_error_code(std::errc::io_error), "Unknown watcher callback failure",
                               "file_watch.callback"};
            }
#endif
        }
    }

  public:
    explicit Watcher(std::filesystem::path path,
                     std::chrono::milliseconds interval = std::chrono::milliseconds(100))
        : path_(std::move(path)), interval_(interval) {}
    ~Watcher() { stop(); }
    Watcher(const Watcher &) = delete;
    Watcher &operator=(const Watcher &) = delete;

    ::sindrecpp::general::Result<void> start(std::function<void(const Event &)> callback) {
        if (!callback || interval_.count() <= 0)
            return ::sindrecpp::general::Result<void>::failure(
                std::make_error_code(std::errc::invalid_argument), "Invalid watcher options", "file_watch.start");
        std::lock_guard<std::mutex> lock(mutex_);
        if (worker_.joinable())
            return ::sindrecpp::general::Result<void>::failure(
                std::make_error_code(std::errc::device_or_resource_busy), "Watcher is already running",
                "file_watch.start");
        callback_ = std::move(callback);
        stopping_.store(false, std::memory_order_release);
#if defined(SINDRECPP_NO_EXCEPTIONS)
        worker_ = std::thread([this] { loop(); });
#else
        try {
            worker_ = std::thread([this] { loop(); });
        } catch (const std::exception &error) {
            return ::sindrecpp::general::Result<void>::failure(
                std::make_error_code(std::errc::resource_unavailable_try_again), error.what(), "file_watch.start");
        }
#endif
        return ::sindrecpp::general::Result<void>::success();
    }
    void stop() noexcept {
        stopping_.store(true, std::memory_order_release);
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) worker_.join();
    }
    std::optional<Error> error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_;
    }
};

} // namespace sindrecpp::general::file_watch
