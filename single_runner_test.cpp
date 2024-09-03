#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <sys/wait.h>
#include <thread>

#include "single_runner.h"

const int testIterations = 10;

void test_runner_can_be_instantiated() {
    {
        SingleRunner runner;
        assert(SingleRunner::running());
        assert(!SingleRunner::stopping());
    }
    assert(!SingleRunner::stopping());
    assert(!SingleRunner::running());
    std::cout << "test_runner_can_be_instantiated passed\n";
}

std::atomic_int activeRunners{0};
class Runner {
public:
    static const int manyIterations = 10'000;
    Runner() {
        thread = std::thread([this] {
            SingleRunner runner;
            assert(++activeRunners == 1);
            ++counter;
            // Check for stop every once in a while, but make sure to always count something,
            // so we can check that the runner is actually running.
            while (!SingleRunner::stopping()) {
                do {
                } while (++counter % manyIterations);
            }
            assert(!SingleRunner::running());
            assert(--activeRunners == 0);
        });
    }
    ~Runner() { thread.join(); }

    uint64_t count() const { return counter.load(); }

private:
    std::atomic_uint64_t counter = 0;
    std::thread thread;
};

void waitUntilCounting(Runner& runner) {
    while (!runner.count()) {
        std::this_thread::yield();
    }
}

void waitUntilCountingManyMore(Runner& runner) {
    auto count = runner.count() + Runner::manyIterations;
    while (runner.count() < count) {
        std::this_thread::yield();
    }
}

void test_runner_can_be_instantiated_and_stopped() {
    {
        SingleRunner runner;
        assert(SingleRunner::running());
        SingleRunner::stopNoWait();
        assert(SingleRunner::stopping());
        assert(!SingleRunner::running());
    }
    assert(!SingleRunner::stopping());
    assert(!SingleRunner::running());
    std::cout << "test_runner_can_be_instantiated_and_stopped passed\n";
}

void test_runner_can_throw() {
    try {
        SingleRunner runner;
        assert(SingleRunner::running());
        SingleRunner::stopNoWait();
        assert(SingleRunner::stopping());
        assert(!SingleRunner::running());
        SingleRunner::checkStop();
        assert(false);
    } catch (SingleRunner::Stop&) {
        assert(!SingleRunner::running());
        assert(!SingleRunner::stopping());
    }
    std::cout << "test_runner_can_throw passed\n";
}

void test_runner_can_be_instantiated_and_stopped_and_started_again_multiple_times() {
    Runner runner;
    waitUntilCounting(runner);
    assert(SingleRunner::running());
    SingleRunner::stop();
    assert(!SingleRunner::running());

    for (int iteration = 0; iteration < testIterations; ++iteration) {
        Runner runner;
        waitUntilCounting(runner);
        SingleRunner::stop();
        assert(!SingleRunner::running());
    }
    std::cout
        << "test_runner_can_be_instantiated_and_stopped_and_started_again_multiple_times passed\n";
}

void test_runner_can_be_instantiated_and_stopped_and_started_again_multiple_times_concurrently() {
    Runner runner0;
    {
        // Check that the runner is counting
        waitUntilCounting(runner0);
        assert(SingleRunner::running());
        assert(!SingleRunner::stopping());
    }

    for (int iteration = 0; iteration < testIterations; ++iteration) {
        // Implicitly stops the first runner
        Runner runner1;
        waitUntilCounting(runner1);

        assert(SingleRunner::running());
        assert(!SingleRunner::stopping());

        // Runner1 is counting: check that the first runner is no longer counting
        auto count0 = runner0.count();
        waitUntilCountingManyMore(runner1);
        assert(runner0.count() == count0);

        // Start a few more runners
        const int numberOfRunners = 10;
        std::array<Runner, numberOfRunners> runners;

        // Wait until all have started counting
        for (auto& runner : runners) waitUntilCounting(runner);

        assert(SingleRunner::running());
        assert(!SingleRunner::stopping());

        // Now only a single runner should still be counting, figure out which one it is.
        std::array<uint64_t, numberOfRunners> count;
        for (int i = 0; i < numberOfRunners; ++i) count[i] = runners[i].count();

        int active = -1;
        while (active == -1) {
            for (int i = 0; i < numberOfRunners; ++i) {
                if (count[i] != runners[i].count()) active = i;
            }
            std::this_thread::yield();
        }

        waitUntilCountingManyMore(runners[active]);
        std::cout << "active runner " << active << ", count " << runners[active].count() << "\n";

        // Ensure that all other runners were not counting while the active runner was
        for (int i = 0; i < numberOfRunners; ++i) {
            assert(i == active || runners[i].count() == count[i]);
        }
        assert(SingleRunner::running());
        SingleRunner::stop();
    }
    assert(!SingleRunner::running());
    assert(!SingleRunner::stopping());
    std::cout << "test_runner_can_be_instantiated_and_stopped_and_started_again_multiple_times_"
                 "concurrently passed\n";
}

int main() {
    test_runner_can_be_instantiated();
    test_runner_can_be_instantiated_and_stopped();
    test_runner_can_throw();
    test_runner_can_be_instantiated_and_stopped_and_started_again_multiple_times();
    test_runner_can_be_instantiated_and_stopped_and_started_again_multiple_times_concurrently();
    std::cout << "All SingleRunner tests passed!\n";
    return 0;
}