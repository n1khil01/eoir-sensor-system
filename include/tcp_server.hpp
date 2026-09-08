#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "detection_event.hpp"

// TCP server that streams DetectionEvents to every connected client as
// newline-delimited JSON.
//
// Design: the processing thread must never block on network I/O, so
// PublishEvent() only pushes onto an internal queue and returns -- a
// dedicated broadcaster thread drains that queue and does the actual
// send() calls to each connected client. A slow client can therefore
// stall the broadcaster (and other clients behind it in the same pass),
// but it can never stall detection processing. That's a real, documented
// limit of this single-broadcaster-thread design, not an oversight.
//
// SIGPIPE safety: writing to a socket the peer has closed raises SIGPIPE
// on Linux/POSIX by default, which kills the process. This class disables
// that at construction (signal(SIGPIPE, SIG_IGN)) and additionally passes
// MSG_NOSIGNAL to send() where the platform supports it, so a client
// disconnecting mid-write is reported through send()'s return value
// instead of terminating the server.
class DetectionEventServer {
public:
    explicit DetectionEventServer(uint16_t port);
    ~DetectionEventServer();

    DetectionEventServer(const DetectionEventServer&) = delete;
    DetectionEventServer& operator=(const DetectionEventServer&) = delete;

    // Opens the listening socket and starts the accept and broadcaster
    // threads. Throws std::runtime_error on failure.
    void Start();

    // Stops both threads and closes every socket. Safe to call once;
    // also called from the destructor if not already stopped.
    void Stop();

    // Enqueues `event` for delivery to every currently connected client.
    // Never blocks on network I/O -- see class comment.
    void PublishEvent(const DetectionEvent& event);

    size_t ClientCount() const;

private:
    void AcceptLoop();
    void BroadcastLoop();

    uint16_t port_;
    int listen_fd_ = -1;

    std::atomic<bool> stop_{false};
    std::thread accept_thread_;
    std::thread broadcast_thread_;

    mutable std::mutex clients_mutex_;
    std::vector<int> client_fds_;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<DetectionEvent> queue_;
};
