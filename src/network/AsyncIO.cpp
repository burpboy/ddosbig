#include "AsyncIO.h"

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>

#if defined(ASYNCIO_PLATFORM_LINUX) || defined(ASYNCIO_PLATFORM_BSD)
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace asyncio {

// ============================================================================
// Utility helpers
// ============================================================================
int setNonBlocking(int fd) {
#if defined(ASYNCIO_PLATFORM_LINUX) || defined(ASYNCIO_PLATFORM_BSD)
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
#else
    (void)fd;
    return 0;
#endif
}

int setCloseOnExec(int fd) {
#if defined(ASYNCIO_PLATFORM_LINUX) || defined(ASYNCIO_PLATFORM_BSD)
    int flags = ::fcntl(fd, F_GETFD, 0);
    if (flags < 0) return -1;
    if (::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) return -1;
    return 0;
#else
    (void)fd;
    return 0;
#endif
}

AsyncFile::AsyncFile(int fd) : fd_(fd) {
    if (fd_ >= 0) {
        setNonBlocking(fd_);
        setCloseOnExec(fd_);
    }
}

AsyncFile::~AsyncFile() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

// ============================================================================
// TimerHandle::cancel
// ============================================================================
void TimerHandle::cancel() {
    // Delegates to impl; guarded by id_ so it stays valid
    if (!loop_ || id_ == 0) return;
    // implemented via EventLoop::Impl below — we call through a free function
    // set via a static hook to avoid exposing Impl.
    extern void cancelTimerHook(EventLoop* loop, TimerHandle::Id id);
    cancelTimerHook(loop_, id_);
    id_ = 0;
}

// ============================================================================
// Platform-specific backend helpers
// ============================================================================

#if defined(ASYNCIO_PLATFORM_LINUX)
// ---------- epoll backend ----------
static uint32_t to_epoll(Event e) {
    uint32_t mask = 0;
    if (has(e, Event::Read))  mask |= EPOLLIN;
    if (has(e, Event::Write)) mask |= EPOLLOUT;
    if (has(e, Event::Error)) mask |= EPOLLERR;
    if (has(e, Event::Hup))   mask |= EPOLLHUP;
    if (has(e, Event::Edge))  mask |= EPOLLET;
    if (has(e, Event::Once))  mask |= EPOLLONESHOT;
    if (mask == 0) mask = EPOLLIN; // ensure valid
    return mask;
}

static Event from_epoll(uint32_t mask) {
    Event e = Event::None;
    if (mask & EPOLLIN)  e |= Event::Read;
    if (mask & EPOLLOUT) e |= Event::Write;
    if (mask & EPOLLERR) e |= Event::Error;
    if (mask & EPOLLHUP) e |= Event::Hup;
    if (mask & EPOLLRDHUP) e |= Event::Hup;
    return e;
}
#endif // ASYNCIO_PLATFORM_LINUX

#if defined(ASYNCIO_PLATFORM_BSD)
// ---------- kqueue backend ----------
static uint32_t to_kqueue_filter(Event e, bool& hasRead, bool& hasWrite) {
    hasRead = has(e, Event::Read);
    hasWrite = has(e, Event::Write);
    // we use separate kevents for read and write
    return 0;
}

static Event from_kqueue(int16_t filter, uint16_t flags) {
    Event e = Event::None;
    if (filter == EVFILT_READ)  e |= Event::Read;
    if (filter == EVFILT_WRITE) e |= Event::Write;
    if (flags & EV_ERROR) e |= Event::Error;
    if (flags & EV_EOF)   e |= Event::Hup;
    return e;
}
#endif

#if defined(ASYNCIO_PLATFORM_FALLBACK)
// ---------- poll fallback ----------
static short to_poll(Event e) {
    short mask = 0;
    if (has(e, Event::Read))  mask |= POLLIN;
    if (has(e, Event::Write)) mask |= POLLOUT;
    return mask;
}
static Event from_poll(short revents) {
    Event e = Event::None;
    if (revents & POLLIN)  e |= Event::Read;
    if (revents & POLLOUT) e |= Event::Write;
    if (revents & POLLERR) e |= Event::Error;
    if (revents & POLLHUP) e |= Event::Hup;
    if (revents & POLLNVAL) e |= Event::Error;
    return e;
}
#endif

// ============================================================================
// EventLoop::Impl
// ============================================================================
struct EventLoop::Impl {
    // platform handle
#if defined(ASYNCIO_PLATFORM_LINUX)
    int epfd = -1;
#elif defined(ASYNCIO_PLATFORM_BSD)
    int kqfd = -1;
#endif

    // fd -> callback + current mask
    struct Entry {
        Callback cb;
        Event    mask = Event::None;
    };
    std::unordered_map<int, Entry> entries;

    // timers
    struct Timer {
        TimerHandle::Id id;
        std::chrono::steady_clock::time_point when;
        std::function<void()> cb;
        bool active = true;
    };
    struct TimerCmp {
        bool operator()(const Timer& a, const Timer& b) const { return a.when > b.when; }
    };
    // priority_queue with lazy deletion (stale entries checked by id)
    std::priority_queue<Timer, std::vector<Timer>, TimerCmp> timers;
    std::unordered_set<TimerHandle::Id> cancelledTimers;
    TimerHandle::Id nextTimerId = 1;
    mutable std::mutex timerMutex;

    Impl() {
#if defined(ASYNCIO_PLATFORM_LINUX)
        epfd = ::epoll_create1(EPOLL_CLOEXEC);
        if (epfd < 0) {
            std::perror("[AsyncIO] epoll_create1");
        }
#elif defined(ASYNCIO_PLATFORM_BSD)
        kqfd = ::kqueue();
        if (kqfd < 0) {
            std::perror("[AsyncIO] kqueue");
        } else {
            setCloseOnExec(kqfd);
        }
#endif
    }

    ~Impl() {
#if defined(ASYNCIO_PLATFORM_LINUX)
        if (epfd >= 0) ::close(epfd);
#elif defined(ASYNCIO_PLATFORM_BSD)
        if (kqfd >= 0) ::close(kqfd);
#endif
    }

    int add(int fd, Event events, Callback cb) {
        if (fd < 0) { errno = EINVAL; return -1; }
        if (entries.count(fd)) { errno = EEXIST; return -1; }

        Entry e;
        e.cb = std::move(cb);
        e.mask = events;

#if defined(ASYNCIO_PLATFORM_LINUX)
        struct epoll_event ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.events = to_epoll(events);
        ev.data.fd = fd;
        if (::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            return -1;
        }
#elif defined(ASYNCIO_PLATFORM_BSD)
        bool hasRead, hasWrite;
        to_kqueue_filter(events, hasRead, hasWrite);
        struct kevent changes[2];
        int n = 0;
        if (hasRead)  { EV_SET(&changes[n++], fd, EVFILT_READ,  EV_ADD | (has(events, Event::Edge) ? EV_CLEAR : 0), 0, 0, nullptr); }
        if (hasWrite) { EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD | (has(events, Event::Edge) ? EV_CLEAR : 0), 0, 0, nullptr); }
        if (n == 0) { EV_SET(&changes[n++], fd, EVFILT_READ, EV_ADD, 0, 0, nullptr); }
        if (::kevent(kqfd, changes, n, nullptr, 0, nullptr) < 0) {
            return -1;
        }
#endif
        entries.emplace(fd, std::move(e));
        return 0;
    }

    int modify(int fd, Event events) {
        auto it = entries.find(fd);
        if (it == entries.end()) { errno = ENOENT; return -1; }

#if defined(ASYNCIO_PLATFORM_LINUX)
        struct epoll_event ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.events = to_epoll(events);
        ev.data.fd = fd;
        if (::epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
            return -1;
        }
#elif defined(ASYNCIO_PLATFORM_BSD)
        bool hasRead, hasWrite;
        to_kqueue_filter(events, hasRead, hasWrite);
        struct kevent changes[2];
        int n = 0;
        if (hasRead)  { EV_SET(&changes[n++], fd, EVFILT_READ,  EV_ADD | (has(events, Event::Edge) ? EV_CLEAR : 0), 0, 0, nullptr); }
        if (hasWrite) { EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD | (has(events, Event::Edge) ? EV_CLEAR : 0), 0, 0, nullptr); }
        // remove the ones we no longer want
        if (!hasRead)  { EV_SET(&changes[n++], fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr); }
        if (!hasWrite) { EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr); }
        if (n > 0) ::kevent(kqfd, changes, n, nullptr, 0, nullptr);
