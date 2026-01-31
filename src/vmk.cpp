/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "vmk.h"

#include <fcitx-config/iniparser.h>
#include <fcitx-utils/charutils.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysymgen.h>
#include <fcitx-utils/standardpath.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/menu.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterface.h>
#include <fcitx/userinterfacemanager.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

#include <dirent.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __linux__
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <limits.h>
#include <mutex>
#endif

fcitx::VMKMode    realMode = fcitx::VMKMode::VMK1;
std::atomic<bool> needEngineReset{false};
std::string       BASE_SOCKET_PATH;
// Global flag to signal mouse click for closing app mode menu
static std::atomic<bool> g_mouse_clicked{false};

std::atomic<bool>        is_deleting_{false};
static const int         MAX_BACKSPACE_COUNT = 15;
std::string              SubstrChar(const std::string& s, size_t start, size_t len);
int                      compareAndSplitStrings(const std::string& A, const std::string& B, std::string& commonPrefix, std::string& deletedPart, std::string& addedPart);
std::once_flag           monitor_init_flag;
std::atomic<bool>        stop_flag_monitor{false};
std::atomic<bool>        monitor_running{false};
int                      uinput_fd_        = -1;
int                      uinput_client_fd_ = -1;
int                      extraBackspace    = 0;

std::string              buildSocketPath(const char* base_path_suffix) {
    const char* username_c = std::getenv("USER");
    std::string username   = username_c ? std::string(username_c) : "default";
    std::string path       = "vmksocket-" + username + "-" + std::string(base_path_suffix);
    return path;
}

