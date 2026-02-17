/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <libinput.h>
#include <libudev.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <pwd.h>
#include <sched.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <signal.h>
#include <atomic>
// --- VARIABLES ---
static int               uinput_fd_ = -1;
static std::atomic<bool> g_running{true};

// signal handler

static void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        g_running.store(false);
    }
}

// get username
std::string get_current_username() {
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_name : "unknown";
}

// system functions
static void boost_process_priority() {
    setpriority(PRIO_PROCESS, 0, -10);
}

static void pin_to_pcore() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i <= 3; ++i)
        CPU_SET(i, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
}

static void send_single_backspace() {
    if (uinput_fd_ < 0)
        return;
    struct input_event ev[2];
    memset(ev, 0, sizeof(ev));

    // Press
    ev[0].type  = EV_KEY;
    ev[0].code  = KEY_BACKSPACE;
    ev[0].value = 1;

    ev[1].type  = EV_SYN;
    ev[1].code  = SYN_REPORT;
    ev[1].value = 0;
    write(uinput_fd_, ev, sizeof(ev));

    // Release
    ev[0].value = 0;
    write(uinput_fd_, ev, sizeof(ev));
}

// LIBINPUT HELPERS
static int open_restricted(const char* path, int flags, void* /*user_data*/) {
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}
static void close_restricted(int fd, void* /*user_data*/) {
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted  = open_restricted,
    .close_restricted = close_restricted,
};