#endif
        it->second.mask = events;
        return 0;
    }

    int remove(int fd) {
        auto it = entries.find(fd);
        if (it == entries.end()) { errno = ENOENT; return -1; }

#if defined(ASYNCIO_PLATFORM_LINUX)
        if (::epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            // ENOENT if fd already closed — acceptable
            if (errno != ENOENT && errno != EBADF) return -1;
        }
#elif defined(ASYNCIO_PLATFORM_BSD)
        struct kevent changes[2];
        EV_SET(&changes[0], fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr);
        EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        ::kevent(kqfd, changes, 2, nullptr, 0, nullptr);
#endif
        entries.erase(it);
        return 0;
    }

    TimerHandle addTimer(int64_t ms, std::function<void()> cb) {
        std::lock_guard<std::mutex> lk(timerMutex);
        Timer t;
        t.id = nextTimerId++;
        t.when = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        t.cb = std::move(cb);
        timers.push(std::move(t));
        return TimerHandle(nextTimerId - 1, nullptr); // loop ptr filled by caller
    }

    void cancelTimer(TimerHandle::Id id) {
        std::lock_guard<std::mutex> lk(timerMutex);
        cancelledTimers.insert(id);
    }

    // Compute timeout for platform wait based on earliest timer.
    // Returns -1 if no timers.
    int computeTimeout(int userTimeoutMs) {
        std::lock_guard<std::mutex> lk(timerMutex);
        // discard cancelled heads
        while (!timers.empty() && !timers.top().active) timers.pop();

        int timerMs = -1;
        if (!timers.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(timers.top().when - now).count();
            if (diff < 0) diff = 0;
            timerMs = static_cast<int>(diff);
        }
        if (userTimeoutMs < 0) return timerMs;
        if (timerMs < 0)       return userTimeoutMs;
        return userTimeoutMs < timerMs ? userTimeoutMs : timerMs;
    }

    // Fire expired timers. Returns count fired.
    int fireTimers() {
        std::vector<std::function<void()>> toRun;
        {
            std::lock_guard<std::mutex> lk(timerMutex);
            auto now = std::chrono::steady_clock::now();
            while (!timers.empty()) {
                // discard cancelled / inactive heads
                while (!timers.empty() && !timers.top().active) timers.pop();
                if (timers.empty()) break;
                // Peek: if the head was cancelled by id, pop and continue
                if (cancelledTimers.count(timers.top().id)) {
                    cancelledTimers.erase(timers.top().id);
                    timers.pop();
                    continue;
                }
                if (timers.top().when > now) break;
                // Move out
                Timer t = timers.top();
                timers.pop();
                if (cancelledTimers.count(t.id)) {
                    cancelledTimers.erase(t.id);
                    continue;
                }
                toRun.push_back(std::move(t.cb));
            }
        }
        for (auto& fn : toRun) {
            if (fn) fn();
        }
        return static_cast<int>(toRun.size());
    }
};

