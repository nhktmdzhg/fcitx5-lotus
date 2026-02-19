/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-monitor.h"
#include "lotus-utils.h"

#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>

void deletingTimeMonitor() {
    while (!stop_flag_monitor.load()) {
        int64_t deleting_since = 0;

        {
            std::unique_lock<std::mutex> lock(monitor_mutex);
            monitor_cv.wait(lock, [] { return is_deleting_.load(std::memory_order_acquire) || stop_flag_monitor.load(std::memory_order_acquire); });
        }

        if (stop_flag_monitor.load())
            break;

        deleting_since = now_ms();

        while (is_deleting_.load(std::memory_order_acquire) && !stop_flag_monitor.load()) {
            int64_t current_time = now_ms();

            int64_t rep_start = replacement_start_ms_.load(std::memory_order_acquire);
            if (rep_start > 0 && (current_time - rep_start) > 200) {
                int expected_id = replacement_thread_id_.load(std::memory_order_acquire);
                if (expected_id > 0) {
                    is_deleting_.store(false, std::memory_order_release);
                    needFallbackCommit.store(true, std::memory_order_release);
                    replacement_start_ms_.store(0, std::memory_order_release);
                    break;
                }
            }

            if ((current_time - deleting_since) >= 1500) {
                is_deleting_.store(false);
                needEngineReset.store(true);
                replacement_start_ms_.store(0, std::memory_order_release);
                break;
            }

            {
                std::unique_lock<std::mutex> lock(monitor_mutex);
                monitor_cv.wait_for(lock, std::chrono::milliseconds(2));
            }
        }
    }
}

void startMonitoringOnce() {
    if (monitor_running.load())
        return;
    std::call_once(monitor_init_flag, []() {
        stop_flag_monitor.store(false);
        std::thread(deletingTimeMonitor).detach();
    });
}

void mousePressResetThread() {
    const std::string mouse_socket_path = buildSocketPath("mouse_socket");

    while (!stop_flag_monitor.load(std::memory_order_relaxed)) {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            sleep(1);
            continue;
        }

        struct sockaddr_un addr{};
        addr.sun_family  = AF_UNIX;
        addr.sun_path[0] = '\0';
        memcpy(addr.sun_path + 1, mouse_socket_path.c_str(), mouse_socket_path.length());
        socklen_t len = offsetof(struct sockaddr_un, sun_path) + mouse_socket_path.length() + 1;

        if (connect(sock, (struct sockaddr*)&addr, len) < 0) {
            close(sock);
            sleep(1);
            continue;
        }
        mouse_socket_fd.store(sock, std::memory_order_release);

        struct pollfd pfd;
        pfd.fd     = sock;
        pfd.events = POLLIN;

        while (!stop_flag_monitor.load(std::memory_order_relaxed)) {
            int ret = poll(&pfd, 1, -1);

            if (ret > 0 && (pfd.revents & POLLIN)) {
                char    buf[16];
                ssize_t n = recv(sock, buf, sizeof(buf), 0);

                if (n <= 0) {
                    break;
                }

                struct ucred cred;
                socklen_t    len                = sizeof(struct ucred);
                char         exe_path[PATH_MAX] = {0};
                if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0) {
                    char path[64];
                    snprintf(path, sizeof(path), "/proc/%d/cmdline", cred.pid);
                    int fd = open(path, O_RDONLY);
                    if (fd >= 0) {
                        read(fd, exe_path, sizeof(exe_path) - 1);
                        close(fd);
                    }
                }

                if (n > 0 && strcmp(exe_path, "/usr/bin/fcitx5-lotus-server") == 0) {
                    needEngineReset.store(true, std::memory_order_relaxed);
                    g_mouse_clicked.store(true, std::memory_order_relaxed);
                }
            } else if (ret < 0 && errno != EINTR) {
                break;
            }
        }
        close(sock);
        mouse_socket_fd.store(-1, std::memory_order_release);
    }
}

void startMouseReset() {
    std::thread(mousePressResetThread).detach();
}
