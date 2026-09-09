/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-server.h
 * @brief Uinput server for handling backspace and mouse events.
 *
 * This server runs as a separate process with elevated privileges to:
 * - Send backspace key events via uinput device
 * - Monitor mouse button presses via libinput
 * - Communicate with fcitx5 addon via Unix sockets
 */

#ifndef _LOTUS_SERVER_H_
#define _LOTUS_SERVER_H_

#include <atomic>
#include <fcntl.h>
#include <libinput.h>
#include <linux/uinput.h>
#include <poll.h>
#include <pwd.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

/**
 * @brief RAII Wrapper for Unix File Descriptors.
 * Ensures that close() is called when the object goes out of scope.
 * Complies with Rule of Five by disabling copy and implementing move.
 */
class FdGuard {
  public:
    explicit FdGuard(int fd = -1) : fd_(fd) {}
    ~FdGuard();

    // Rule of Five: Disable copying, allow moving
    FdGuard(const FdGuard&)            = delete;
    FdGuard& operator=(const FdGuard&) = delete;
    FdGuard(FdGuard&& other) noexcept;
    FdGuard& operator=(FdGuard&& other) noexcept;

    int      get() const {
        return fd_;
    }
    bool is_valid() const {
        return fd_ >= 0;
    }
    void reset(int new_fd = -1);

  private:
    int fd_;
};

/**
 * @brief Protocol message for keyboard socket.
 * type=0: send N backspaces (legacy)
 * type=1: Shift+Left select N chars, then send done ACK
 */
struct ShiftSelectMsg {
    int type;  ///< 0 = backspace, 1 = shift+select
    int count; ///< number of keys to send
};

/**
 * @brief RAII Wrapper for uinput device.
 * Handles UI_DEV_CREATE and UI_DEV_DESTROY automatically.
 */
class UinputDevice {
  public:
    UinputDevice() = default;
    ~UinputDevice();

    // Disable copy, allow move
    UinputDevice(const UinputDevice&)            = delete;
    UinputDevice& operator=(const UinputDevice&) = delete;
    UinputDevice(UinputDevice&&)                 = default;
    UinputDevice& operator=(UinputDevice&&)      = default;

    bool          initialize();
    void          send_backspace();
    /**
     * @brief Sends Shift+Left N times to select N chars, then sends done ACK.
     * @param count Number of Left key presses.
     * @param client_fd Socket fd to send ACK "D" after completion.
     */
    void          send_shift_select(int count, int client_fd);
    int           get_fd() const {
        return guard_.get();
    }

  private:
    FdGuard guard_;
    void    emit_key(int code, int value);
};

/**
 * @brief RAII Wrapper for Libinput and Udev contexts.
 */
class LibinputContext {
  public:
    LibinputContext(const struct libinput_interface* interface);
    ~LibinputContext();

    // Disable copy, allow move
    LibinputContext(const LibinputContext&)            = delete;
    LibinputContext& operator=(const LibinputContext&) = delete;
    LibinputContext(LibinputContext&& other) noexcept : udev_(other.udev_), li_(other.li_) {
        other.udev_ = nullptr;
        other.li_   = nullptr;
    }

    LibinputContext& operator=(LibinputContext&& other) noexcept {
        if (this != &other) {
            this->~LibinputContext();
            udev_       = other.udev_;
            li_         = other.li_;
            other.udev_ = nullptr;
            other.li_   = nullptr;
        }
        return *this;
    }

    bool is_valid() const {
        return li_ != nullptr;
    }
    struct libinput* get_li() const {
        return li_;
    }
    int get_fd() const {
        return libinput_get_fd(li_);
    }

  private:
    struct udev*     udev_ = nullptr;
    struct libinput* li_   = nullptr;
};

/**
 * @brief Maximum length of Unix socket paths.
*/
#define UNIX_PATH_MAX sizeof(((struct sockaddr_un*)0)->sun_path)

/**
 * @brief Global flag to control server running state.
 */
extern std::atomic<bool> g_running; //NOLINT

/**
 * @brief Signal handler for graceful shutdown.
 * @param sig Signal number (SIGTERM or SIGINT).
 */
void signal_handler(int sig);

/**
 * @brief Gets the current username.
 * @return Username string or "unknown" if cannot determine.
 */
std::string get_current_username();

/**
 * @brief Gets the UID for a given username.
 * @param username Username to resolve.
 * @return UID of the user, or (uid_t)-1 if not found.
 */
uid_t get_uid_for_user(const std::string& username);

/**
 * @brief Boosts process priority for real-time responsiveness.
 */
void boost_process_priority();

/**
 * @brief Pins process to performance cores (cores 0-3).
 */
void pin_to_pcore();

/**
 * @brief Opens input device with restricted permissions.
 * @param path Device path.
 * @param flags Open flags.
 * @param user_data Unused user data.
 * @return File descriptor or negative errno on error.
 */
int open_restricted(const char* path, int flags, void* user_data);

/**
 * @brief Closes input device.
 * @param fd File descriptor to close.
 * @param user_data Unused user data.
 */
void close_restricted(int fd, void* user_data);

/**
 * @brief libinput interface callbacks.
 */
extern const struct libinput_interface interface;

/**
 * @brief Main entry point for the uinput server.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit code (0 on success).
 */
int main(int argc, char* argv[]);

#endif // _LOTUS_SERVER_H_