void deletingTimeMonitor() {
    auto t_start  = std::chrono::high_resolution_clock::now();
    bool last_val = 0;

    while (!stop_flag_monitor.load()) {
        bool current_val = is_deleting_.load();

        if (!last_val && current_val) {
            t_start = std::chrono::high_resolution_clock::now();
        }

        if (current_val) {
            auto t_now    = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start).count();

            if (duration >= 1500) {
                is_deleting_.store(false);
                needEngineReset.store(true);
                current_val = false;
            }
        }
        last_val = current_val;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
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

struct KeyEntry {
    uint32_t sym;
    uint32_t state;
};

bool isBackspace(uint32_t sym) {
    return sym == 65288 || sym == 8 || sym == FcitxKey_BackSpace;
}

namespace fcitx {
    namespace {
        constexpr std::string_view MacroPrefix             = "macro/";
        constexpr std::string_view InputMethodActionPrefix = "vmk-input-method-";
        constexpr std::string_view CharsetActionPrefix     = "vmk-charset-";
        const std::string          CustomKeymapFile        = "conf/vmk-custom-keymap.conf";

        std::string                macroFile(std::string_view imName) {
            return stringutils::concat("conf/vmk-macro-", imName, ".conf");
        }

        uintptr_t newMacroTable(const vmkMacroTable& macroTable) {
            std::vector<char*> charArray;
            RawConfig          r;
            macroTable.save(r);
            for (const auto& keymap : *macroTable.macros) {
                charArray.push_back(const_cast<char*>(keymap.key->data()));
                charArray.push_back(const_cast<char*>(keymap.value->data()));
            }
            charArray.push_back(nullptr);
            return NewMacroTable(charArray.data());
        }

        std::vector<std::string> convertToStringList(char** array) {
            std::vector<std::string> result;
            for (int i = 0; array[i]; ++i) {
                result.push_back(array[i]);
                free(array[i]);
            }
            free(array);
            return result;
        }

        static void DeletePreviousNChars(fcitx::InputContext* ic, size_t n, fcitx::Instance* instance) {
            if (!ic || !instance || n == 0)
                return;
            if (ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) {
                int offset = -static_cast<int>(n);
                ic->deleteSurroundingText(offset, static_cast<int>(n));
                return;
            }
            for (size_t i = 0; i < n; ++i) {
                fcitx::Key key(FcitxKey_BackSpace);
                ic->forwardKey(key, false);
                ic->forwardKey(key, true);
            }
        }

    } // namespace

    FCITX_DEFINE_LOG_CATEGORY(vmk, "vmk");

    class VMKState final : public InputContextProperty {
      public:
        VMKState(vmkEngine* engine, InputContext* ic) : engine_(engine), ic_(ic) {
            setEngine();
        }

        ~VMKState() {}

        void setEngine() {
            vmkEngine_.reset();
            realMode = fcitx::modeStringToEnum(engine_->config().mode.value());

            if (engine_->config().inputMethod.value() == "Custom") {
                std::vector<char*> charArray;
                for (const auto& keymap : *engine_->customKeymap().customKeymap) {
                    charArray.push_back(const_cast<char*>(keymap.key->data()));
                    charArray.push_back(const_cast<char*>(keymap.value->data()));
                }
                charArray.push_back(nullptr);
                vmkEngine_.reset(NewCustomEngine(charArray.data(), engine_->dictionary(), engine_->macroTable()));
            } else {
                vmkEngine_.reset(NewEngine(engine_->config().inputMethod->data(), engine_->dictionary(), engine_->macroTable()));
            }
            setOption();
        }

        void setOption() {
            if (!vmkEngine_)
                return;
            FcitxBambooEngineOption option = {
                .autoNonVnRestore    = *engine_->config().autoNonVnRestore,
                .ddFreeStyle         = true,
                .macroEnabled        = *engine_->config().macro,
                .autoCapitalizeMacro = *engine_->config().capitalizeMacro,
                .spellCheckWithDicts = *engine_->config().spellCheck,
                .outputCharset       = engine_->config().outputCharset->data(),
                .modernStyle         = *engine_->config().modernStyle,
                .freeMarking         = *engine_->config().freeMarking,
            };
            EngineSetOption(vmkEngine_.handle(), &option);
        }

        bool connect_uinput_server() {
            if (uinput_client_fd_ >= 0)
                return true;
            BASE_SOCKET_PATH               = buildSocketPath("kb_socket");
            const std::string current_path = BASE_SOCKET_PATH;
            int               current_fd   = socket(AF_UNIX, SOCK_STREAM, 0);
            if (current_fd < 0)
                return false;

            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;

            addr.sun_path[0] = '\0';
            memcpy(addr.sun_path + 1, current_path.c_str(), current_path.length());
            socklen_t len = offsetof(struct sockaddr_un, sun_path) + current_path.length() + 1;

            if (connect(current_fd, (struct sockaddr*)&addr, len) == 0) {
                uinput_client_fd_ = current_fd;
                uinput_fd_        = current_fd;
                return true;
            }
            close(current_fd);
            uinput_client_fd_ = -1;
            uinput_fd_        = -1;
            return false;
        }

        bool send_command_to_server(int count) {
            if (uinput_client_fd_ >= 0) {
                if (send(uinput_client_fd_, &count, sizeof(count), 0) >= 0)
                    return true;
            }
            if (uinput_client_fd_ >= 0) {
                close(uinput_client_fd_);
                uinput_client_fd_ = -1;
                uinput_fd_        = -1;
            }
            if (!connect_uinput_server())
                return false;
            if (send(uinput_client_fd_, &count, sizeof(count), 0) < 0) {
                close(uinput_client_fd_);
                uinput_client_fd_ = -1;
                uinput_fd_        = -1;
                return false;
            }
            return true;
        }

        int setup_uinput() {
            return connect_uinput_server() ? uinput_fd_ : -1;
        }

        void send_backspace_uinput(int count) {
            if (uinput_fd_ < 0 && !connect_uinput_server())
                return;
            if (count > MAX_BACKSPACE_COUNT)
                count = MAX_BACKSPACE_COUNT;
            send_command_to_server(count);
        }

        bool handleUInputKeyPress(fcitx::KeyEvent& event, fcitx::KeySym currentSym) {
            if (!is_deleting_.load())
                return false;
            if (isBackspace(currentSym)) {
                current_backspace_count_ += 1;
                if (current_backspace_count_ < expected_backspaces_) {
                    return false;
                } else {
                    is_deleting_.store(false);
                    current_thread_id_.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    ic_->commitString(pending_commit_string_);
                    expected_backspaces_     = 0;
                    current_backspace_count_ = -1;
                    pending_commit_string_   = "";

                    event.filterAndAccept();
                    return true;
                }
            }
            return false;
        }

        void replayBufferToEngine(const std::string& buffer) {
            if (!vmkEngine_.handle())
                return;

            ResetEngine(vmkEngine_.handle());
            for (size_t i = 0; i < buffer.length();) {
                if (i + 2 < buffer.length() && buffer.substr(i, 3) == "\\b\\") {
                    EngineProcessKeyEvent(vmkEngine_.handle(), FcitxKey_BackSpace, 0);
                    i += 3;
                } else {
                    unsigned char c = static_cast<unsigned char>(buffer[i]);
                    EngineProcessKeyEvent(vmkEngine_.handle(), (uint32_t)c, 0);
                    i += 1;
                }
            }
        }

        bool isAutofillCertain(const fcitx::SurroundingText& s) {
            if (!s.isValid() || oldPreBuffer_.empty()) {
                return false;
            }

            int cursor = s.cursor();
            int anchor = s.anchor();

            if (cursor != anchor) {
                int selectionStart = std::min(anchor, cursor);
                int selectionEnd   = std::max(anchor, cursor);

                if (selectionStart >= cursor || (selectionStart < cursor && selectionEnd > cursor)) {
                    return true;
                }
            }

            return false;
        }

        // Helper function for preedit mode
        void handlePreeditMode(KeyEvent& keyEvent) {
            if (EngineProcessKeyEvent(vmkEngine_.handle(), keyEvent.rawKey().sym(), keyEvent.rawKey().states()))
                keyEvent.filterAndAccept();
            if (char* commit = EnginePullCommit(vmkEngine_.handle())) {
                if (commit[0])
                    ic_->commitString(commit);
                free(commit);
            }
            ic_->inputPanel().reset();
            UniqueCPtr<char> preedit(EnginePullPreedit(vmkEngine_.handle()));
            if (preedit && preedit.get()[0]) {
                std::string_view view = preedit.get();
                Text             text;
                TextFormatFlags  fmt = TextFormatFlag::NoFlag;
                if (utf8::validate(view))
                    text.append(std::string(view), fmt);
                text.setCursor(text.textLength());
                if (ic_->capabilityFlags().test(CapabilityFlag::Preedit))
                    ic_->inputPanel().setClientPreedit(text);
                else
                    ic_->inputPanel().setPreedit(text);
            }
            ic_->updatePreedit();
            ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
        }

        // Helper function for vmk1/vmk1hc mode
        void handleUinputMode(KeyEvent& keyEvent, fcitx::KeySym currentSym, bool checkEmptyPreedit) {
            if (!is_deleting_.load()) {
                if (isBackspace(currentSym) || currentSym == FcitxKey_space || currentSym == FcitxKey_Return) {
                    if (isBackspace(currentSym)) {
                        history_ += "\\b\\";
                        replayBufferToEngine(history_);
                        UniqueCPtr<char> preeditC(EnginePullPreedit(vmkEngine_.handle()));
                        oldPreBuffer_ = (preeditC && preeditC.get()[0]) ? preeditC.get() : "";
                    } else {
                        history_.clear();
                        ResetEngine(vmkEngine_.handle());
                        oldPreBuffer_.clear();
                    }
                    keyEvent.forward();
                    return;
                }
            } else {
                if (isBackspace(currentSym) && is_deleting_) {
                    if (handleUInputKeyPress(keyEvent, currentSym))
                        return;
                    return;
                }
                keyEvent.filterAndAccept();
                return;
            }
            if (!is_deleting_.load()) {
                if (uinput_fd_ < 0)
                    setup_uinput();
                if (!EngineProcessKeyEvent(vmkEngine_.handle(), currentSym, keyEvent.rawKey().states())) {
                    if (checkEmptyPreedit) {
                        UniqueCPtr<char> preeditC(EnginePullPreedit(vmkEngine_.handle()));
                        std::string      preeditStr = (preeditC && preeditC.get()[0]) ? preeditC.get() : "";
                        if (preeditStr.empty()) {
                            history_.clear();
                            oldPreBuffer_.clear();
                            keyEvent.forward();
                        }
                    }
                    return;
                }
                if (currentSym >= 32 && currentSym <= 126) {
                    history_ += static_cast<char>(currentSym);
                } else {
                    keyEvent.forward();
                    return;
                }
                replayBufferToEngine(history_);
                if (auto commitF = UniqueCPtr<char>(EnginePullCommit(vmkEngine_.handle())); commitF && commitF.get()[0]) {
                    history_.clear();
                    ResetEngine(vmkEngine_.handle());
                    oldPreBuffer_.clear();
                    return;
                }
                keyEvent.filterAndAccept();
                UniqueCPtr<char> preeditC(EnginePullPreedit(vmkEngine_.handle()));
                std::string      preeditStr = (preeditC && preeditC.get()[0]) ? preeditC.get() : "";
                std::string      commonPrefix, deletedPart, addedPart;
                if (compareAndSplitStrings(oldPreBuffer_, preeditStr, commonPrefix, deletedPart, addedPart)) {
                    if (deletedPart.empty()) {
                        if (!addedPart.empty()) {
                            ic_->commitString(addedPart);
                            oldPreBuffer_ = preeditStr;
                        }
                    } else {
                        current_backspace_count_ = 0;
                        pending_commit_string_   = addedPart;
                        oldPreBuffer_            = preeditStr;
                        if (uinput_client_fd_ < 0) {
                            std::string rawKey = keyEvent.key().toString();
                            if (!rawKey.empty()) {
                                ic_->commitString(rawKey);
                            }
                            return;
                        }
                        if (isAutofillCertain(ic_->surroundingText()))
                            extraBackspace = 1;

                        if (is_deleting_.load()) {
                            is_deleting_.store(false);
                        }

                        expected_backspaces_ = fcitx::utf8::length(deletedPart) + 1 + extraBackspace;
                        if (expected_backspaces_ > 0)
                            is_deleting_.store(true);
                        send_backspace_uinput(expected_backspaces_);
                        int my_id = ++current_thread_id_;

                        std::thread([this, my_id]() {
                            auto start = std::chrono::steady_clock::now();

                            while (is_deleting_.load()) {
                                if (current_thread_id_.load() != my_id) {
                                    return;
                                }

                                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                                auto now = std::chrono::steady_clock::now();
                                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > 200) {
                                    break;
                                }
                            }

                            if (current_thread_id_.load() == my_id) {
                                if (!pending_commit_string_.empty()) {
                                    ic_->commitString(pending_commit_string_);
                                    pending_commit_string_ = "";
                                }
                                is_deleting_.store(false);
                            }
                        }).detach();

                        extraBackspace = 0;
                    }
                }
                return;
            }
        }

        void keyEvent(KeyEvent& keyEvent) {
            if (!vmkEngine_ || keyEvent.isRelease())
                return;
            if (uinput_client_fd_ < 0) {
                connect_uinput_server();
            }
            if (current_backspace_count_ >= (int)expected_backspaces_ && is_deleting_.load()) {
                is_deleting_.store(false);
                current_backspace_count_ = -1;
                expected_backspaces_     = 0;
            }
            if (needEngineReset.load() && (realMode == fcitx::VMKMode::VMK1 || realMode == fcitx::VMKMode::VMK2 || realMode == fcitx::VMKMode::VMK1HC)) {
                oldPreBuffer_.clear();
                history_.clear();
                ResetEngine(vmkEngine_.handle());
                is_deleting_.store(false);
                current_backspace_count_ = -1;
                needEngineReset.store(false);
            }
            if (keyEvent.rawKey().check(FcitxKey_Shift_L) || keyEvent.rawKey().check(FcitxKey_Shift_R))
                return;
            const fcitx::KeySym currentSym = keyEvent.rawKey().sym();

            switch (realMode) {
                case fcitx::VMKMode::VMK1: {
                    handleUinputMode(keyEvent, currentSym, true);
                    break;
                }
                case fcitx::VMKMode::VMK1HC: {
                    handleUinputMode(keyEvent, currentSym, false);
                    break;
                }
                case fcitx::VMKMode::VMK2: {
                    auto ic = keyEvent.inputContext();
                    if (!ic)
                        return;
                    EngineProcessKeyEvent(vmkEngine_.handle(), keyEvent.rawKey().sym(), keyEvent.rawKey().states());
                    if (auto commitF = UniqueCPtr<char>(EnginePullCommit(vmkEngine_.handle())); commitF && commitF.get()[0]) {
                        ResetEngine(vmkEngine_.handle());
                        oldPreBuffer_.clear();
                        ic->inputPanel().reset();
                        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                        return;
                    }
                    UniqueCPtr<char> preeditC(EnginePullPreedit(vmkEngine_.handle()));
                    std::string      preeditStr = (preeditC && preeditC.get()[0]) ? preeditC.get() : "";
                    if (preeditStr != oldPreBuffer_) {
                        keyEvent.filterAndAccept();
                        if (!oldPreBuffer_.empty()) {
                            DeletePreviousNChars(ic, fcitx::utf8::length(oldPreBuffer_), engine_->instance());
                        }
                        if (!preeditStr.empty()) {
                            ic->commitString(preeditStr);
                            oldPreBuffer_ = preeditStr;
                        } else
                            oldPreBuffer_.clear();
                    }
                    break;
                }
                case fcitx::VMKMode::Preedit: {
                    handlePreeditMode(keyEvent);
                    break;
                }
                default: {
                    break;
                }
            }
        }

        void reset() {
            is_deleting_.store(false);
            clearAllBuffers();

            switch (realMode) {
                case fcitx::VMKMode::Preedit: {
                    ic_->inputPanel().reset();
                    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                    ic_->updatePreedit();
                    break;
                }
                case fcitx::VMKMode::VMK2:
                case fcitx::VMKMode::VMK1:
                case fcitx::VMKMode::VMK1HC: {
                    ic_->inputPanel().reset();
                    break;
                }
                default: {
                    break;
                }
            }
        }

        void commitBuffer() {
            switch (realMode) {
                case fcitx::VMKMode::Preedit: {
                    ic_->inputPanel().reset();
                    if (vmkEngine_) {
                        EngineCommitPreedit(vmkEngine_.handle());
                        UniqueCPtr<char> commit(EnginePullCommit(vmkEngine_.handle()));
                        if (commit && commit.get()[0])
                            ic_->commitString(commit.get());
                    }
                    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                    ic_->updatePreedit();
                    break;
                }
                case fcitx::VMKMode::VMK2: {
                    if (vmkEngine_)
                        ResetEngine(vmkEngine_.handle());
                    break;
                }
                default: {
                    break;
                }
            }
        }

        void clearAllBuffers() {
            oldPreBuffer_.clear();
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            current_thread_id_.store(0);
            history_.clear();
            if (vmkEngine_)
                ResetEngine(vmkEngine_.handle());
        }

      private:
        vmkEngine*       engine_;
        InputContext*    ic_;
        CGoObject        vmkEngine_;
        std::string      oldPreBuffer_;
        std::string      history_;
        size_t           expected_backspaces_     = 0;
        size_t           current_backspace_count_ = 0;
        std::string      pending_commit_string_;
        std::atomic<int> current_thread_id_{0};
    };

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

            // Connect to socket
            if (connect(sock, (struct sockaddr*)&addr, len) < 0) {
                close(sock);
                sleep(1); // Wait for socket to be ready
                continue;
            }

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

                    // Check app name
                    struct ucred cred;
                    socklen_t    len                = sizeof(struct ucred);
                    char         exe_path[PATH_MAX] = {0};
                    if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0) {
                        char path[64];
                        snprintf(path, sizeof(path), "/proc/%d/exe", cred.pid);
                        ssize_t ret = readlink(path, exe_path, sizeof(exe_path) - 1);
                        if (ret != -1) {
                            exe_path[ret] = '\0';
                        }
                    }

                    if (n > 0 && std::string(exe_path) == "/usr/bin/fcitx5-vmk-server") {
                        needEngineReset.store(true, std::memory_order_relaxed);
                        // Also signal that mouse was clicked to close app mode menu
                        g_mouse_clicked.store(true, std::memory_order_relaxed);
                    }
                } else if (ret < 0 && errno != EINTR) {
                    break;
                }
            }
            close(sock);
        }
    }

    void startMouseReset() {
        std::thread(mousePressResetThread).detach();
    }

    vmkEngine::vmkEngine(Instance* instance) : instance_(instance), factory_([this](InputContext& ic) { return new VMKState(this, &ic); }) {
        startMonitoringOnce();
        Init();
        {
            auto imNames = convertToStringList(GetInputMethodNames());
            imNames.push_back("Custom");
            imNames_ = std::move(imNames);
        }
        config_.inputMethod.annotation().setList(imNames_);
        auto fd = StandardPath::global().open(StandardPath::Type::PkgData, "vmk/vietnamese.cm.dict", O_RDONLY);
        if (!fd.isValid())
            throw std::runtime_error("Failed to load dictionary");
        dictionary_.reset(NewDictionary(fd.release()));

        auto& uiManager = instance_->userInterfaceManager();
        modeAction_     = std::make_unique<SimpleAction>();
        modeAction_->setIcon("preferences-system");
        modeAction_->setShortText(_("Chế độ gõ"));
        uiManager.registerAction("vmk-mode", modeAction_.get());
        modeMenu_ = std::make_unique<Menu>();
        modeAction_->setMenu(modeMenu_.get());

        std::vector<fcitx::VMKMode> modes = {fcitx::VMKMode::VMK1, fcitx::VMKMode::VMK2, fcitx::VMKMode::Preedit, fcitx::VMKMode::VMK1HC};
        for (const auto& mode : modes) {
            auto action = std::make_unique<SimpleAction>();
            action->setShortText(modeEnumToString(mode));
            action->setCheckable(true);
            uiManager.registerAction("vmk-mode-" + modeEnumToString(mode), action.get());
            connections_.emplace_back(action->connect<SimpleAction::Activated>([this, mode](InputContext* ic) {
                if (globalMode_ == mode) {
                    return;
                }

                config_.mode.setValue(modeEnumToString(mode));
                saveConfig();
                realMode    = mode;
                globalMode_ = mode;
                reloadConfig();
                updateModeAction(ic);
                if (ic) {
                    ic->updateUserInterface(fcitx::UserInterfaceComponent::StatusArea);
                }
            }));
            modeMenu_->addAction(action.get());
            modeSubAction_.push_back(std::move(action));
        }

        inputMethodAction_ = std::make_unique<SimpleAction>();
        inputMethodAction_->setIcon("document-edit");
        inputMethodAction_->setShortText("Kiểu gõ");
        uiManager.registerAction("vmk-input-method", inputMethodAction_.get());
        inputMethodMenu_ = std::make_unique<Menu>();
        inputMethodAction_->setMenu(inputMethodMenu_.get());

        for (const auto& imName : imNames_) {
            inputMethodSubAction_.emplace_back(std::make_unique<SimpleAction>());
            auto action = inputMethodSubAction_.back().get();
            action->setShortText(imName);
            action->setCheckable(true);
            uiManager.registerAction(stringutils::concat(InputMethodActionPrefix, imName), action);
            connections_.emplace_back(action->connect<SimpleAction::Activated>([this, imName](InputContext* ic) {
                if (config_.inputMethod.value() == imName)
                    return;
                config_.inputMethod.setValue(imName);
                saveConfig();
                refreshEngine();
                updateInputMethodAction(ic);
                if (ic)
                    ic->updateUserInterface(fcitx::UserInterfaceComponent::StatusArea);
            }));
            inputMethodMenu_->addAction(action);
        }

        charsetAction_ = std::make_unique<SimpleAction>();
        charsetAction_->setShortText(_("Bảng mã"));
        charsetAction_->setIcon("character-set");
        uiManager.registerAction("vmk-charset", charsetAction_.get());
        charsetMenu_ = std::make_unique<Menu>();
        charsetAction_->setMenu(charsetMenu_.get());

        auto charsets = convertToStringList(GetCharsetNames());
        for (const auto& charset : charsets) {
            charsetSubAction_.emplace_back(std::make_unique<SimpleAction>());
            auto action = charsetSubAction_.back().get();
            action->setShortText(charset);
            action->setCheckable(true);
            uiManager.registerAction(stringutils::concat(CharsetActionPrefix, charset), action);
            connections_.emplace_back(action->connect<SimpleAction::Activated>([this, charset](InputContext* ic) {
                if (config_.outputCharset.value() == charset)
                    return;
                config_.outputCharset.setValue(charset);
                saveConfig();
                refreshEngine();
                updateCharsetAction(ic);
                if (ic)
                    ic->updateUserInterface(fcitx::UserInterfaceComponent::StatusArea);
            }));
            charsetMenu_->addAction(action);
        }
        config_.outputCharset.annotation().setList(charsets);

        spellCheckAction_ = std::make_unique<SimpleAction>();
        spellCheckAction_->setLongText(_("Enable spell check"));
        spellCheckAction_->setIcon("tools-check-spelling");
        spellCheckAction_->setCheckable(true);
        connections_.emplace_back(spellCheckAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.spellCheck.setValue(!*config_.spellCheck);
            saveConfig();
            refreshOption();
            updateSpellAction(ic);
        }));
        uiManager.registerAction("vmk-spellcheck", spellCheckAction_.get());

        macroAction_ = std::make_unique<SimpleAction>();
        macroAction_->setLongText(_("Enable Macro"));
        macroAction_->setIcon("document-edit");
        macroAction_->setCheckable(true);
        connections_.emplace_back(macroAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.macro.setValue(!*config_.macro);
            saveConfig();
            refreshOption();
            updateMacroAction(ic);
        }));
        uiManager.registerAction("vmk-macro", macroAction_.get());

        capitalizeMacroAction_ = std::make_unique<SimpleAction>();
        capitalizeMacroAction_->setLongText(_("Capitalize Macro"));
        capitalizeMacroAction_->setIcon("format-text-uppercase");
        capitalizeMacroAction_->setCheckable(true);
        connections_.emplace_back(capitalizeMacroAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.capitalizeMacro.setValue(!*config_.capitalizeMacro);
            saveConfig();
            refreshOption();
            updateCapitalizeMacroAction(ic);
        }));
        uiManager.registerAction("vmk-capitalizemacro", capitalizeMacroAction_.get());

        autoNonVnRestoreAction_ = std::make_unique<SimpleAction>();
        autoNonVnRestoreAction_->setLongText(_("Auto restore keys with invalid words"));
        autoNonVnRestoreAction_->setIcon("edit-undo");
        autoNonVnRestoreAction_->setCheckable(true);
        connections_.emplace_back(autoNonVnRestoreAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.autoNonVnRestore.setValue(!*config_.autoNonVnRestore);
            saveConfig();
            refreshOption();
            updateAutoNonVnRestoreAction(ic);
        }));
        uiManager.registerAction("vmk-autonvnrestore", autoNonVnRestoreAction_.get());

        modernStyleAction_ = std::make_unique<SimpleAction>();
        modernStyleAction_->setLongText(_("Use oà, _uý (instead of òa, úy)"));
        modernStyleAction_->setIcon("text-x-generic");
        modernStyleAction_->setCheckable(true);
        connections_.emplace_back(modernStyleAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.modernStyle.setValue(!*config_.modernStyle);
            saveConfig();
            refreshOption();
            updateModernStyleAction(ic);
        }));
        uiManager.registerAction("vmk-modernstyle", modernStyleAction_.get());

        freeMarkingAction_ = std::make_unique<SimpleAction>();
        freeMarkingAction_->setLongText(_("Allow type with more freedom"));
        freeMarkingAction_->setIcon("document-open-recent");
        freeMarkingAction_->setCheckable(true);
        connections_.emplace_back(freeMarkingAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.freeMarking.setValue(!*config_.freeMarking);
            saveConfig();
            refreshOption();
            updateFreeMarkingAction(ic);
        }));
        uiManager.registerAction("vmk-freemarking", freeMarkingAction_.get());

        reloadConfig();
        globalMode_ = modeStringToEnum(config_.mode.value());
        updateModeAction(nullptr);
        instance_->inputContextManager().registerProperty("VMKState", &factory_);

        std::string configDir = StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
        if (!std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
        }
        appRulesPath_ = configDir + "/vmk-app-rules.conf";
        loadAppRules();
    }

    void vmkEngine::reloadConfig() {
        readAsIni(config_, "conf/vmk.conf");
        readAsIni(customKeymap_, CustomKeymapFile);
        for (const auto& imName : imNames_) {
            auto& table = macroTables_[imName];
            readAsIni(table, macroFile(imName));
            macroTableObject_[imName].reset(newMacroTable(table));
        }
        populateConfig();
    }

    const Configuration* vmkEngine::getSubConfig(const std::string& path) const {
        if (path == "custom_keymap")
            return &customKeymap_;
        if (stringutils::startsWith(path, MacroPrefix)) {
            const auto imName = path.substr(MacroPrefix.size());
            if (auto iter = macroTables_.find(imName); iter != macroTables_.end())
                return &iter->second;
        }
        return nullptr;
    }

    void vmkEngine::setConfig(const RawConfig& config) {
        config_.load(config, true);
        saveConfig();
        populateConfig();
    }

    void vmkEngine::populateConfig() {
        refreshEngine();
        refreshOption();
        updateModeAction(nullptr);
        updateInputMethodAction(nullptr);
        updateCharsetAction(nullptr);
        updateSpellAction(nullptr);
        updateMacroAction(nullptr);
        updateCapitalizeMacroAction(nullptr);
        updateAutoNonVnRestoreAction(nullptr);
        updateModernStyleAction(nullptr);
        updateFreeMarkingAction(nullptr);
    }

    void vmkEngine::setSubConfig(const std::string& path, const RawConfig& config) {
        if (path == "custom_keymap") {
            customKeymap_.load(config, true);
            safeSaveAsIni(customKeymap_, CustomKeymapFile);
            refreshEngine();
        } else if (stringutils::startsWith(path, MacroPrefix)) {
            const auto imName = path.substr(MacroPrefix.size());
            if (auto iter = macroTables_.find(imName); iter != macroTables_.end()) {
                iter->second.load(config, true);
                safeSaveAsIni(iter->second, macroFile(imName));
                macroTableObject_[imName].reset(newMacroTable(iter->second));
                refreshEngine();
            }
        }
    }

    std::string vmkEngine::subMode(const fcitx::InputMethodEntry&, fcitx::InputContext&) {
        return *config_.inputMethod;
    }

    void vmkEngine::activate(const InputMethodEntry& entry, InputContextEvent& event) {
        auto                     ic = event.inputContext();
        static std::atomic<bool> mouseThreadStarted{false};
        if (!mouseThreadStarted.exchange(true))
            startMouseReset();

        auto& statusArea = event.inputContext()->statusArea();
        if (ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit))
            instance_->inputContextManager().setPreeditEnabledByDefault(true);

        // ibus-bamboo mode save/load
        std::string    appName = ic->program();
        fcitx::VMKMode targetMode;

        if (!appRules_.empty() && appRules_.count(appName)) {
            targetMode = appRules_[appName];
        } else {
            targetMode = globalMode_;
        }
        reloadConfig();
        updateModeAction(event.inputContext());
        updateInputMethodAction(event.inputContext());
        updateCharsetAction(event.inputContext());

        realMode = targetMode;

        auto state = ic->propertyFor(&factory_);

        state->clearAllBuffers();
        is_deleting_.store(false);
        needEngineReset.store(false);
        ic->inputPanel().reset();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        ic->updatePreedit();

        statusArea.addAction(StatusGroup::InputMethod, modeAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, inputMethodAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, charsetAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, spellCheckAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, macroAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, capitalizeMacroAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, autoNonVnRestoreAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, modernStyleAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, freeMarkingAction_.get());
    }

    void vmkEngine::keyEvent(const InputMethodEntry& entry, KeyEvent& keyEvent) {
        auto ic = keyEvent.inputContext();

        // Check if mouse was clicked to close app mode menu
        if (isSelectingAppMode_ && g_mouse_clicked.load(std::memory_order_relaxed)) {
            closeAppModeMenu();
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            auto state = ic->propertyFor(&factory_);
            state->reset();
        }

        // logic when opening app mode menu
        if (isSelectingAppMode_) {
            if (keyEvent.isRelease())
                return;

            keyEvent.filterAndAccept();
            fcitx::VMKMode selectedMode  = fcitx::VMKMode::NoMode;
            bool           selectionMade = false;

            // map number key to mode
            if (keyEvent.key().check(FcitxKey_1))
                selectedMode = fcitx::VMKMode::VMK1;
            else if (keyEvent.key().check(FcitxKey_2))
                selectedMode = fcitx::VMKMode::VMK2;
            else if (keyEvent.key().check(FcitxKey_3))
                selectedMode = fcitx::VMKMode::Preedit;
            else if (keyEvent.key().check(FcitxKey_4))
                selectedMode = fcitx::VMKMode::VMK1HC;
            else if (keyEvent.key().check(FcitxKey_5))
                selectedMode = fcitx::VMKMode::Off;
            else if (keyEvent.key().check(FcitxKey_6)) {
                if (appRules_.count(currentConfigureApp_)) {
                    appRules_.erase(currentConfigureApp_);
                    saveAppRules();
                }
                selectionMade = true;
            } else if (keyEvent.key().check(FcitxKey_Escape)) {
                selectionMade = true;
            } else if (keyEvent.key().check(FcitxKey_grave)) {
                isSelectingAppMode_ = false;
                ic->inputPanel().reset();
                ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                auto state = ic->propertyFor(&factory_);
                state->reset();
                ic->commitString("`");
                return;
            }

            if (selectedMode != fcitx::VMKMode::NoMode) {
                appRules_[currentConfigureApp_] = selectedMode;
                saveAppRules();

                realMode      = selectedMode;
                selectionMade = true;
            }

            if (selectionMade) {
                isSelectingAppMode_ = false;
                ic->inputPanel().reset();
                ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                auto state = ic->propertyFor(&factory_);
                state->reset();
            }
            return;
        }

        // logic when typing `
        if (!keyEvent.isRelease() && keyEvent.rawKey().check(FcitxKey_grave)) {
            currentConfigureApp_ = ic->program();
            if (currentConfigureApp_.empty())
                currentConfigureApp_ = "unknown-app";
            g_mouse_clicked.store(false, std::memory_order_relaxed);
            showAppModeMenu(ic);
            keyEvent.filterAndAccept();
            return;
        }
        auto state = keyEvent.inputContext()->propertyFor(&factory_);
        state->keyEvent(keyEvent);
    }

    void vmkEngine::reset(const InputMethodEntry& entry, InputContextEvent& event) {
        auto state = event.inputContext()->propertyFor(&factory_);
        state->reset();
    }

    void vmkEngine::deactivate(const InputMethodEntry& entry, InputContextEvent& event) {
        auto state = event.inputContext()->propertyFor(&factory_);
        if (realMode == fcitx::VMKMode::Preedit) {
            if (event.type() != EventType::InputContextFocusOut)
                state->commitBuffer();
            else
                state->reset();
        } else {
            state->clearAllBuffers();
            is_deleting_.store(false);
            needEngineReset.store(false);
            event.inputContext()->inputPanel().reset();
            event.inputContext()->updateUserInterface(UserInterfaceComponent::InputPanel);
            event.inputContext()->updatePreedit();
        }
    }

    void vmkEngine::refreshEngine() {
        if (!factory_.registered())
            return;
        instance_->inputContextManager().foreach ([this](InputContext* ic) {
            auto state = ic->propertyFor(&factory_);
            state->setEngine();
            if (ic->hasFocus())
                state->reset();
            return true;
        });
    }

    void vmkEngine::refreshOption() {
        if (!factory_.registered())
            return;
        instance_->inputContextManager().foreach ([this](InputContext* ic) {
            auto state = ic->propertyFor(&factory_);
            state->setOption();
            if (ic->hasFocus())
                state->reset();
            return true;
        });
    }

    void vmkEngine::updateModeAction(InputContext* ic) {
        std::string currentModeStr = config_.mode.value();
        realMode                   = fcitx::modeStringToEnum(currentModeStr);
        globalMode_                = realMode;

        for (const auto& action : modeSubAction_) {
            action->setChecked(action->name() == "vmk-mode-" + currentModeStr);
            if (ic)
                action->update(ic);
        }
        modeAction_->setLongText(_("Chế độ gõ: ") + currentModeStr);

        if (ic) {
            modeAction_->update(ic);
        }
    }

    void vmkEngine::updateInputMethodAction(InputContext* ic) {
        auto name = stringutils::concat(InputMethodActionPrefix, *config_.inputMethod);
        for (const auto& action : inputMethodSubAction_) {
            action->setChecked(action->name() == name);
            if (ic)
                action->update(ic);
        }
        if (ic) {
            inputMethodAction_->setLongText(stringutils::concat("Input Method: ", *config_.inputMethod));
            inputMethodAction_->update(ic);
        }
    }

    void vmkEngine::updateCharsetAction(InputContext* ic) {
        auto name = stringutils::concat(CharsetActionPrefix, *config_.outputCharset);
        for (const auto& action : charsetSubAction_) {
            action->setChecked(action->name() == name);
            if (ic)
                action->update(ic);
        }
    }

    void vmkEngine::updateSpellAction(InputContext* ic) {
        spellCheckAction_->setChecked(*config_.spellCheck);
        spellCheckAction_->setShortText(*config_.spellCheck ? _("Spell Check: Bật") : _("Spell Check: Tắt"));
        if (ic) {
            spellCheckAction_->update(ic);
        }
    }

    void vmkEngine::updateMacroAction(InputContext* ic) {
        macroAction_->setChecked(*config_.macro);
        macroAction_->setShortText(*config_.macro ? _("Macro: Bật") : _("Macro: Tắt"));
        if (ic) {
            macroAction_->update(ic);
        }
    }

    void vmkEngine::updateCapitalizeMacroAction(InputContext* ic) {
        capitalizeMacroAction_->setChecked(*config_.capitalizeMacro);
        capitalizeMacroAction_->setShortText(*config_.capitalizeMacro ? _("Capitalize Macro: Bật") : _("Capitalize Macro: Tắt"));
        if (ic) {
            capitalizeMacroAction_->update(ic);
        }
    }

    void vmkEngine::updateAutoNonVnRestoreAction(InputContext* ic) {
        autoNonVnRestoreAction_->setChecked(*config_.autoNonVnRestore);
        autoNonVnRestoreAction_->setShortText(*config_.autoNonVnRestore ? _("Auto Non-VN Restore: Bật") : _("Auto Non-VN Restore: Tắt"));
        if (ic) {
            autoNonVnRestoreAction_->update(ic);
        }
    }

    void vmkEngine::updateModernStyleAction(InputContext* ic) {
        modernStyleAction_->setChecked(*config_.modernStyle);
        modernStyleAction_->setShortText(*config_.modernStyle ? _("Modern Style: Bật") : _("Modern Style: Tắt"));
        if (ic) {
            modernStyleAction_->update(ic);
        }
    }

    void vmkEngine::updateFreeMarkingAction(InputContext* ic) {
        freeMarkingAction_->setChecked(*config_.freeMarking);
        freeMarkingAction_->setShortText(*config_.freeMarking ? _("Free Marking: Bật") : _("Free Marking: Tắt"));
        if (ic) {
            freeMarkingAction_->update(ic);
        }
    }
    void vmkEngine::loadAppRules() {
        appRules_.clear();
        std::ifstream file(appRulesPath_);
        if (!file.is_open())
            return;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            auto delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string app     = line.substr(0, delimiterPos);
                std::string modeStr = line.substr(delimiterPos + 1);
                appRules_[app]      = fcitx::modeStringToEnum(modeStr);
            }
        }
        file.close();
    }

    void vmkEngine::saveAppRules() {
        std::ofstream file(appRulesPath_, std::ios::trunc);
        if (!file.is_open())
            return;

        file << "# VMK Per-App Configuration\n";
        for (const auto& pair : appRules_) {
            std::string modeStr = fcitx::modeEnumToString(pair.second);
            file << pair.first << "=" << modeStr << "\n";
        }
        file.close();
    }

    void vmkEngine::closeAppModeMenu() {
        isSelectingAppMode_ = false;
        g_mouse_clicked.store(false, std::memory_order_relaxed);
    }

    void vmkEngine::showAppModeMenu(InputContext* ic) {
        isSelectingAppMode_ = true;

        auto candidateList = std::make_unique<CommonCandidateList>();

        candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
        candidateList->setPageSize(7);

        fcitx::VMKMode currentAppRules = fcitx::VMKMode::Off;
        if (appRules_.count(currentConfigureApp_)) {
            currentAppRules = appRules_[currentConfigureApp_];
        } else {
            currentAppRules = globalMode_;
        }

        auto getLabel = [&](const fcitx::VMKMode& modeName, const std::string& modeLabel) {
            if (modeName == currentAppRules) {
                return Text(modeLabel + " (Mặc định)");
            } else {
                return Text(modeLabel);
            }
        };

        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(Text("Tên app nhận diện được bởi fcitx5: " + currentConfigureApp_)));
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(getLabel(fcitx::VMKMode::VMK1, "1. Fake backspace by Uinput")));
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(getLabel(fcitx::VMKMode::VMK2, "2. Surrounding Text")));
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(getLabel(fcitx::VMKMode::Preedit, "3. Preedit")));
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(getLabel(fcitx::VMKMode::VMK1HC, "4. VMK1HC")));
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(getLabel(fcitx::VMKMode::Off, "5. OFF - Tắt bộ gõ")));
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(Text("6. Xóa thiết lập cho app")));
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(Text("`. Tắt menu và gõ `")));

        ic->inputPanel().reset();
        ic->inputPanel().setCandidateList(std::move(candidateList));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }
} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::vmkFactory)

