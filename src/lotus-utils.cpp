/*
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-utils.h"
#include "lotus-config.h"

#include <cstddef>
#include <fcitx-utils/utf8.h>
#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>

// Global variables
std::atomic<fcitx::LotusMode> realMode{fcitx::LotusMode::Smooth};
std::atomic<bool>             needEngineReset{false};
std::atomic<bool>             g_mouse_clicked{false};
std::atomic<bool>             is_deleting_{false};
std::atomic<bool>             stop_flag_monitor{false};
std::atomic<int>              uinput_client_fd_{-1};
std::atomic<unsigned int>     realtextLen{0};
std::atomic<int>              mouse_socket_fd{-1};
std::atomic<uint64_t>         lastCommitTimeUsec_{0};

FCITX_DEFINE_LOG_CATEGORY(lotus, "lotus", fcitx::LogLevel::NoLog);

// Kiểm tra xem có nên từ chối yêu cầu reset từ ứng dụng/compositor hay không.
// Khi gõ tiếng Việt (đặc biệt trong các app như Chrome, Electron, Qt, GTK):
// 1. Quá trình gửi phím Backspace qua uinput hoặc commit ký tự mới thường kích hoạt sự kiện reset từ app gửi về Fcitx5.
// 2. Nếu chấp nhận reset lúc này, Bamboo Engine sẽ bị reset về trạng thái ban đầu, làm mất toàn bộ ngữ cảnh từ đang gõ dở
//    dẫn đến các lỗi kinh điển: mất dấu, nhảy chữ, gõ 'tiếng' thành 'tiế ng', v.v.
// Vì vậy: Chặn triệt để reset nếu:
// - Đang bận gửi phím xóa qua uinput (is_deleting_ == true)
// - Hoặc vừa mới commit xong ký tự thay thế trong cửa sổ an toàn 50ms (lastCommitTimeUsec_)
bool shouldRejectReset() {
    if (is_deleting_.load(std::memory_order_acquire)) {
        return true;
    }
    uint64_t lastCommit = lastCommitTimeUsec_.load(std::memory_order_acquire);
    if (lastCommit != 0) {
        uint64_t now = fcitx::now(CLOCK_MONOTONIC);
        if (now >= lastCommit && (now - lastCommit) <= 50000) { // Cửa sổ an toàn 50ms (50,000 microsecond) sau commit
            return true;
        }
    }
    return false;
}

std::string buildSocketPath(const char* base_path_suffix) {
    struct passwd  pwd{};
    struct passwd* result   = nullptr;
    long           buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (buf_size == -1) {
        buf_size = 16384;
    }
    std::vector<char> buf(buf_size);
    std::string       username;
    int               res = getpwuid_r(getuid(), &pwd, buf.data(), buf_size, &result);
    if (res == 0 && result != nullptr) {
        username = result->pw_name;
    } else {
        username = "unknown";
    }
    std::string path;
    path.reserve(32);
    path += "lotussocket-";
    path += username;
    path += '-';
    path += base_path_suffix;
    const size_t max_socket_path_length = UNIX_PATH_MAX - 1;
    path.resize(std::min(path.length(), max_socket_path_length));
    return path;
}

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool isBackspace(uint32_t sym) {
    return sym == 65288 || sym == 8 || sym == FcitxKey_BackSpace;
}

int compareAndSplitStrings(const std::string& A, const std::string& B, std::string& deletedPart, std::string& addedPart) {
    size_t i = 0;
    size_t j = 0;
    while (i < A.size() && j < B.size()) {
        unsigned int lenA = fcitx_utf8_char_len(&A[i]);
        unsigned int lenB = fcitx_utf8_char_len(&B[j]);
        if (lenA == 0 || lenB == 0) {
            break;
        }
        if (i + lenA > A.size() || j + lenB > B.size()) {
            break;
        }
        if (lenA == lenB && std::strncmp(&A[i], &B[j], lenA) == 0) {
            i += lenA;
            j += lenB;
        } else {
            break;
        }
    }

    deletedPart.assign(A, i);
    addedPart.assign(B, j);
    return (deletedPart.empty() && addedPart.empty()) ? 1 : 2;
}

bool isStartsWith(const std::string& str, const std::string& prefix) {
#if __cplusplus >= 202002L
    return str.starts_with(prefix);
#else
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
#endif
}

std::string getFrontendName(fcitx::InputContext* ic) {
    if (ic == nullptr) {
        return "unknown";
    }
    return ic->frontend();
}

void eraseLastUtf8Codepoint(std::string& buffer) {
    if (buffer.empty()) {
        return;
    }
    size_t pos = buffer.size() - 1;
    while (pos > 0 && (static_cast<unsigned char>(buffer[pos]) & 0xC0) == 0x80) {
        --pos;
    }
    buffer.erase(pos);
}
