#include "tcp_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <stdexcept>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace {
void SetNoDelay(int fd) {
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}

// Belt-and-suspenders SIGPIPE suppression: MSG_NOSIGNAL handles it on
// Linux, SO_NOSIGPIPE is the macOS equivalent, and ignoring the signal
// process-wide covers any platform/code path that has neither.
void SuppressSigpipe(int fd) {
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
    (void)fd;
#endif
}
}  // namespace

DetectionEventServer::DetectionEventServer(uint16_t port) : port_(port) {
    std::signal(SIGPIPE, SIG_IGN);
}

DetectionEventServer::~DetectionEventServer() { Stop(); }

void DetectionEventServer::Start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("socket() failed");
    }

    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) <
        0) {
        close(listen_fd_);
        throw std::runtime_error(std::string("bind() failed on port ") +
                                  std::to_string(port_) + ": " +
                                  std::strerror(errno));
    }

    if (listen(listen_fd_, /*backlog=*/16) < 0) {
        close(listen_fd_);
        throw std::runtime_error("listen() failed");
    }

    stop_.store(false);
    accept_thread_ = std::thread(&DetectionEventServer::AcceptLoop, this);
    broadcast_thread_ =
        std::thread(&DetectionEventServer::BroadcastLoop, this);
}

void DetectionEventServer::Stop() {
    if (stop_.exchange(true)) {
        return;  // already stopped
    }

    if (listen_fd_ >= 0) {
        shutdown(listen_fd_, SHUT_RDWR);
        close(listen_fd_);
        listen_fd_ = -1;
    }
    queue_cv_.notify_all();

    if (accept_thread_.joinable()) accept_thread_.join();
    if (broadcast_thread_.joinable()) broadcast_thread_.join();

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int fd : client_fds_) {
        close(fd);
    }
    client_fds_.clear();
}

void DetectionEventServer::AcceptLoop() {
    while (!stop_.load(std::memory_order_relaxed)) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd =
            accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr),
                   &client_len);
        if (client_fd < 0) {
            // listen_fd_ was closed by Stop(), or a transient accept error.
            continue;
        }
        SetNoDelay(client_fd);
        SuppressSigpipe(client_fd);

        std::lock_guard<std::mutex> lock(clients_mutex_);
        client_fds_.push_back(client_fd);
    }
}

void DetectionEventServer::BroadcastLoop() {
    while (true) {
        DetectionEvent event;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [&] {
                return !queue_.empty() || stop_.load(std::memory_order_relaxed);
            });
            if (queue_.empty()) {
                if (stop_.load(std::memory_order_relaxed)) return;
                continue;
            }
            event = queue_.front();
            queue_.pop_front();
        }

        std::string payload = ToJsonLine(event);

        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto it = client_fds_.begin(); it != client_fds_.end();) {
            ssize_t sent = send(*it, payload.data(), payload.size(),
                                 MSG_NOSIGNAL);
            if (sent < 0) {
                close(*it);
                it = client_fds_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void DetectionEventServer::PublishEvent(const DetectionEvent& event) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push_back(event);
    }
    queue_cv_.notify_one();
}

size_t DetectionEventServer::ClientCount() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return client_fds_.size();
}
