#include <sindre/ai/execution.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    using namespace sindre::ai;
    using namespace std::chrono_literals;
    try {
        detail::Executor executor(8);
        std::vector<std::future<int>> accepted;
        for (int i = 0; i < 8; ++i) accepted.push_back(executor.submit([i] { return i * 2; }));
        executor.close();
        for (int i = 0; i < 8; ++i) if (accepted[i].get() != i * 2) return 1;
        bool rejected = false;
        try { (void)executor.submit([] {}); } catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) return 2;

        std::promise<void> release, started, prepared;
        auto gate = release.get_future().share();
        auto begun = started.get_future(), second_prepared = prepared.get_future();
        std::atomic<int> preparation{0}, inference{0};
        Pipeline<int, int, int> pipeline(
            [&](int value) { if (++preparation == 2) prepared.set_value(); return value + 1; },
            [&](int value) {
                if (++inference == 1) { started.set_value(); gate.wait(); }
                return value * 2;
            }, 2);
        auto first = pipeline.infer_async(1);
        if (begun.wait_for(5s) != std::future_status::ready) { release.set_value(); return 3; }
        auto second = pipeline.infer_async(2);
        if (second_prepared.wait_for(5s) != std::future_status::ready) { release.set_value(); return 4; }
        rejected = false;
        try { (void)pipeline.infer_async(3); } catch (const std::runtime_error&) { rejected = true; }
        release.set_value();
        if (!rejected || first.get() != 4 || second.get() != 6) return 5;
        if (pipeline.infer(4) != 10) return 6;
        pipeline.close();
        rejected = false;
        try { (void)pipeline.infer_async(1); } catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) return 7;

        Pipeline<int, int, int> errors(
            [](int x) { if (x < 0) throw std::invalid_argument("prepare"); return x; },
            [](int x) { if (x == 0) throw std::runtime_error("infer"); return x; });
        auto bad_prepare = errors.infer_async(-1), bad_infer = errors.infer_async(0);
        rejected = false;
        try { (void)bad_prepare.get(); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) return 8;
        rejected = false;
        try { (void)bad_infer.get(); } catch (const std::runtime_error&) { rejected = true; }
        if (!rejected || errors.infer(5) != 5) return 9;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 10;
    }
}