// ============================================================================
// Static hook to allow TimerHandle::cancel to reach Impl
// ============================================================================
namespace {
    // We store the loop->impl mapping so TimerHandle::cancel works.
    // Simpler: make EventLoop expose a friend. We use a free function.
}

void cancelTimerHook(EventLoop* loop, TimerHandle::Id id) {
    if (!loop) return;
    // Downcast to Impl
    struct Access : EventLoop { using EventLoop::impl_; };
    // Not allowed to access private member from outside; but we declared
    // the free function as extern inside the .cpp, and it's a friend?
    // Instead, we use a public method. Simplify: add a public method below.
    // For brevity, we call the public method we add:
    loop->cancelTimerPublic(id);
}

// ============================================================================
// EventLoop public API
// ============================================================================
EventLoop::EventLoop() : impl_(new Impl()) {}

EventLoop::~EventLoop() = default;

int EventLoop::add(int fd, Event events, Callback cb) {
    return impl_->add(fd, events, std::move(cb));
}

int EventLoop::modify(int fd, Event events) {
    return impl_->modify(fd, events);
}

int EventLoop::remove(int fd) {
    return impl_->remove(fd);
}

TimerHandle EventLoop::addTimer(int64_t ms, std::function<void()> cb) {
    TimerHandle h = impl_->addTimer(ms, std::move(cb));
    // Recreate handle with loop pointer
    return TimerHandle(h.id(), this);
}

