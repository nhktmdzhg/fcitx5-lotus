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
// --- VARIABLES ---
static int uinput_fd_ = -1;

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

static inline void sleep_us(long us) {
    struct timespec ts;
    ts.tv_sec  = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
}

// input injector functions
static inline struct input_event make_event(int type, int code, int value) {
    struct input_event ev{};
    ev.type  = static_cast<unsigned short>(type);
    ev.code  = static_cast<unsigned short>(code);
    ev.value = value;
    return ev;
}

void send_backspace_uinput(int count) {
    if (uinput_fd_ < 0 || count <= 0)
        return;
    if (count > 10)
        count = 10;

    const int                INTER_KEY_DELAY_US = 1200;

    const struct input_event cycle[4] = {
        make_event(EV_KEY, KEY_BACKSPACE, 1),
        make_event(EV_SYN, SYN_REPORT, 0),
        make_event(EV_KEY, KEY_BACKSPACE, 0),
        make_event(EV_SYN, SYN_REPORT, 0),
    };

    for (int i = 0; i < count; ++i) {
        write(uinput_fd_, cycle, sizeof(cycle));
        sleep_us(INTER_KEY_DELAY_US);
    }
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
        struct uinput_user_dev uidev{};
        snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Fcitx5_Uinput_Server");
        (void)write(uinput_fd_, &uidev, sizeof(uidev));
        ioctl(uinput_fd_, UI_DEV_CREATE);
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

    std::vector<struct pollfd> fds(3);
    fds[0].fd     = server_fd;
    fds[0].events = POLLIN;
    fds[1].fd     = li_fd;
    fds[1].events = POLLIN;
    fds[2].fd     = mouse_server_fd;
    fds[2].events = POLLIN;

    int addon_fd = -1;

    while (true) {
        int ret = poll(fds.data(), fds.size(), -1);

        if (ret < 0) {
            // if error, continue
            if (errno == EINTR)
                continue;
            break; // real error
        }

        // handle socket (backspace)
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                int          num_backspace = 0;
                int          n             = recv(client_fd, &num_backspace, sizeof(num_backspace), 0);

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

                if (n > 0 && strcmp(exe_path, "/usr/bin/fcitx5") == 0) {
                    send_backspace_uinput(num_backspace);
                    char ack = '7';
                    send(client_fd, &ack, sizeof(ack), MSG_NOSIGNAL);
                }
                close(client_fd);
            }
        }

        // connect to mouse socket
        if (fds[2].revents & POLLIN) {
            int new_fd = accept(mouse_server_fd, nullptr, nullptr);
            if (new_fd >= 0) {
                if (addon_fd >= 0)
                    close(addon_fd);
                addon_fd = new_fd;
                fcntl(addon_fd, F_SETFL, O_NONBLOCK);
            }
        }

        // handle mouse (libinput)
        if (fds[1].revents & POLLIN) {
            libinput_dispatch(li); // Update internal state
            struct libinput_event* event;

            while ((event = libinput_get_event(li))) {
                enum libinput_event_type type = libinput_event_get_type(event);

                if (type == LIBINPUT_EVENT_DEVICE_ADDED) {
                    // add new device
                    struct libinput_device* dev = libinput_event_get_device(event);
                    if (libinput_device_config_tap_get_finger_count(dev) > 0) {
                        libinput_device_config_tap_set_enabled(dev, LIBINPUT_CONFIG_TAP_ENABLED);
                        libinput_device_config_tap_set_button_map(dev, LIBINPUT_CONFIG_TAP_MAP_LRM);
                    }
                } else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
                    struct libinput_event_pointer* p = libinput_event_get_pointer_event(event);
                    // only when pressed
                    if (libinput_event_pointer_get_button_state(p) == LIBINPUT_BUTTON_STATE_PRESSED) {

                        // send flag through socket
                        if (addon_fd >= 0) {
                            if (send(addon_fd, "C", 1, MSG_NOSIGNAL) < 0) {
                                close(addon_fd);
                                addon_fd = -1;
                            }
                        }
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
    close(server_fd);
    close(mouse_server_fd);
    return 0;
}