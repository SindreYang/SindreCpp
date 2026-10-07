#include <sindre/ai/execution.h>

namespace sindre::ai::detail {

Executor::Executor(std::size_t capacity) : capacity_(capacity) {
    if (!capacity) throw std::invalid_argument("Queue capacity must be positive");
    worker_ = std::thread([this] { loop(); });
}

Executor::~Executor() {
    close();
}

void Executor::close() noexcept {
    std::lock_guard<std::mutex> join_lock(join_mutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }
    condition_.notify_all();
    // Destroying an executor from its own callback is a programming error.
    if (worker_.joinable()) {
        if (worker_.get_id() == std::this_thread::get_id()) std::terminate();
        worker_.join();
    }
}

void Executor::loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return closed_ || !tasks_.empty(); });
            if (tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task(); // packaged_task stores exceptions.
    }
}

} // namespace sindre::ai::detail
