#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace sindre::ai {

namespace detail {

/// @brief 带容量上限的单工作线程 FIFO 执行器。
///
/// 关闭时会排空已经接受的任务，任务异常通过 future 传递。
class Executor {
public:
    explicit Executor(std::size_t capacity = 16);
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;
    ~Executor();

    template <class Function>
    auto submit(Function function) -> std::future<std::invoke_result_t<Function>> {
        using Result = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::move(function));
        auto future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) throw std::runtime_error("Executor is closed");
            if (tasks_.size() >= capacity_) throw std::runtime_error("Executor queue is full");
            tasks_.push([task] { (*task)(); });
        }
        condition_.notify_one();
        return future;
    }
    void close() noexcept;

private:
    void loop();
    std::size_t capacity_;
    bool closed_ = false;
    std::mutex mutex_, join_mutex_;
    std::condition_variable condition_;
    std::queue<std::function<void()>> tasks_;
    std::thread worker_;
};
} // namespace detail

/// @brief 用两个独立工作线程重叠预处理和推理阶段。
///
/// Input、Prepared 和 Output 均按值传递；回调需要模型所有权时应显式捕获 shared_ptr。
template <class Input, class Prepared, class Output>
class Pipeline {
public:
    using Prepare = std::function<Prepared(Input)>;
    using Infer = std::function<Output(Prepared)>;
    Pipeline(Prepare prepare, Infer infer, std::size_t max_in_flight = 16)
        : prepare_(std::move(prepare)), infer_(std::move(infer)),
          capacity_(max_in_flight), preprocess_(max_in_flight), inference_(max_in_flight) {
        if (!prepare_ || !infer_) throw std::invalid_argument("Pipeline callbacks are required");
    }
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    ~Pipeline() { close(); }

    std::future<Output> infer_async(Input input) {
        auto promise = std::make_shared<std::promise<Output>>();
        auto future = promise->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) throw std::runtime_error("Pipeline is closed");
            if (in_flight_ >= capacity_) throw std::runtime_error("Pipeline is full");
            ++in_flight_;
            // Keep acceptance and submission atomic with close().
            try {
                (void)preprocess_.submit([this, promise, input = std::move(input)]() mutable {
                    try {
                        auto prepared = prepare_(std::move(input));
                        (void)inference_.submit([this, promise, prepared = std::move(prepared)]() mutable {
                            bool released = false;
                            try {
                                auto output = infer_(std::move(prepared));
                                finish();
                                released = true;
                                promise->set_value(std::move(output));
                            } catch (...) {
                                auto error = std::current_exception();
                                if (!released) finish();
                                promise->set_exception(error);
                            }
                        });
                    } catch (...) {
                        auto error = std::current_exception();
                        finish();
                        promise->set_exception(error);
                    }
                });
            } catch (...) {
                --in_flight_;
                throw;
            }
        }
        return future;
    }
    Output infer(Input input) { return infer_async(std::move(input)).get(); }
    void close() noexcept {
        std::lock_guard<std::mutex> join_lock(join_mutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        preprocess_.close(); // All accepted preparation tasks submit stage 2 first.
        inference_.close();
    }
private:
    void finish() {
        std::lock_guard<std::mutex> lock(mutex_);
        --in_flight_;
    }
    Prepare prepare_;
    Infer infer_;
    std::size_t capacity_, in_flight_ = 0;
    bool closed_ = false;
    std::mutex mutex_, join_mutex_;
    detail::Executor preprocess_, inference_;
};

} // namespace sindre::ai