// MAIN FUNCTION
int main(int argc, char* argv[]) {
    std::string target_user;
    if (argc == 3 && strcmp(argv[1], "-u") == 0) {
        target_user = argv[2];
    } else {
        target_user = get_current_username();
    }
    boost_process_priority();
    pin_to_pcore();

    std::string backspace_socket;
    backspace_socket.reserve(40);
    backspace_socket += "vmksocket-";
    backspace_socket += target_user;
    backspace_socket += "-kb_socket";

    std::string mouse_flag_socket;
    mouse_flag_socket.reserve(48);
    mouse_flag_socket += "vmksocket-";
    mouse_flag_socket += target_user;
    mouse_flag_socket += "-mouse_socket";

    // Setup Uinput
    uinput_fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd_ >= 0) {
        ioctl(uinput_fd_, UI_SET_EVBIT, EV_KEY);
        ioctl(uinput_fd_, UI_SET_KEYBIT, KEY_BACKSPACE);
        struct uinput_setup usetup;
        memset(&usetup, 0, sizeof(usetup));
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor  = 0x1234;
        usetup.id.product = 0x5678;
        strncpy(usetup.name, "VMK-Uinput-Server", UINPUT_MAX_NAME_SIZE - 1);
        ioctl(uinput_fd_, UI_DEV_SETUP, &usetup);
        ioctl(uinput_fd_, UI_DEV_CREATE);
        sleep(1);
    }

    int                server_fd       = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0); // Non-blocking socket
    int                mouse_server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0); // Non-blocking socket

    struct sockaddr_un addr_kb{};
    struct sockaddr_un addr_mouse{};

    addr_kb.sun_family    = AF_UNIX;
    addr_mouse.sun_family = AF_UNIX;

    addr_kb.sun_path[0]    = '\0';
    addr_mouse.sun_path[0] = '\0';

    memcpy(addr_kb.sun_path + 1, backspace_socket.c_str(), backspace_socket.length());
    memcpy(addr_mouse.sun_path + 1, mouse_flag_socket.c_str(), mouse_flag_socket.length());

    socklen_t kb_len    = offsetof(struct sockaddr_un, sun_path) + backspace_socket.length() + 1;
    socklen_t mouse_len = offsetof(struct sockaddr_un, sun_path) + mouse_flag_socket.length() + 1;

    if (bind(server_fd, (struct sockaddr*)&addr_kb, kb_len) != 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        return 1;
    }

    if (bind(mouse_server_fd, (struct sockaddr*)&addr_mouse, mouse_len) != 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        return 1;
    }

    listen(server_fd, 5);
    listen(mouse_server_fd, 5);

    struct udev*     udev = udev_new();
    struct libinput* li   = libinput_udev_create_context(&interface, NULL, udev);
    libinput_udev_assign_seat(li, "seat0");
    int                        li_fd = libinput_get_fd(li);

    std::vector<struct pollfd> fds;
    fds.push_back({server_fd, POLLIN, 0});
    fds.push_back({li_fd, POLLIN, 0});
    fds.push_back({mouse_server_fd, POLLIN, 0});
    fds.push_back({-1, POLLIN, 0});

    int              addon_fd           = -1;
    int              pending_backspaces = 0;

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    while (true) {
        int poll_timeout = (pending_backspaces > 0) ? 1 : -1;
        int ret          = poll(fds.data(), fds.size(), poll_timeout);

        if (ret < 0) {
            // if error, continue
            if (errno == EINTR) {
                if (!g_running.load(std::memory_order_acquire)) {
                    break;
                }
                continue;
            }
            break; // real error
        }

        if (ret == 0) {
            if (pending_backspaces > 0) {
                send_single_backspace();
                --pending_backspaces;
                if (pending_backspaces == 0) {
                    char ack = '7';
                    send(fds[3].fd, &ack, sizeof(ack), MSG_NOSIGNAL | MSG_DONTWAIT);
                }
            }
        }

        libinput_dispatch(li); // Update internal state

        // handle socket (backspace)
        if (fds[0].revents & POLLIN) {
            int client_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK);
            if (client_fd >= 0) {
                struct ucred cred;
                socklen_t    len                = sizeof(struct ucred);
                char         exe_path[PATH_MAX] = {0};

                if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0) {
                    char path[64];
                    snprintf(path, sizeof(path), "/proc/%d/exe", cred.pid);

                    ssize_t ret = readlink(path, exe_path, sizeof(exe_path) - 1);
                    if (ret != -1) {
                        exe_path[ret] = '\0';
                    }
                }

                if (strcmp(exe_path, "/usr/bin/fcitx5") == 0) {
                    if (fds[3].fd >= 0) {
                        close(fds[3].fd);
                    }
                    fds[3].fd = client_fd;
                } else {
                    close(client_fd);
                }
            }
        }

        // handle connect from addon
        if (fds[3].revents & (POLLIN | POLLHUP | POLLERR)) {
            int count = 0;
            if (fds[3].fd >= 0) {
                ssize_t n = recv(fds[3].fd, &count, sizeof(count), 0);
                if (n <= 0) {
                    close(fds[3].fd);
                    fds[3].fd = -1;
                } else {
                    if (count > 10) {
                        count = 10;
                    }
                    pending_backspaces += count - 1;
                    send_single_backspace();
                }
            }
        }

        // connect to mouse socket
        if (fds[2].revents & POLLIN) {
            int new_fd = accept4(mouse_server_fd, nullptr, nullptr, SOCK_NONBLOCK);
            if (new_fd >= 0) {
                if (addon_fd >= 0)
                    close(addon_fd);
                addon_fd = new_fd;
            }
        }

        // handle mouse (libinput)
        if (fds[1].revents & POLLIN) {
            struct libinput_event* event;

            while ((event = libinput_get_event(li))) {
                enum libinput_event_type type = libinput_event_get_type(event);

                if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
                    struct libinput_event_pointer* p = libinput_event_get_pointer_event(event);
                    // only when pressed
                    if (libinput_event_pointer_get_button_state(p) == LIBINPUT_BUTTON_STATE_PRESSED) {
                        // send flag through socket
                        if (addon_fd >= 0) {
                            send(addon_fd, "C", 1, MSG_NOSIGNAL | MSG_DONTWAIT);
                        }
                    }
                } else if (type == LIBINPUT_EVENT_DEVICE_ADDED) {
                    // add new device
                    struct libinput_device* dev = libinput_event_get_device(event);
                    if (libinput_device_config_tap_get_finger_count(dev) > 0) {
                        libinput_device_config_tap_set_enabled(dev, LIBINPUT_CONFIG_TAP_ENABLED);
                        libinput_device_config_tap_set_button_map(dev, LIBINPUT_CONFIG_TAP_MAP_LRM);
                    }
                }
                libinput_event_destroy(event);
            }
        }
    }

    // Cleanup
    libinput_unref(li);
    udev_unref(udev);
    if (uinput_fd_ >= 0) {
        ioctl(uinput_fd_, UI_DEV_DESTROY);
        close(uinput_fd_);
    }
    if (addon_fd >= 0) {
        close(addon_fd);
    }
    if (fds[3].fd >= 0) {
        close(fds[3].fd);
    }
    close(server_fd);
    close(mouse_server_fd);
    return 0;
}
