/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "vmk.h"
#include "ack-apps.h"

#include <cstdint>
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
#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <limits.h>
#include <mutex>
#endif

fcitx::VMKMode    realMode = fcitx::VMKMode::VMKSmooth;
std::atomic<bool> needEngineReset{false};
std::string       BASE_SOCKET_PATH;
// Global flag to signal mouse click for closing app mode menu
static std::atomic<bool> g_mouse_clicked{false};
std::atomic<bool>        is_deleting_{false};
static const int         MAX_BACKSPACE_COUNT = 8;
std::string              SubstrChar(const std::string& s, size_t start, size_t len);
int                      compareAndSplitStrings(const std::string& A, const std::string& B, std::string& commonPrefix, std::string& deletedPart, std::string& addedPart);
std::once_flag           monitor_init_flag;
std::atomic<bool>        stop_flag_monitor{false};
std::atomic<bool>        monitor_running{false};
int                      uinput_client_fd_ = -1;
int                      realtextLen       = 0;
bool                     waitAck           = false;
std::atomic<int>         mouse_socket_fd{-1};

std::atomic<int64_t>     replacement_start_ms_{0};
std::atomic<int>         replacement_thread_id_{0};
std::atomic<bool>        needFallbackCommit{false};

std::mutex               monitor_mutex;
std::condition_variable  monitor_cv;

//
std::string buildSocketPath(const char* base_path_suffix) {
    const char* username_c = std::getenv("USER");
    std::string path;
    path.reserve(32);
    path += "vmksocket-";
    path += (username_c ? username_c : "default");
    path += '-';
    path += base_path_suffix;
    return path;
}

static inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

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
        constexpr int              MAX_SCAN_LENGTH         = 15;

        std::string                macroFile(std::string_view imName) {
            return stringutils::concat("conf/vmk-macro-", imName, ".conf");
        }

        uintptr_t newMacroTable(const vmkMacroTable& macroTable) {
            const auto&        macros = *macroTable.macros;
            std::vector<char*> charArray;
            charArray.reserve(macros.size() * 2 + 1);
            for (const auto& keymap : macros) {
                charArray.push_back(const_cast<char*>(keymap.key->data()));
                charArray.push_back(const_cast<char*>(keymap.value->data()));
            }
            charArray.push_back(nullptr);
            return NewMacroTable(charArray.data());
        }

        std::vector<std::string> convertToStringList(char** array) {
            std::vector<std::string> result;
            int                      count = 0;
            for (int i = 0; array[i]; ++i)
                ++count;
            result.reserve(count);
            for (int i = 0; i < count; ++i) {
                result.emplace_back(array[i]);
                free(array[i]);
            }
            free(array);
            return result;
        }

        bool isWordBreak(uint32_t ucs4) {
            return ucs4 == ' ' || ucs4 == '\t' || ucs4 == '\n' || ucs4 == '\r' || ucs4 == 0 || // Null char
                (ucs4 < 65 && ucs4 > 57);
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
            realMode = modeStringToEnum(engine_->config().mode.value());

            if (engine_->config().inputMethod.value() == "Custom") {
                const auto&        keymaps = *engine_->customKeymap().customKeymap;
                std::vector<char*> charArray;
                charArray.reserve(keymaps.size() * 2 + 1);
                for (const auto& keymap : keymaps) {
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
                return true;
            }
            close(current_fd);
            uinput_client_fd_ = -1;
            return false;
        }

        int setup_uinput() {
            return connect_uinput_server() ? uinput_client_fd_ : -1;
        }

        void send_backspace_uinput(int count) {
            if (uinput_client_fd_ < 0 && !connect_uinput_server())
                return;
            if (count > MAX_BACKSPACE_COUNT)
                count = MAX_BACKSPACE_COUNT;

            if (uinput_client_fd_ < 0) {
                if (!connect_uinput_server())
                    return;
            }

            ssize_t n = send(uinput_client_fd_, &count, sizeof(count), MSG_NOSIGNAL);

            if (n < 0) {
                close(uinput_client_fd_);
                uinput_client_fd_ = -1;
                if (connect_uinput_server()) {
                    send(uinput_client_fd_, &count, sizeof(count), MSG_NOSIGNAL);
                }
            }

            if (waitAck) {
                char ack;
                recv(uinput_client_fd_, &ack, sizeof(ack), MSG_NOSIGNAL);
            }
        }

        void replayBufferToEngine(const std::string& buffer) {
            if (!vmkEngine_.handle())
                return;

            ResetEngine(vmkEngine_.handle());
            for (uint32_t c : fcitx::utf8::MakeUTF8CharRange(buffer)) {
                if (c == static_cast<uint32_t>('\b')) {
                    EngineProcessKeyEvent(vmkEngine_.handle(), FcitxKey_BackSpace, 0);
                } else {
                    EngineProcessKeyEvent(vmkEngine_.handle(), c, 0);
                }
            }
        }

        bool isAutofillCertain(const SurroundingText& s) {
            if (!s.isValid() || oldPreBuffer_.empty()) {
                return false;
            }

            int cursor = s.cursor();
            int anchor = s.anchor();

            // This alway is false
            if (cursor != anchor) {
                int selectionStart = std::min(anchor, cursor);
                int selectionEnd   = std::max(anchor, cursor);

                if (selectionStart >= cursor || (selectionStart < cursor && selectionEnd > cursor)) {
                    return true;
                }
            }

            const auto& text    = s.text();
            size_t      textLen = fcitx_utf8_strlen(text.c_str());

            // Guard?
            // if (textLen <= static_cast<size_t>(realtextLen))
            //     realtextLen = textLen;

            if (textLen == static_cast<size_t>(cursor)) {
                realtextLen = textLen;
                return false;
            }

            // Text exists after cursor AND cursor is exactly where we expected
            // (realtextLen tracks where cursor should be after our last commit).
            // This is the only reliable signal that the app appended a suggestion.
            // why -1?? cuz some SurroundingText is only update on event
            // so when u type "12345" surronding text is "1234" it will
            // update to "12345" when next u type "6" so this will make
            // work correctly when cursor in the middle of the string
            // Failed with:
            //              a.com[/] + 's' -> a.coóm // Bad
            //              a.co[m/] + 's' -> a.có   // Good
            if (textLen - 1 > static_cast<size_t>(cursor) && cursor == realtextLen)
                return true;

            if (realtextLen < cursor)
                realtextLen = cursor;

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

        // Helper function for emoji mode
        void updateEmojiPageStatus(CommonCandidateList* commonList) {
            if (!commonList || commonList->empty()) {
                return;
            }

            int pageSize = commonList->pageSize();
            if (pageSize <= 0) {
                pageSize = 9;
            }

            int         totalItems  = commonList->totalSize();
            int         currentPage = commonList->currentPage() + 1;
            int         totalPages  = (totalItems + pageSize - 1) / pageSize;

            std::string status = _("Page ") + std::to_string(currentPage) + "/" + std::to_string(totalPages);
            ic_->inputPanel().setAuxDown(Text(status));
        }

        void handleEmojiMode(KeyEvent& keyEvent) {
            if (keyEvent.key().hasModifier()) {
                keyEvent.forward();
                return;
            }

            const KeySym currentSym = keyEvent.rawKey().sym();

            auto         baseList   = ic_->inputPanel().candidateList();
            auto         commonList = std::dynamic_pointer_cast<CommonCandidateList>(baseList);
            if (commonList && currentSym >= FcitxKey_1 && currentSym <= FcitxKey_9) {
                int offset      = currentSym - FcitxKey_1;
                int globalIndex = commonList->currentPage() * commonList->pageSize() + offset;

                if (globalIndex < (int)commonList->totalSize()) {
                    commonList->candidateFromAll(globalIndex).select(ic_);
                    keyEvent.filterAndAccept();
                    return;
                }
            }

            if (commonList && !commonList->empty()) {
                int  globalCursorIndex = commonList->globalCursorIndex();
                int  totalSize         = commonList->totalSize();
                int  currentPage       = commonList->currentPage();
                int  pageSize          = commonList->pageSize();
                int  localCursorIndex  = globalCursorIndex - currentPage * pageSize;

                bool handled = false;

                switch (currentSym) {
                    case FcitxKey_Tab:
                    case FcitxKey_Down: {
                        if (globalCursorIndex == totalSize - 1) {
                            commonList->setGlobalCursorIndex(globalCursorIndex);
                        } else if (localCursorIndex < pageSize - 1) {
                            commonList->setGlobalCursorIndex(globalCursorIndex + 1);
                        } else {
                            commonList->next();
                            int newPage = commonList->currentPage();
                            commonList->setGlobalCursorIndex(newPage * pageSize);
                        }
                        handled = true;
                        break;
                    }

                    case FcitxKey_ISO_Left_Tab:
                    case FcitxKey_Up: {
                        if (globalCursorIndex == 0) {
                            commonList->setGlobalCursorIndex(globalCursorIndex);
                        } else if (localCursorIndex > 0) {
                            commonList->setGlobalCursorIndex(globalCursorIndex - 1);
                        } else {
                            commonList->prev();
                            int newPage  = commonList->currentPage();
                            int newIndex = newPage * pageSize + pageSize - 1;
                            commonList->setGlobalCursorIndex(newIndex);
                        }
                        handled = true;
                        break;
                    }
                    case FcitxKey_Page_Down:
                    case FcitxKey_Right: {
                        if (commonList->hasNext()) {
                            commonList->next();
                            int newPage = commonList->currentPage();
                            commonList->setGlobalCursorIndex(newPage * pageSize);
                            handled = true;
                        }
                        break;
                    }
                    case FcitxKey_Page_Up:
                    case FcitxKey_Left: {
                        if (commonList->hasPrev()) {
                            commonList->prev();
                            int newPage = commonList->currentPage();
                            commonList->setGlobalCursorIndex(newPage * pageSize);
                            handled = true;
                        }
                        break;
                    }
                    default: break;
                }

                if (handled) {
                    updateEmojiPageStatus(commonList.get());
                    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                    keyEvent.filterAndAccept();
                    return;
                }
            }

            if (isBackspace(currentSym)) {
                if (!emojiBuffer_.empty()) {
                    emojiBuffer_.pop_back();
                    while (!emojiBuffer_.empty() && (emojiBuffer_.back() & 0xC0) == 0x80) {
                        emojiBuffer_.pop_back();
                    }
                    keyEvent.filterAndAccept();
                } else {
                    keyEvent.forward();
                }
                updateEmojiPreedit();
                return;
            }

            switch (currentSym) {
                case FcitxKey_space:
                case FcitxKey_Return: {
                    if (commonList && !commonList->empty()) {
                        int globalIdx = commonList->globalCursorIndex();
                        commonList->candidateFromAll(globalIdx).select(ic_);
                        keyEvent.filterAndAccept();
                    } else if (currentSym == FcitxKey_Return && !emojiBuffer_.empty()) {
                        ic_->commitString(emojiBuffer_);
                        emojiBuffer_.clear();
                        ic_->inputPanel().reset();
                        ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                        keyEvent.filterAndAccept();
                    } else {
                        keyEvent.forward();
                    }
                    return;
                }

                case FcitxKey_Escape: {
                    emojiBuffer_.clear();
                    emojiCandidates_.clear();
                    ic_->inputPanel().reset();
                    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                    keyEvent.filterAndAccept();
                    return;
                }

                default: break;
            }

            {
                std::string utf8Char = Key::keySymToUTF8(currentSym);
                if (!utf8Char.empty()) {
                    emojiBuffer_ += utf8Char;
                    keyEvent.filterAndAccept();
                    updateEmojiPreedit();
                } else {
                    keyEvent.forward();
                }
            }
        }

        void selectEmojiCandidate(int index) {
            if (index >= 0 && index < static_cast<int>(emojiCandidates_.size())) {
                ic_->commitString(emojiCandidates_[index].output);
                emojiBuffer_.clear();
                emojiCandidates_.clear();
                ic_->inputPanel().reset();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
            }
        }

        void updateEmojiPreedit() {
            if (emojiBuffer_.empty()) {
                emojiCandidates_.clear();
                ic_->inputPanel().reset();
                ic_->updatePreedit();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                return;
            }

            emojiCandidates_ = engine_->emojiLoader().search(emojiBuffer_);

            if (!emojiBuffer_.empty()) {
                Text preeditText;
                preeditText.append(emojiBuffer_, TextFormatFlag::Underline);
                preeditText.setCursor(preeditText.textLength());
                if (ic_->capabilityFlags().test(CapabilityFlag::Preedit))
                    ic_->inputPanel().setClientPreedit(preeditText);
                else
                    ic_->inputPanel().setPreedit(preeditText);
            }

            if (!emojiCandidates_.empty()) {
                auto candidateList = std::make_unique<CommonCandidateList>();
                candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
                candidateList->setPageSize(9);

                for (size_t i = 0; i < emojiCandidates_.size(); ++i) {
                    int  localIndex = i % 9 + 1;
                    Text displayLabel(std::to_string(localIndex) + ": " + emojiCandidates_[i].trigger + " " + emojiCandidates_[i].output);
                    candidateList->append(std::make_unique<EmojiCandidateWord>(displayLabel, this, emojiCandidates_[i].output));
                }
                candidateList->setGlobalCursorIndex(0);

                ic_->inputPanel().setCandidateList(std::move(candidateList));
                auto currentList = std::dynamic_pointer_cast<CommonCandidateList>(ic_->inputPanel().candidateList());
                updateEmojiPageStatus(currentList.get());
            } else {
                ic_->inputPanel().setCandidateList(nullptr);
            }

            ic_->updatePreedit();
            ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
        }

        bool handleUInputKeyPress(KeyEvent& event, KeySym currentSym, int sleepTime) {
            if (!is_deleting_.load()) {
                return false;
            }
            if (isBackspace(currentSym)) {
                current_backspace_count_ += 1;
                if (current_backspace_count_ < expected_backspaces_) {
                    return false;
                } else {
                    is_deleting_.store(false);
                    replacement_start_ms_.store(0, std::memory_order_release);
                    replacement_thread_id_.store(0, std::memory_order_release);
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
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

        void performReplacement(const std::string& deletedPart, const std::string& addedPart) {
            int my_id                = ++current_thread_id_;
            current_backspace_count_ = 0;
            pending_commit_string_   = addedPart;
            const auto& surrounding  = ic_->surroundingText();
            expected_backspaces_     = utf8::length(deletedPart) + 1 + (isAutofillCertain(surrounding) ? 1 : 0);
            replacement_thread_id_.store(my_id, std::memory_order_release);
            replacement_start_ms_.store(now_ms(), std::memory_order_release);
            is_deleting_.store(true, std::memory_order_release);
            monitor_cv.notify_one();
            send_backspace_uinput(expected_backspaces_);
        }

        void checkForwardSpecialKey(KeyEvent& keyEvent, KeySym& currentSym) {
            if (keyEvent.key().isCursorMove() || currentSym == FcitxKey_Tab || currentSym == FcitxKey_KP_Tab || currentSym == FcitxKey_ISO_Left_Tab ||
                currentSym == FcitxKey_Escape || keyEvent.key().hasModifier()) {
                history_.clear();
                ResetEngine(vmkEngine_.handle());
                oldPreBuffer_.clear();
                keyEvent.forward();
                return;
            }

            if (currentSym == FcitxKey_Delete) {
                keyEvent.forward();
                return;
            }

            if (currentSym >= FcitxKey_KP_0 && currentSym <= FcitxKey_KP_9) {
                currentSym = static_cast<KeySym>(FcitxKey_0 + (currentSym - FcitxKey_KP_0));
            }

            switch (currentSym) {
                case FcitxKey_KP_Add: {
                    currentSym = FcitxKey_plus;
                    break;
                }
                case FcitxKey_KP_Subtract: {
                    currentSym = FcitxKey_minus;
                    break;
                }
                case FcitxKey_KP_Divide: {
                    currentSym = FcitxKey_slash;
                    break;
                }
                case FcitxKey_KP_Multiply: {
                    currentSym = FcitxKey_asterisk;
                    break;
                }
                case FcitxKey_KP_Decimal: {
                    currentSym = FcitxKey_period;
                    break;
                }
                case FcitxKey_KP_Enter: {
                    currentSym = FcitxKey_Return;
                    break;
                }
                case FcitxKey_KP_Equal: {
                    currentSym = FcitxKey_equal;
                    break;
                }
                case FcitxKey_KP_Space: {
                    currentSym = FcitxKey_space;
                    break;
                }
                default: break;
            }
        }

        // Helper function for vmk1/vmk1hc/vmksmooth mode
        void handleUinputMode(KeyEvent& keyEvent, KeySym currentSym, bool checkEmptyPreedit, int sleepTime) {
            checkForwardSpecialKey(keyEvent, currentSym);
            if (is_deleting_.load(std::memory_order_acquire)) {
                if (isBackspace(currentSym)) {
                    if (realtextLen > 0)
                        realtextLen -= 1;
                    if (handleUInputKeyPress(keyEvent, currentSym, sleepTime)) {
                        return;
                    }
                } else {
                    keyEvent.filterAndAccept();
                }
                return;
            }

            if (uinput_client_fd_ < 0) {
                setup_uinput();
            }

            if (isBackspace(currentSym) || currentSym == FcitxKey_Return) {
                if (isBackspace(currentSym)) {
                    history_.push_back('\b');
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

            std::string keyUtf8 = Key::keySymToUTF8(currentSym);
            if (keyUtf8.empty()) {
                keyEvent.forward();
                return;
            }

            bool processed = EngineProcessKeyEvent(vmkEngine_.handle(), currentSym, keyEvent.rawKey().states());

            auto commitF = UniqueCPtr<char>(EnginePullCommit(vmkEngine_.handle()));
            if (commitF && commitF.get()[0]) {
                std::string commitStr = commitF.get();
                std::string commonPrefix, deletedPart, addedPart;
                compareAndSplitStrings(oldPreBuffer_, commitStr, commonPrefix, deletedPart, addedPart);

                if (!deletedPart.empty()) {
                    performReplacement(deletedPart, addedPart);
                } else if (!addedPart.empty()) {
                    ic_->commitString(addedPart);
                }

                history_.clear();
                ResetEngine(vmkEngine_.handle());
                oldPreBuffer_.clear();

                keyEvent.filterAndAccept();
                return;
            }

            if (!processed) {
                if (checkEmptyPreedit) {
                    UniqueCPtr<char> preeditC(EnginePullPreedit(vmkEngine_.handle()));
                    if (!preeditC || !preeditC.get()[0]) {
                        history_.clear();
                        oldPreBuffer_.clear();
                        keyEvent.forward();
                    }
                }
                return;
            }

            history_ += keyUtf8;
            realtextLen += 1;

            replayBufferToEngine(history_);

            auto commitAfterReplay = UniqueCPtr<char>(EnginePullCommit(vmkEngine_.handle()));
            if (commitAfterReplay && commitAfterReplay.get()[0]) {
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
                    if (uinput_client_fd_ < 0) {
                        std::string rawKey = keyEvent.key().toString();
                        if (!rawKey.empty()) {
                            ic_->commitString(rawKey);
                        }
                        return;
                    }

                    if (is_deleting_.load()) {
                        is_deleting_.store(false, std::memory_order_release);
                    }

                    performReplacement(deletedPart, addedPart);
                    oldPreBuffer_ = preeditStr;
                }
            }
        }

        void handleSurroundingText(KeyEvent& keyEvent, KeySym currentSym) {
            checkForwardSpecialKey(keyEvent, currentSym);
            auto ic = keyEvent.inputContext();
            if (!ic || !ic->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
                keyEvent.forward();
                return;
            }

            const auto& surrounding = ic->surroundingText();
            if (!surrounding.isValid()) {
                keyEvent.forward();
                return;
            }

            if (isBackspace(keyEvent.rawKey().sym())) {
                ResetEngine(vmkEngine_.handle());
                keyEvent.forward();
                return;
            }

            if (surrounding.anchor() != surrounding.cursor()) {
                ic->deleteSurroundingText(0, 0);
            }

            const std::string& text   = surrounding.text();
            int                cursor = surrounding.cursor();

            size_t             textLen = utf8::lengthValidated(text);

            if (textLen == utf8::INVALID_LENGTH || cursor <= 0 || cursor > (int)textLen) {
                goto process_normal;
            }

            {
                auto startIter = utf8::nextNChar(text.begin(), cursor);
                auto endIter   = startIter;

                int  scanCount = 0;
                while (startIter != text.begin() && scanCount < MAX_SCAN_LENGTH) {
                    auto prev = startIter;
                    if (prev != text.begin()) {
                        do {
                            --prev;
                        } while (prev != text.begin() && ((*prev & 0xC0) == 0x80));
                    }

                    uint32_t ucs4 = utf8::getChar(prev, text.end());

                    if (isWordBreak(ucs4))
                        break;

                    startIter = prev;
                    ++scanCount;
                }

                std::string oldWord(startIter, endIter);

                if (oldWord.empty()) {
                    goto process_normal;
                }

                EngineRebuildFromText(vmkEngine_.handle(), oldWord.c_str());

                bool processed = EngineProcessKeyEvent(vmkEngine_.handle(), keyEvent.rawKey().sym(), keyEvent.rawKey().states());

                if (!processed) {
                    keyEvent.forward();
                    ResetEngine(vmkEngine_.handle());
                    return;
                }

                auto        commitPtr  = UniqueCPtr<char>(EnginePullCommit(vmkEngine_.handle()));
                auto        preeditPtr = UniqueCPtr<char>(EnginePullPreedit(vmkEngine_.handle()));

                std::string newWord = "";
                if (commitPtr && commitPtr.get()[0])
                    newWord += commitPtr.get();
                if (preeditPtr && preeditPtr.get()[0])
                    newWord += preeditPtr.get();

                std::string commonPrefix, deletedPart, addedPart;
                compareAndSplitStrings(oldWord, newWord, commonPrefix, deletedPart, addedPart);
                if (deletedPart.empty() && addedPart == keyEvent.key().toString()) {
                    ResetEngine(vmkEngine_.handle());
                    keyEvent.forward();
                    return;
                }

                if (!deletedPart.empty() || !addedPart.empty()) {
                    size_t charsToDelete = utf8::length(deletedPart);

                    if (charsToDelete > 0) {
                        ic->deleteSurroundingText(-static_cast<int>(charsToDelete), static_cast<int>(charsToDelete));
                    }

                    if (!addedPart.empty()) {
                        ic->commitString(addedPart);
                    }

                    ResetEngine(vmkEngine_.handle());
                    keyEvent.filterAndAccept();
                    return;
                }

                ResetEngine(vmkEngine_.handle());
                keyEvent.filterAndAccept();
                return;
            }

        process_normal:
            ResetEngine(vmkEngine_.handle());
            bool processed = EngineProcessKeyEvent(vmkEngine_.handle(), keyEvent.rawKey().sym(), keyEvent.rawKey().states());
            if (processed) {
                auto        commitPtr  = UniqueCPtr<char>(EnginePullCommit(vmkEngine_.handle()));
                auto        preeditPtr = UniqueCPtr<char>(EnginePullPreedit(vmkEngine_.handle()));
                std::string out        = "";
                if (commitPtr && commitPtr.get()[0])
                    out += commitPtr.get();
                if (preeditPtr && preeditPtr.get()[0])
                    out += preeditPtr.get();

                if (!out.empty())
                    ic->commitString(out);

                ResetEngine(vmkEngine_.handle());
                keyEvent.filterAndAccept();
            } else {
                keyEvent.forward();
            }
        }

        void keyEvent(KeyEvent& keyEvent) {
            if (!vmkEngine_ || keyEvent.isRelease())
                return;
            if (uinput_client_fd_ < 0) {
                connect_uinput_server();
            }
            if (current_backspace_count_ >= expected_backspaces_ && is_deleting_.load()) {
                is_deleting_.store(false);
                current_backspace_count_ = -1;
                expected_backspaces_     = 0;
            }
            if (needEngineReset.load() && (realMode == VMKMode::VMK1 || realMode == VMKMode::VMK2 || realMode == VMKMode::VMK1HC || realMode == VMKMode::VMKSmooth)) {
                oldPreBuffer_.clear();
                history_.clear();
                ResetEngine(vmkEngine_.handle());
                is_deleting_.store(false);
                current_backspace_count_ = -1;
                needEngineReset.store(false);
            }

            if (needFallbackCommit.load(std::memory_order_acquire)) {
                needFallbackCommit.store(false, std::memory_order_release);
                if (current_thread_id_.load(std::memory_order_acquire) == replacement_thread_id_.load(std::memory_order_acquire)) {
                    if (!pending_commit_string_.empty()) {
                        ic_->commitString(pending_commit_string_);
                        pending_commit_string_.clear();
                    }
                }
                replacement_thread_id_.store(0, std::memory_order_release);
                replacement_start_ms_.store(0, std::memory_order_release);
            }
            if (keyEvent.rawKey().check(FcitxKey_Shift_L) || keyEvent.rawKey().check(FcitxKey_Shift_R))
                return;
            const KeySym currentSym = keyEvent.rawKey().sym();

            switch (realMode) {
                case VMKMode::VMK1: {
                    handleUinputMode(keyEvent, currentSym, true, 20);
                    break;
                }
                case VMKMode::VMK1HC: {
                    handleUinputMode(keyEvent, currentSym, false, 20);
                    break;
                }
                case VMKMode::VMK2: {
                    handleSurroundingText(keyEvent, currentSym);
                    break;
                }
                case VMKMode::Preedit: {
                    handlePreeditMode(keyEvent);
                    break;
                }
                case VMKMode::Emoji: {
                    handleEmojiMode(keyEvent);
                    break;
                }
                case VMKMode::VMKSmooth: {
                    handleUinputMode(keyEvent, currentSym, true, 5);
                    break;
                }
                default: {
                    break;
                }
            }
        }

        void reset() {
            // TODO: This will report wrong when use mouse
            // click into the url bar that will select the cursor
            // pos now is 0 and realtextLen = textLen. Then the text
            // is select this not suggestion so can't sent 2 backspace
            // But in other case if click to the url bar in the first
            // place cursor pos is 0 and realtextLen = textLen too.
            // So we need get value of SurroundingText::selectedText()
            // but maybe can use that cus cursor alway = anchor in this
            // code base in vmk1 mode
            //
            // https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx/surroundingtext.cpp
            /*
              std::string SurroundingText::selectedText() const {
                FCITX_D();
                auto start = std::min(anchor(), cursor());
                auto end = std::max(anchor(), cursor());
                auto len = end - start;
                if (len == 0)
                    return {};
                auto startIter = utf8::nextNChar(d->text_.begin(), start);
                auto endIter = utf8::nextNChar(startIter, len);
                return std::string(startIter, endIter);
              }
              void SurroundingText::setText(const std::string &text, unsigned int cursor, unsigned int anchor) {
                FCITX_D();
                auto length = utf8::lengthValidated(text);
                if (length == utf8::INVALID_LENGTH || length < cursor || length < anchor) {
                    invalidate();
                    return;
                }
                d->valid_ = true;
                d->text_ = text;
                d->cursor_ = cursor;
                d->anchor_ = anchor;
                d->utf8Length_ = length;
            }
            void SurroundingText::setCursor(unsigned int cursor, unsigned int anchor) {
                FCITX_D();
                if (d->utf8Length_ < cursor || d->utf8Length_ < anchor) {
                    invalidate();
                    return;
                }
                d->cursor_ = cursor;
                d->anchor_ = anchor;
            }
            */

            const auto& surrounding = ic_->surroundingText();
            const auto& text        = surrounding.text();
            size_t      textLen     = fcitx_utf8_strlen(text.c_str());
            realtextLen             = textLen;
            if (is_deleting_.load(std::memory_order_acquire)) {
                return;
            }
            is_deleting_.store(false);

            // Commit pending preedit text before clearing buffers to prevent data loss.
            // Strategy: Call EngineCommitPreedit() first (for Preedit mode finalization),
            // then pull any remaining preedit text (for other modes like VMK1/VMKSmooth).
            // This ensures text is saved regardless of which mode we're switching from.
            if (vmkEngine_) {
                // Finalize preedit properly (required for Preedit mode)
                if (realMode == VMKMode::Preedit) {
                    EngineCommitPreedit(vmkEngine_.handle());
                    UniqueCPtr<char> commit(EnginePullCommit(vmkEngine_.handle()));
                    if (commit && commit.get()[0]) {
                        ic_->commitString(commit.get());
                    }
                }
            }

            clearAllBuffers();

            switch (realMode) {
                case VMKMode::Preedit: {
                    ic_->inputPanel().reset();
                    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                    ic_->updatePreedit();
                    break;
                }
                case VMKMode::VMK2:
                case VMKMode::VMK1:
                case VMKMode::VMK1HC:
                case VMKMode::VMKSmooth: {
                    ic_->inputPanel().reset();
                    break;
                }
                case VMKMode::Emoji: {
                    ic_->inputPanel().reset();
                    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                    ic_->updatePreedit();
                    break;
                }
                default: {
                    break;
                }
            }
        }

        void commitBuffer() {
            switch (realMode) {
                case VMKMode::Preedit: {
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
                case VMKMode::VMK1:
                case VMKMode::VMK1HC:
                case VMKMode::VMKSmooth: {
                    // For uinput modes: commit pending preedit when focus changes
                    // (e.g., when user tabs to another field while typing)
                    if (vmkEngine_) {
                        UniqueCPtr<char> preedit(EnginePullPreedit(vmkEngine_.handle()));
                        if (preedit && preedit.get()[0]) {
                            ic_->commitString(preedit.get());
                        }
                    }
                    break;
                }
                case VMKMode::VMK2: {
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
            if (is_deleting_.load(std::memory_order_acquire)) {
                return;
            }
            oldPreBuffer_.clear();
            history_.clear();
            if (!is_deleting_.load(std::memory_order_acquire)) {
                expected_backspaces_     = 0;
                current_backspace_count_ = 0;
                pending_commit_string_.clear();
            }
            emojiBuffer_.clear();
            emojiCandidates_.clear();
            if (vmkEngine_)
                ResetEngine(vmkEngine_.handle());
        }

        bool isEmptyHistory() {
            return history_.empty();
        }

      private:
        struct EmojiCandidateWord : public CandidateWord {
            VMKState*   state_;
            std::string emojiOutput_;
            EmojiCandidateWord(Text text, VMKState* state, const std::string& emojiOutput) : CandidateWord(std::move(text)), state_(state), emojiOutput_(emojiOutput) {}

            void select(InputContext* inputContext) const override {
                FCITX_UNUSED(inputContext);
                state_->ic_->commitString(emojiOutput_);

                state_->emojiBuffer_.clear();
                state_->emojiCandidates_.clear();

                state_->ic_->inputPanel().reset();
                state_->ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                state_->ic_->updatePreedit();
            }
        };
        vmkEngine*       engine_;
        InputContext*    ic_;
        CGoObject        vmkEngine_;
        std::string      oldPreBuffer_;
        std::string      history_;
        size_t           expected_backspaces_     = 0;
        size_t           current_backspace_count_ = 0;
        std::string      pending_commit_string_;
        std::atomic<int> current_thread_id_{0};
        // Emoji mode variables
        std::string             emojiBuffer_;
        std::vector<EmojiEntry> emojiCandidates_;
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

                    if (n > 0 && strcmp(exe_path, "/usr/bin/fcitx5-vmk-server") == 0) {
                        needEngineReset.store(true, std::memory_order_relaxed);
                        // Also signal that mouse was clicked to close app mode menu
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

    vmkEngine::vmkEngine(Instance* instance) : instance_(instance), factory_([this](InputContext& ic) { return new VMKState(this, &ic); }) {
        startMonitoringOnce();
        Init();
        {
            auto imNames = convertToStringList(GetInputMethodNames());
            imNames.push_back("Custom");
            imNames_ = std::move(imNames);
        }
        config_.inputMethod.annotation().setList(imNames_);
#if VMK_USE_MODERN_FCITX_API
        auto fd = StandardPaths::global().open(StandardPathsType::PkgData, "vmk/vietnamese.cm.dict");
#else
        auto fd = StandardPath::global().open(StandardPath::Type::PkgData, "vmk/vietnamese.cm.dict", O_RDONLY);
#endif
        if (!fd.isValid())
            throw std::runtime_error("Failed to load dictionary");
        dictionary_.reset(NewDictionary(fd.release()));

        auto& uiManager = instance_->userInterfaceManager();
        modeAction_     = std::make_unique<SimpleAction>();
        modeAction_->setIcon("preferences-system");
        modeAction_->setShortText(_("Typing Mode"));
        uiManager.registerAction("vmk-mode", modeAction_.get());
        modeMenu_ = std::make_unique<Menu>();
        modeAction_->setMenu(modeMenu_.get());

        std::vector<VMKMode> modes = {VMKMode::VMKSmooth, VMKMode::VMK1, VMKMode::VMK2, VMKMode::Preedit, VMKMode::VMK1HC};
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
                setMode(mode, ic);
                globalMode_ = mode;
                reloadConfig();
                updateModeAction(ic);
            }));
            modeMenu_->addAction(action.get());
            modeSubAction_.push_back(std::move(action));
        }

        inputMethodAction_ = std::make_unique<SimpleAction>();
        inputMethodAction_->setIcon("document-edit");
        inputMethodAction_->setShortText("Input Method");
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
                    ic->updateUserInterface(UserInterfaceComponent::StatusArea);
            }));
            inputMethodMenu_->addAction(action);
        }

        charsetAction_ = std::make_unique<SimpleAction>();
        charsetAction_->setShortText(_("Charset"));
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
                    ic->updateUserInterface(UserInterfaceComponent::StatusArea);
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

        fixVmk1WithAckAction_ = std::make_unique<SimpleAction>();
        fixVmk1WithAckAction_->setLongText(_("Fix uinput mode with ack"));
        fixVmk1WithAckAction_->setIcon("network-transmit-receive");
        fixVmk1WithAckAction_->setCheckable(true);
        connections_.emplace_back(fixVmk1WithAckAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.fixVmk1WithAck.setValue(!*config_.fixVmk1WithAck);
            saveConfig();
            refreshOption();
            updateFixVmk1WithAckAction(ic);
        }));
        uiManager.registerAction("vmk-fixvmk1withack", fixVmk1WithAckAction_.get());

        reloadConfig();
        globalMode_ = modeStringToEnum(config_.mode.value());
        updateModeAction(nullptr);
        instance_->inputContextManager().registerProperty("VMKState", &factory_);

#if VMK_USE_MODERN_FCITX_API
        std::string configDir = (StandardPaths::global().userDirectory(StandardPathsType::Config) / "fcitx5" / "conf").string();
#else
        std::string configDir = StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
#endif

        if (!std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
        }
        appRulesPath_ = configDir + "/vmk-app-rules.conf";
        loadAppRules();
    }

    vmkEngine::~vmkEngine() {
        stop_flag_monitor.store(true, std::memory_order_release);
        monitor_cv.notify_all();
        int fd = mouse_socket_fd.load(std::memory_order_acquire);
        if (fd >= 0) {
            close(fd);
        }
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
        updateFixVmk1WithAckAction(nullptr);
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

    std::string vmkEngine::subMode(const InputMethodEntry&, InputContext&) {
        return *config_.inputMethod;
    }

    void vmkEngine::activate(const InputMethodEntry& entry, InputContextEvent& event) {
        FCITX_UNUSED(entry);
        auto                     ic = event.inputContext();
        static std::atomic<bool> mouseThreadStarted{false};
        if (!mouseThreadStarted.exchange(true))
            startMouseReset();

        auto& statusArea = event.inputContext()->statusArea();
        if (ic->capabilityFlags().test(CapabilityFlag::Preedit))
            instance_->inputContextManager().setPreeditEnabledByDefault(true);

        // ibus-bamboo mode save/load
        std::string appName = ic->program();
        VMKMode     targetMode;

        if (!appRules_.empty() && appRules_.count(appName)) {
            targetMode = appRules_[appName];
        } else {
            targetMode = globalMode_;
        }
        reloadConfig();
        updateModeAction(event.inputContext());
        updateInputMethodAction(event.inputContext());
        updateCharsetAction(event.inputContext());

        if (*config_.fixVmk1WithAck) {
            if (targetMode == VMKMode::VMK1 || targetMode == VMKMode::VMK1HC || targetMode == VMKMode::VMKSmooth) {
                bool needWaitAck = false;
                for (const auto& ackApp : ack_apps) {
                    if (appName.find(ackApp) != std::string::npos) {
                        needWaitAck = true;
                        break;
                    }
                }
                waitAck = needWaitAck;
            }
        }

        setMode(targetMode, event.inputContext());

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
        statusArea.addAction(StatusGroup::InputMethod, fixVmk1WithAckAction_.get());
    }

    void vmkEngine::keyEvent(const InputMethodEntry& entry, KeyEvent& keyEvent) {
        FCITX_UNUSED(entry);
        auto ic = keyEvent.inputContext();

        // Handle mouse click event from fcitx5-vmk-server to close app mode menu
        // The server sends signal via Unix socket when user clicks outside the menu
        if (isSelectingAppMode_ && g_mouse_clicked.load(std::memory_order_relaxed)) {
            closeAppModeMenu();
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            auto state = ic->propertyFor(&factory_);
            state->reset();
        }

        // Handle keyboard input when app mode selection menu is active
        if (isSelectingAppMode_) {
            if (keyEvent.isRelease())
                return;

            auto   baseList = ic->inputPanel().candidateList();
            auto   menuList = std::dynamic_pointer_cast<CommonCandidateList>(baseList);
            KeySym keySym   = keyEvent.key().sym();

            // Lambda to move cursor in candidate list with wrap-around
            // Note: Index 0 is reserved for header ("App: ..."), so valid range is [1, totalSize-1]
            auto moveCursor = [&](int delta) {
                if (!menuList || menuList->empty()) {
                    return false;
                }

                int totalSize = menuList->totalSize();
                if (totalSize <= 1) {
                    return false;
                }

                int cursorIndex = menuList->globalCursorIndex();
                if (cursorIndex < 1 || cursorIndex >= totalSize) {
                    cursorIndex = 1;
                }

                int nextIndex = cursorIndex + delta;
                // Wrap around: bottom → top or top → bottom
                if (nextIndex < 1) {
                    nextIndex = totalSize - 1;
                } else if (nextIndex >= totalSize) {
                    nextIndex = 1;
                }

                menuList->setGlobalCursorIndex(nextIndex);
                ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                return true;
            };

            keyEvent.filterAndAccept();

            VMKMode selectedMode  = VMKMode::NoMode;
            bool    selectionMade = false;

            switch (keySym) {
                case FcitxKey_Tab:
                case FcitxKey_Down: {
                    if (moveCursor(1)) {
                        return;
                    }
                    break;
                }
                case FcitxKey_ISO_Left_Tab:
                case FcitxKey_Up: {
                    if (moveCursor(-1)) {
                        return;
                    }
                    break;
                }
                case FcitxKey_space:
                case FcitxKey_Return: {
                    if (menuList && !menuList->empty()) {
                        int selectedIndex = menuList->globalCursorIndex();
                        if (selectedIndex < 1 || selectedIndex >= menuList->totalSize()) {
                            selectedIndex = 1;
                        }
                        menuList->candidateFromAll(selectedIndex).select(ic);
                        return;
                    }
                    break;
                }
                    // Map keyboard shortcuts to modes
                    // Numbers [1-4]: VMK input modes, Letters [q/w/e/r]: Special modes/actions
                case FcitxKey_1: {
                    selectedMode = VMKMode::VMKSmooth;
                    break;
                }
                case FcitxKey_2: {
                    selectedMode = VMKMode::VMK1;
                    break;
                }
                case FcitxKey_3: {
                    selectedMode = VMKMode::VMK1HC;
                    break;
                }
                case FcitxKey_4: {
                    selectedMode = VMKMode::VMK2;
                    break;
                }
                case FcitxKey_q: {
                    selectedMode = VMKMode::Preedit;
                    break;
                }
                case FcitxKey_w: {
                    selectedMode = VMKMode::Emoji;
                    break;
                }
                case FcitxKey_e: {
                    selectedMode = VMKMode::Off;
                    break;
                }
                case FcitxKey_r: {
                    if (appRules_.count(currentConfigureApp_)) {
                        appRules_.erase(currentConfigureApp_);
                        saveAppRules();
                    }
                    selectionMade = true;
                    break;
                }
                case FcitxKey_Escape: {
                    selectionMade = true;
                    break;
                }
                case FcitxKey_grave: {
                    isSelectingAppMode_ = false;
                    ic->inputPanel().reset();
                    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                    auto state = ic->propertyFor(&factory_);
                    state->reset();
                    ic->commitString("`");
                    return;
                }
                default: break;
            }

            if (selectedMode != VMKMode::NoMode) {
                if (selectedMode != VMKMode::Emoji) {
                    appRules_[currentConfigureApp_] = selectedMode;
                    saveAppRules();
                }

                setMode(selectedMode, ic);
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

        // Open app mode selection menu when user presses backtick/grave key (`)
        // Triggered by Shift+` key combination, allows user to change input mode per-app
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
        const auto& s       = ic->surroundingText();
        const auto& text    = s.text();
        size_t      textLen = fcitx_utf8_strlen(text.c_str());
        int         cursor  = s.cursor();
        if (textLen == static_cast<size_t>(cursor))
            realtextLen = static_cast<int>(textLen);
    }

    void vmkEngine::reset(const InputMethodEntry& entry, InputContextEvent& event) {
        FCITX_UNUSED(entry);
        auto state = event.inputContext()->propertyFor(&factory_);
        if (!state->isEmptyHistory() && event.type() != EventType::InputContextFocusOut) {
            return;
        }

        if (event.type() == EventType::InputContextFocusOut) {
            state->reset();
        }
    }

    void vmkEngine::deactivate(const InputMethodEntry& entry, InputContextEvent& event) {
        FCITX_UNUSED(entry);
        auto state = event.inputContext()->propertyFor(&factory_);
        if (realMode == VMKMode::Preedit) {
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
        VMKMode     newMode        = modeStringToEnum(currentModeStr);
        globalMode_                = newMode;
        setMode(newMode, ic);

        for (const auto& action : modeSubAction_) {
            action->setChecked(action->name() == "vmk-mode-" + currentModeStr);
            if (ic)
                action->update(ic);
        }
        modeAction_->setLongText(_("Typing Mode: ") + currentModeStr);

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
        spellCheckAction_->setShortText(*config_.spellCheck ? _("Spell Check: On") : _("Spell Check: Off"));
        if (ic) {
            spellCheckAction_->update(ic);
        }
    }

    void vmkEngine::updateMacroAction(InputContext* ic) {
        macroAction_->setChecked(*config_.macro);
        macroAction_->setShortText(*config_.macro ? _("Macro: On") : _("Macro: Off"));
        if (ic) {
            macroAction_->update(ic);
        }
    }

    void vmkEngine::updateCapitalizeMacroAction(InputContext* ic) {
        capitalizeMacroAction_->setChecked(*config_.capitalizeMacro);
        capitalizeMacroAction_->setShortText(*config_.capitalizeMacro ? _("Capitalize Macro: On") : _("Capitalize Macro: Off"));
        if (ic) {
            capitalizeMacroAction_->update(ic);
        }
    }

    void vmkEngine::updateAutoNonVnRestoreAction(InputContext* ic) {
        autoNonVnRestoreAction_->setChecked(*config_.autoNonVnRestore);
        autoNonVnRestoreAction_->setShortText(*config_.autoNonVnRestore ? _("Auto Non-VN Restore: On") : _("Auto Non-VN Restore: Off"));
        if (ic) {
            autoNonVnRestoreAction_->update(ic);
        }
    }

    void vmkEngine::updateModernStyleAction(InputContext* ic) {
        modernStyleAction_->setChecked(*config_.modernStyle);
        modernStyleAction_->setShortText(*config_.modernStyle ? _("Modern Style: On") : _("Modern Style: Off"));
        if (ic) {
            modernStyleAction_->update(ic);
        }
    }

    void vmkEngine::updateFreeMarkingAction(InputContext* ic) {
        freeMarkingAction_->setChecked(*config_.freeMarking);
        freeMarkingAction_->setShortText(*config_.freeMarking ? _("Free Marking: On") : _("Free Marking: Off"));
        if (ic) {
            freeMarkingAction_->update(ic);
        }
    }

    void vmkEngine::updateFixVmk1WithAckAction(InputContext* ic) {
        fixVmk1WithAckAction_->setChecked(*config_.fixVmk1WithAck);
        fixVmk1WithAckAction_->setShortText(*config_.fixVmk1WithAck ? _("Fix Vmk1 With Ack: On") : _("Fix Vmk1 With Ack: Off"));
        if (ic) {
            fixVmk1WithAckAction_->update(ic);
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
                appRules_[app]      = modeStringToEnum(modeStr);
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
            std::string modeStr = modeEnumToString(pair.second);
            file << pair.first << "=" << modeStr << "\n";
        }
        file.close();
    }

    void vmkEngine::closeAppModeMenu() {
        isSelectingAppMode_ = false;
        g_mouse_clicked.store(false, std::memory_order_relaxed);
    }

    namespace {
        // Custom candidate word class that supports both mouse click and keyboard selection
        // Unlike DisplayOnlyCandidateWord, this executes a callback when selected,
        // enabling interactive menu items in the app mode selection UI
        class AppModeCandidateWord : public CandidateWord {
          public:
            AppModeCandidateWord(Text text, std::function<void(InputContext*)> callback) : CandidateWord(std::move(text)), callback_(std::move(callback)) {}

            void select(InputContext* ic) const override {
                if (callback_) {
                    callback_(ic);
                }
            }

          private:
            std::function<void(InputContext*)> callback_;
        };
    } // namespace

    void vmkEngine::showAppModeMenu(InputContext* ic) {
        isSelectingAppMode_ = true;

        auto candidateList = std::make_unique<CommonCandidateList>();

        candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
        candidateList->setPageSize(10);

        // Helper lambda: Add ">>" marker to highlight current active mode
        auto getLabel = [&](const VMKMode& modeName, const std::string& modeLabel) {
            if (modeName == realMode) {
                return Text(">> " + modeLabel);
            } else {
                return Text("   " + modeLabel);
            }
        };

        // Helper lambda: Cleanup after mode selection (reset UI and commit pending text)
        auto cleanup = [this](InputContext* ic) {
            isSelectingAppMode_ = false;
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            auto state = ic->propertyFor(&factory_);
            state->reset(); // This will commit any pending preedit text
        };

        // Helper lambda: Create callback to apply selected mode and save app-specific settings
        // Note: Emoji mode is transient (not saved to appRules), user must explicitly
        // select it each time they open the menu
        auto applyMode = [this, cleanup](VMKMode mode) {
            return [this, mode, cleanup](InputContext* ic) {
                if (mode != VMKMode::Emoji) {
                    appRules_[currentConfigureApp_] = mode;
                    saveAppRules();
                }

                setMode(mode, ic);
                cleanup(ic);
            };
        };

        // Build candidate list for app mode menu
        // Structure: Header + 8 selectable items (4 VMK modes + 4 special options)
        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(Text(_("App: ") + currentConfigureApp_)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(VMKMode::VMKSmooth, _("[1] Uinput (smooth)")), applyMode(VMKMode::VMKSmooth)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(VMKMode::VMK1, _("[2] Uinput (Slow)")), applyMode(VMKMode::VMK1)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(VMKMode::VMK1HC, _("[3] Uinput (Hardcore)")), applyMode(VMKMode::VMK1HC)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(VMKMode::VMK2, _("[4] Surrounding Text")), applyMode(VMKMode::VMK2)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(VMKMode::Preedit, _("[q] Preedit")), applyMode(VMKMode::Preedit)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(VMKMode::Emoji, _("[w] Emoji Picker")), applyMode(VMKMode::Emoji)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(VMKMode::Off, _("[e] OFF")), applyMode(VMKMode::Off)));

        candidateList->append(std::make_unique<AppModeCandidateWord>(Text(_("[r] Remove App Settings")), [this, cleanup](InputContext* ic) {
            if (appRules_.count(currentConfigureApp_)) {
                appRules_.erase(currentConfigureApp_);
                saveAppRules();
            }
            cleanup(ic);
        }));

        candidateList->append(std::make_unique<AppModeCandidateWord>(Text(_("[`] Type `")), [cleanup](InputContext* ic) {
            cleanup(ic);
            ic->commitString("`");
        }));

        // Set initial cursor position to highlight current active mode
        // Index mapping: 1=VMKSmooth, 2=VMK1, 3=VMK1HC, 4=VMK2, 5=Preedit, 6=Emoji, 7=Off
        int selectedIndex = 1;
        switch (realMode) {
            case VMKMode::VMKSmooth: selectedIndex = 1; break;
            case VMKMode::VMK1: selectedIndex = 2; break;
            case VMKMode::VMK1HC: selectedIndex = 3; break;
            case VMKMode::VMK2: selectedIndex = 4; break;
            case VMKMode::Preedit: selectedIndex = 5; break;
            case VMKMode::Emoji: selectedIndex = 6; break;
            case VMKMode::Off: selectedIndex = 7; break;
            default: selectedIndex = 1; break;
        }
        candidateList->setGlobalCursorIndex(selectedIndex);

        ic->inputPanel().reset();
        ic->inputPanel().setCandidateList(std::move(candidateList));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void vmkEngine::setMode(VMKMode mode, InputContext* ic) {
        realMode = mode;
        if (ic) {
            ic->updateUserInterface(UserInterfaceComponent::StatusArea);
        }
    }

    std::string vmkEngine::overrideIcon(const fcitx::InputMethodEntry& /*entry*/) {
        switch (realMode) {
            case VMKMode::Off: return "fcitx-vmk-off";
            case VMKMode::Emoji: return "fcitx-vmk-emoji";
            default: return {};
        }
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
    const char* ptrA   = A.c_str();
    const char* ptrB   = B.c_str();
    const char* endA   = ptrA + A.size();
    const char* endB   = ptrB + B.size();
    const char* startA = ptrA;

    while (ptrA < endA && ptrB < endB) {
        unsigned int lenA = fcitx_utf8_char_len(ptrA);
        unsigned int lenB = fcitx_utf8_char_len(ptrB);
        if (lenA == lenB && std::strncmp(ptrA, ptrB, lenA) == 0) {
            ptrA += lenA;
            ptrB += lenB;
        } else {
            break;
        }
    }

    commonPrefix.assign(startA, ptrA);
    deletedPart.assign(ptrA, endA);
    addedPart.assign(ptrB, endB);
    return (deletedPart.empty() && addedPart.empty()) ? 1 : 2;
}