// This public method is used by TimerHandle::cancel via the hook.
// Declared in header? We need to declare it. We'll do so via friend.
// For now we use a cleaner approach: add it as a public method in the header.
// (See note below.)

static const char* backendNameImpl() {
#if defined(ASYNCIO_PLATFORM_LINUX)
    return "epoll";
#elif defined(ASYNCIO_PLATFORM_BSD)
    return "kqueue";
#else
    return "poll";
#endif
}

const char* EventLoop::backendName() { return backendNameImpl(); }

int EventLoop::runOnce(int timeoutMs) {
    if (!impl_) { errno = EINVAL; return -1; }

    // 1. Fire expired timers first (they may add/remove fds)
    int fired = impl_->fireTimers();

    // 2. Compute wait timeout
    int waitMs = impl_->computeTimeout(timeoutMs);

    // 3. Wait for I/O
    int ioEvents = 0;

#if defined(ASYNCIO_PLATFORM_LINUX)
    constexpr int MAX_EVENTS = 256;
    struct epoll_event evs[MAX_EVENTS];
    int n = ::epoll_wait(impl_->epfd, evs, MAX_EVENTS, waitMs);
    if (n < 0) {
        if (errno == EINTR) return fired;
        return -1;
    }
    for (int i = 0; i < n; ++i) {
        int fd = evs[i].data.fd;
        auto it = impl_->entries.find(fd);
        if (it == impl_->entries.end()) continue; // removed during iteration
        Event ev = from_epoll(evs[i].events);
        // Save callback (copy) so it survives if callback removes itself
        Callback cb = it->second.cb;
        if (cb) cb(ev);
        ++ioEvents;
    }
#elif defined(ASYNCIO_PLATFORM_BSD)
    constexpr int MAX_EVENTS = 256;
    struct kevent evs[MAX_EVENTS];
    struct timespec ts;
    struct timespec* tsp = nullptr;
    if (waitMs >= 0) {
        ts.tv_sec  = waitMs / 1000;
        ts.tv_nsec = (waitMs % 1000) * 1000000L;
        tsp = &ts;
    }
    int n = ::kevent(impl_->kqfd, nullptr, 0, evs, MAX_EVENTS, tsp);
    if (n < 0) {
        if (errno == EINTR) return fired;
        return -1;
    }
    for (int i = 0; i < n; ++i) {
        int fd = static_cast<int>(evs[i].ident);
        auto it = impl_->entries.find(fd);
        if (it == impl_->entries.end()) continue;
        Event ev = from_kqueue(evs[i].filter, evs[i].flags);
        Callback cb = it->second.cb;
        if (cb) cb(ev);
        ++ioEvents;
    }
#else
    // ---- poll fallback ----
    std::vector<struct pollfd> pfds;
    pfds.reserve(impl_->entries.size());
    std::vector<int> fds;
    fds.reserve(impl_->entries.size());
    for (auto& kv : impl_->entries) {
        struct pollfd p;
        p.fd = kv.first;
        p.events = to_poll(kv.second.mask);
        p.revents = 0;
        pfds.push_back(p);
        fds.push_back(kv.first);
    }
    int n = ::poll(pfds.data(), pfds.size(), waitMs);
    if (n < 0) {
        if (errno == EINTR) return fired;
        return -1;
    }
    for (size_t i = 0; i < pfds.size(); ++i) {
        if (pfds[i].revents == 0) continue;
        auto it = impl_->entries.find(fds[i]);
        if (it == impl_->entries.end()) continue;
        Event ev = from_poll(pfds[i].revents);
        Callback cb = it->second.cb;
        if (cb) cb(ev);
        ++ioEvents;
    }
#endif

    return fired + ioEvents;
}

void EventLoop::run() {
    running_ = true;
    while (running_) {
        int r = runOnce(-1);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
    }
}

} // namespace asyncio
