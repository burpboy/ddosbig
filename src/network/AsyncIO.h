#ifndef ASYNCIO_H
#define ASYNCIO_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

// ============================================================================
// Platform Detection
// ============================================================================
#if defined(__linux__)
    #define ASYNCIO_PLATFORM_LINUX 1
    #include <sys/epoll.h>
    #if defined(ASYNCIO_USE_IO_URING) && __has_include(<liburing.h>)
        #define ASYNCIO_HAS_IO_URING 1
        #include <liburing.h>
    #endif
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define ASYNCIO_PLATFORM_BSD 1
    #include <sys/event.h>
    #include <sys/time.h>
#else
    #define ASYNCIO_PLATFORM_FALLBACK 1
    #include <poll.h>
#endif

namespace asyncio {

// ============================================================================
// Event Mask (portable)
// ============================================================================
enum class Event : uint32_t {
    None   = 0,
    Read   = 1u << 0,
    Write  = 1u << 1,
    Error  = 1u << 2,
    Hup    = 1u << 3,
    Edge   = 1u << 4,   // edge-triggered (epoll/kqueue only)
    Once   = 1u << 5,   // one-shot
};

inline Event operator|(Event a, Event b) {
    return static_cast<Event>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline Event operator&(Event a, Event b) {
    return static_cast<Event>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline Event& operator|=(Event& a, Event b) { a = a | b; return a; }
inline bool has(Event mask, Event flag) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Forward decl
// ============================================================================
class EventLoop;

// ============================================================================
// Timer handle
// ============================================================================
class TimerHandle {
public:
    using Id = uint64_t;
    TimerHandle() : id_(0), loop_(nullptr) {}
    TimerHandle(Id id, EventLoop* loop) : id_(id), loop_(loop) {}
    void cancel();
    bool valid() const { return id_ != 0; }
    Id id() const { return id_; }
private:
    Id id_;
    EventLoop* loop_;
};

// ============================================================================
// Result structure for read/write completion
// ============================================================================
struct IoResult {
    int fd = -1;
    ssize_t bytes = 0;
    int error = 0;          // errno value, 0 = success
    bool eof = false;

    bool ok() const { return error == 0; }
    explicit operator bool() const { return ok(); }
};

// ============================================================================
// EventLoop — the main class
// ============================================================================
class EventLoop {
public:
    using Callback = std::function<void(Event)>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // ---- File descriptor registration ----
    // Returns 0 on success, -1 on failure (errno set).
    int add(int fd, Event events, Callback cb);
    int modify(int fd, Event events);
    int remove(int fd);

    // ---- One-shot timer ----
    // callback invoked once after `ms` milliseconds.
    TimerHandle addTimer(int64_t ms, std::function<void()> cb);

    // ---- Run loop ----
    // timeoutMs: -1 = block indefinitely; 0 = non-blocking poll
    // Returns number of events dispatched, or -1 on error.
    int runOnce(int timeoutMs = -1);

    // Run until stop() is called.
    void run();

    void stop() { running_ = false; }

    // ---- Accessors ----
    static const char* backendName();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool running_ = false;
};

// ============================================================================
// Helper: set non-blocking
// ============================================================================
int setNonBlocking(int fd);
int setCloseOnExec(int fd);

// ============================================================================
// Async file descriptor wrapper (optional convenience)
// ============================================================================
class AsyncFile {
public:
    explicit AsyncFile(int fd);
    ~AsyncFile();
    int fd() const { return fd_; }
    int release() { int f = fd_; fd_ = -1; return f; }
private:
    int fd_;
};

} // namespace asyncio

#endif // ASYNCIO_H
