# CPU/RAM Optimization Tips

## 1. CPU Optimization

### A. Asynchronous I/O Selection
*   **Linux**: Use `epoll` for general use. If using Linux Kernel 5.1+, enable `io_uring` for significantly lower latency and higher throughput. `io_uring` reduces context switches and allows submission/completion batching.
*   **Configuration**: Ensure `AsyncIO` detects the best available mechanism at runtime.

### B. Thread Affinity
*   Bind worker threads to specific CPU cores to prevent cache thrashing.
*   **Command**: Use `taskset` or `sched_setaffinity` in `SocketManager`.
*   **Example**: If you have 16 cores, run 16 threads, each pinned to one core.

### C. Zero-Copy Techniques
*   When sending large payloads, use `sendfile()` or `io_uring's `IOSQE_IO_LINK` to reduce memory copies between user space and kernel space.
*   For raw sockets, ensure payload buffers are aligned to cache line boundaries (64 bytes) to avoid false sharing.

### D. Rate Limiting Efficiency
*   Use a **Token Bucket** algorithm implemented with atomic counters instead of mutex locks for rate limiting.
*   Avoid `sleep()` calls; use precise nanosecond calculations to wake up exactly when the next packet should be sent.

## 2. RAM Optimization

### A. Memory Pool Allocation
*   Pre-allocate memory pools for packet buffers. Instead of `malloc/free` for every packet, use a custom allocator or `mmap`'d shared memory regions.
*   **Buffer Reuse**: Reuse socket buffers across connections to reduce GC pressure (if using any managed layers) and heap fragmentation.

### B. Socket Buffer Tuning
*   Increase `SO_SNDBUF` and `SO_RCVBUF` for high-throughput connections.
    ```cpp
    int bufsize = 1024 * 1024; // 1MB
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
    ```
*   Disable Nagle’s Algorithm (`TCP_NODELAY`) for low-latency protocols like SYN flood or small HTTP requests.

### C. Log Buffering
*   Use a ring buffer for logging instead of direct `fprintf` or `cout`. Flush logs asynchronously to disk periodically.
*   Limit log verbosity in production. Use `--verbose` only for debugging.

## 3. Network Stack Tuning (Linux)

Edit `/etc/sysctl.conf` and apply with `sysctl -p`:

```ini
# Increase maximum number of connections
net.core.somaxconn = 65535

# Increase local port range for spoofing
net.ipv4.ip_local_port_range = 1024 65535

# Reduce time-wait sockets
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 15

# Increase file descriptor limits
fs.file-max = 1000000
