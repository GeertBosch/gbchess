#include "single_runner.h"

namespace {
std::atomic<int8_t> state;
}  // namespace

/** If called concurrently with an active runner, stops at least one. */
void SingleRunner::stop() {
    if (state.load() == IDLE) return;  // No runner active
    while (state.fetch_or(STOPPED) & STARTED) state.wait(STOPPING);
}

void SingleRunner::stopNoWait() {
    if (state.load() == IDLE) return;  // No runner active
    state.fetch_or(STOPPED);
}

/** Should be checked regularly during a run. If true, the run should be stopped. */
bool SingleRunner::stopping() noexcept {
    return state.load() == STOPPING;
}

bool SingleRunner::running() noexcept {
    return state.load() == STARTED;
}

void SingleRunner::checkStop() {
    if (stopping()) throw Stop();
}

SingleRunner::SingleRunner() {
    while (true) {
        int8_t expected = IDLE;
        if (state.compare_exchange_weak(expected, STARTED)) return;
        stop();
        expected = STOPPED;
        state.compare_exchange_weak(expected, IDLE);
    }
}

/** Called when a search has finished, either because of completion or a stop request. */
SingleRunner::~SingleRunner() {
    assert(state.load() & STARTED);
    auto oldState = state.exchange(IDLE);
    if (oldState == STOPPING) state.notify_all();
}