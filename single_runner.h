#include <atomic>
#include <cassert>
#include <cstdint>
#include <exception>
#include <string>

/**
 * The SingleRunner struct ensures that only a single runner is active. It also provides a way to
 * stop the runner from another thread. This is done by colling stop(), setting the state to
 * STOPPING or STOPPED, and waiting for the state to settle to STOPPED. The runner will stop as soon
 * as possible. Creating a new SingleRunner will first stop the previous one, if any. This makes it
 * safe to use global state within a runner.
 */
struct SingleRunner {

    /** If called concurrently with an active runner, stops at least one and waits for it. */
    static void stop();

    /** Same as above, but return immediately. */
    static void stopNoWait();

    /** Should be checked regularly during a run. If true, the run should be stopped. */
    static bool stopping() noexcept;

    /** Returns true if a runner is currently active. */
    static bool running() noexcept;

    /** Throws a Stop exception if the run should be stopped. Alternative to calling stopping() */
    static void checkStop();

    struct Stop : public std::exception {
        const char* what() const noexcept override { return "Stopped"; }
    };

    /** Ctor/Dtor, so runners can just declare SingleRunner RAII object and be safe. */
    SingleRunner();
    ~SingleRunner();

private:
    enum RunState : int8_t { IDLE = 0, STARTED = 1, STOPPED = 2, STOPPING = 3 };
};