std::string SubstrChar(const std::string& s, size_t start, size_t len) {
    if (s.empty())
        return "";
    const char* start_ptr = fcitx_utf8_get_nth_char(s.c_str(), static_cast<uint32_t>(start));
    if (*start_ptr == '\0')
        return "";
    if (len == std::string::npos)
        return std::string(start_ptr);
    const char* end_ptr = fcitx_utf8_get_nth_char(start_ptr, static_cast<uint32_t>(len));
    return std::string(start_ptr, end_ptr - start_ptr);
}

int compareAndSplitStrings(const std::string& A, const std::string& B, std::string& commonPrefix, std::string& deletedPart, std::string& addedPart) {
    size_t lengthA     = fcitx_utf8_strlen(A.c_str());
    size_t lengthB     = fcitx_utf8_strlen(B.c_str());
    size_t minLength   = std::min(lengthA, lengthB);
    size_t matchLength = 0;
    for (size_t i = 0; i < minLength; ++i) {
        const char*  ptrA = fcitx_utf8_get_nth_char(A.c_str(), static_cast<uint32_t>(i));
        const char*  ptrB = fcitx_utf8_get_nth_char(B.c_str(), static_cast<uint32_t>(i));
        unsigned int lenA = fcitx_utf8_char_len(ptrA);
        unsigned int lenB = fcitx_utf8_char_len(ptrB);
        if (lenA == lenB && std::strncmp(ptrA, ptrB, lenA) == 0)
            ++matchLength;
        else
            break;
    }
    commonPrefix = SubstrChar(A, 0, matchLength);
    deletedPart  = SubstrChar(A, matchLength, std::string::npos);
    addedPart    = SubstrChar(B, matchLength, std::string::npos);
    return (deletedPart.empty() && addedPart.empty()) ? 1 : 2;
}