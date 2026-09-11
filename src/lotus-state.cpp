/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-state.h"
#include "lotus-engine.h"
#include "lotus-candidates.h"
#include "lotus-utils.h"
#include "lotus.h"

#include <cstddef>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>
#include <fcitx/menu.h>
#include <fcitx/userinterface.h>

#include <algorithm>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

#include <thread>

namespace fcitx {
    constexpr int      MAX_SCAN_LENGTH = 15;

    static inline bool isWordBreak(uint32_t ucs4) {
        // Space, tab, newline, carriage return, null, or punctuation/symbols (: ; < = > ? @)
        return ucs4 == ' ' || ucs4 == '\t' || ucs4 == '\n' || ucs4 == '\r' || ucs4 == 0 || (ucs4 >= 58 && ucs4 <= 64);
    }

    LotusState::LotusState(LotusEngine* engine, InputContext* ic) : engine_(engine), ic_(ic) {
        setEngine();
    }

    void LotusState::setEngine() {
        lotusEngine_.reset();
        realMode = engine_->config().mode.value();

        if (engine_->config().inputMethod.value() == "Custom") {
            const auto&        keymaps = *engine_->customKeymap().customKeymap;
            std::vector<char*> charArray;
            charArray.reserve((keymaps.size() * 2) + 1);
            for (const auto& keymap : keymaps) {
                charArray.push_back(const_cast<char*>(keymap.key->data()));   //NOLINT
                charArray.push_back(const_cast<char*>(keymap.value->data())); //NOLINT
            }
            charArray.push_back(nullptr);
            lotusEngine_.reset(NewCustomEngine(charArray.data(), engine_->dictionary(), engine_->macroTable()));
        } else {
            lotusEngine_.reset(NewEngine(engine_->config().inputMethod->data(), engine_->dictionary(), engine_->macroTable()));
        }
        setOption();
        resetMacroSkip();
    }

    void LotusState::setOption() {
        if (!lotusEngine_)
            return;
        FcitxBambooEngineOption option = {
            .autoNonVnRestore    = *engine_->config().autoNonVnRestore,
            .ddFreeStyle         = *engine_->config().ddFreeStyle,
            .macroEnabled        = *engine_->config().enableMacro,
            .autoCapitalizeMacro = *engine_->config().capitalizeMacro,
            .spellCheckWithDicts = *engine_->config().spellCheck,
            .outputCharset       = engine_->config().outputCharset->data(),
            .modernStyle         = *engine_->config().modernStyle,
            .freeMarking         = *engine_->config().freeMarking,
            .w2u                 = static_cast<int>(*engine_->config().w2u),
            .bracketTransform    = static_cast<int>(*engine_->config().bracketTransform),
            .timeFormat          = engine_->config().timeFormat->data(),
            .dateFormat          = engine_->config().dateFormat->data(),
        };

        EngineSetOption(lotusEngine_.handle(), &option);
    }

    bool LotusState::connect_uinput_server() {
        if (uinput_client_fd_ >= 0)
            return true;
        const std::string current_path = buildSocketPath("kb_socket");
        int               current_fd   = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
        if (current_fd < 0) {
            LOTUS_ERROR("Failed to create socket: " + std::string(strerror(errno)));
            return false;
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;

        addr.sun_path[0] = '\0';
        memcpy(&addr.sun_path[1], current_path.c_str(), current_path.length());
        socklen_t len = offsetof(struct sockaddr_un, sun_path) + current_path.length() + 1;

        if (connect(current_fd, (struct sockaddr*)&addr, len) == 0) {
            uinput_client_fd_ = current_fd;
            return true;
        }
        LOTUS_ERROR("Failed to connect to socket: " + std::string(strerror(errno)));
        close(current_fd);
        int old_fd = uinput_client_fd_.exchange(-1);
        if (old_fd != -1) {
            close(old_fd);
        }
        return false;
    }

    int LotusState::setup_uinput() {
        return connect_uinput_server() ? uinput_client_fd_.load(std::memory_order_acquire) : -1;
    }

    void LotusState::send_backspace_uinput(int count) const {
        if (uinput_client_fd_ < 0 && !connect_uinput_server()) {
            LOTUS_ERROR("Cannot send backspace since cannot connect to uinput server");
            return;
        }

        ssize_t n = send(uinput_client_fd_, &count, sizeof(count), MSG_NOSIGNAL);

        if (n < 0) {
            LOTUS_WARN("Failed to send backspace: " + std::string(strerror(errno)));
            int old_fd = uinput_client_fd_.exchange(-1);
            if (old_fd != -1) {
                close(old_fd);
            }
            if (connect_uinput_server()) {
                LOTUS_INFO("Reconnected to uinput server successfully");
                send(uinput_client_fd_, &count, sizeof(count), MSG_NOSIGNAL);
            }
        }

        if (waitAck_) {
            LOTUS_INFO("Waiting for ack");
            std::this_thread::sleep_for(std::chrono::milliseconds(count * 5));
        }
    }

    bool LotusState::isAutofillCertain(const SurroundingText& s) {
        if (!s.isValid() || oldPreBuffer_.empty()) {
            return false;
        }

        auto               textRange         = fcitx::utf8::MakeUTF8CharRange(s.text());
        auto               oldPreBufferRange = fcitx::utf8::MakeUTF8CharRange(oldPreBuffer_);
        std::u32string     u32Text(textRange.begin(), textRange.end());
        std::u32string     u32OldPreBuffer(oldPreBufferRange.begin(), oldPreBufferRange.end());

        const unsigned int cursor  = s.cursor();
        const unsigned int anchor  = s.anchor();
        const size_t       textLen = u32Text.length();

        // Fix that surrounding text is delay update
        const size_t buffLen    = u32OldPreBuffer.length();
        const size_t pb         = u32Text.find(u32OldPreBuffer);
        size_t       rangeStart = static_cast<size_t>(cursor) >= buffLen ? static_cast<size_t>(cursor) - buffLen : 0;
        const bool   sameprefix = pb != std::u32string::npos && pb >= rangeStart && pb <= static_cast<size_t>(cursor);

        // Detect browser autofill/autocomplete suggestions via selection.
        if (cursor != anchor) {
            unsigned int selectionStart = std::min(anchor, cursor);
            unsigned int selectionEnd   = std::max(anchor, cursor);

            // Only consider it browser autofill if the selection starts at the cursor
            // and extends to the end of the line (common address bar behavior).
            if (selectionStart >= cursor || (selectionStart < cursor && selectionEnd > cursor)) {
                if (!sameprefix)
                    return false;
                // If the selection contains a newline, it's likely a multiline editor (AI ghost text),
                // not a single-line URL/Search bar.
                size_t p = u32Text.find(U'\n', selectionStart);
                return p == std::u32string::npos || p >= static_cast<size_t>(selectionEnd);
            }
        }

        if (textLen == static_cast<size_t>(cursor)) {
            realtextLen.store(textLen, std::memory_order_release);
            return false;
        }

        // Heuristic: rapid text growth in a single-line context.
        // Applied only when no newline is present after the cursor to distinguish from AI text in editors.
        // Gecko/Firefox: if buffLen > textLen, surrounding text is stale (async update race)
        if (buffLen > textLen) {
            return false;
        }
        if (textLen > static_cast<size_t>(cursor) + 1 && cursor == realtextLen.load(std::memory_order_acquire) && u32Text.find(U'\n', cursor) == std::u32string::npos && sameprefix)
            return true;

        for (auto v = realtextLen.load(std::memory_order_acquire); v < cursor && !realtextLen.compare_exchange_weak(v, cursor, std::memory_order_acq_rel);)
            ;
        return false;
    }

    void LotusState::handlePreeditMode(KeyEvent& keyEvent, KeySym currentSym) {
        if (EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U)
            keyEvent.filterAndAccept();
        if (auto commit = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()))) {
            if (commit && (*commit.get() != 0)) {
                LOTUS_INFO("Commit: " + std::string(commit.get()));
                ic_->commitString(commit.get());
            }
        }
        ic_->inputPanel().reset();
        UniqueCPtr<char> preedit(EnginePullPreedit(lotusEngine_.handle()));
        if (preedit && (*preedit.get() != 0)) {
            std::string_view view = preedit.get();
            Text             text;
            TextFormatFlags  fmt = TextFormatFlag::NoFlag;
            if (utf8::validate(view))
                text.append(std::string(view), fmt);
            text.setCursor(static_cast<int>(text.textLength()));
            if (ic_->capabilityFlags().test(CapabilityFlag::Preedit))
                ic_->inputPanel().setClientPreedit(text);
            else
                ic_->inputPanel().setPreedit(text);
        }
        ic_->updatePreedit();
        ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void LotusState::updateEmojiPageStatus(CommonCandidateList* commonList) {
        if ((commonList == nullptr) || commonList->empty()) {
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

    void LotusState::handleEmojiMode(KeyEvent& keyEvent) {
        const KeySym currentSym      = keyEvent.rawKey().sym();
        bool         isCtrlBackspace = isBackspace(currentSym) && ((keyEvent.rawKey().states() & KeyState::Ctrl) != 0U);

        if (keyEvent.key().hasModifier() && !isCtrlBackspace) {
            keyEvent.forward();
            return;
        }

        auto baseList   = ic_->inputPanel().candidateList();
        auto commonList = std::dynamic_pointer_cast<CommonCandidateList>(baseList);
        if (commonList && currentSym >= FcitxKey_1 && currentSym <= FcitxKey_9) {
            int offset      = currentSym - FcitxKey_1;
            int globalIndex = (commonList->currentPage() * commonList->pageSize()) + offset;

            if (globalIndex < commonList->totalSize()) {
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
            int  localCursorIndex  = globalCursorIndex - (currentPage * pageSize);

            bool handled = false;

            switch (currentSym) {
                case FcitxKey_Tab:
                case FcitxKey_Down: {
                    if (localCursorIndex < pageSize - 1 && globalCursorIndex < totalSize - 1) {
                        commonList->setGlobalCursorIndex(globalCursorIndex + 1);
                    } else {
                        commonList->setGlobalCursorIndex(currentPage * pageSize);
                    }
                    handled = true;
                    break;
                }

                case FcitxKey_ISO_Left_Tab:
                case FcitxKey_Up: {
                    if (localCursorIndex > 0) {
                        commonList->setGlobalCursorIndex(globalCursorIndex - 1);
                    } else {
                        int lastIndex = std::min((currentPage * pageSize) + pageSize - 1, totalSize - 1);
                        commonList->setGlobalCursorIndex(lastIndex);
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
                if (isCtrlBackspace) {
                    emojiBuffer_.clear();
                } else {
                    eraseLastUtf8Codepoint(emojiBuffer_);
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
                    updateEmojiPreedit();
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
                emojiBuffer_.append(utf8Char);
                keyEvent.filterAndAccept();
                updateEmojiPreedit();
            } else {
                keyEvent.forward();
            }
        }
    }
    void LotusState::updateEmojiPreedit() {
        if (emojiBuffer_.empty()) {
            emojiCandidates_ = engine_->emojiLoader().history();
            if (emojiCandidates_.empty()) {
                ic_->inputPanel().reset();
                ic_->updatePreedit();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                return;
            }
        } else {
            emojiCandidates_ = engine_->emojiLoader().search(emojiBuffer_);
        }

        if (!emojiBuffer_.empty()) {
            Text preeditText;
            preeditText.append(emojiBuffer_, TextFormatFlag::Underline);
            preeditText.setCursor(static_cast<int>(preeditText.textLength()));
            if (ic_->capabilityFlags().test(CapabilityFlag::Preedit))
                ic_->inputPanel().setClientPreedit(preeditText);
            else
                ic_->inputPanel().setPreedit(preeditText);
        } else {
            ic_->inputPanel().setClientPreedit(Text());
            ic_->inputPanel().setPreedit(Text());
        }

        if (!emojiCandidates_.empty()) {
            auto candidateList = std::make_unique<CommonCandidateList>();
            candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
            candidateList->setPageSize(9);

            for (size_t i = 0; i < emojiCandidates_.size(); ++i) {
                size_t localIndex = (i % 9) + 1;
                Text   displayLabel;
                if (emojiBuffer_.empty()) {
                    displayLabel.append(std::to_string(localIndex) + ": " + emojiCandidates_[i].output, TextFormatFlag::NoFlag);
                } else {
                    displayLabel.append(std::to_string(localIndex) + ": " + emojiCandidates_[i].trigger + " " + emojiCandidates_[i].output, TextFormatFlag::NoFlag);
                }
                candidateList->append(std::make_unique<EmojiCandidateWord>(displayLabel, this, emojiCandidates_[i]));
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

    bool LotusState::handleUInputKeyPress(KeyEvent& event, KeySym currentSym, int sleepTime) {
        if (!is_deleting_.load()) {
            return false;
        }
        if (isBackspace(currentSym)) {
            current_backspace_count_ += 1;
            if (current_backspace_count_ < expected_backspaces_) {
                return false; // Allow intermediate backspaces to reach the app to clear autofill/old text.
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * (expected_backspaces_ - 1)));
            // Validate surr cursor pos should match realtextLen after all BS applied
            const auto& surr = ic_->surroundingText();
            if (surr.isValid() && surr.cursor() == realtextLen.load(std::memory_order_acquire)) {
                LOTUS_INFO("Skip retry");
            } else {
                // Retry x3 (2 ms each), khi can (chromium,electron,...)
                for (int retry = 0; retry < 3; ++retry) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    const auto& surr2 = ic_->surroundingText();
                    if (surr2.isValid() && surr2.cursor() == realtextLen.load(std::memory_order_acquire)) {
                        break;
                    }
                }
            }
            ic_->commitString(pending_commit_string_);
            LOTUS_INFO("Commit: " + pending_commit_string_);
            lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();

            event.filterAndAccept(); // Filter out the final trigger backspace.
            is_deleting_.store(false);
            replayBufferedKeys();
            return true;
        }
        return false;
    }

    void LotusState::performReplacement(const std::string& deletedPart, const std::string& addedPart) {
        LOTUS_INFO("Perform replacement: " + deletedPart + " -> " + addedPart); //NOLINT
        current_backspace_count_      = 0;
        pending_commit_string_        = addedPart;
        expected_backspaces_          = static_cast<int>(utf8::length(deletedPart));
        const auto&       surrounding = ic_->surroundingText();
        const std::string surrText    = surrounding.text();
        bool isSurrText = engine_->config().useSurroundingTextIfPossible.value() && ic_->capabilityFlags().test(CapabilityFlag::SurroundingText) && surrounding.isValid() &&
            !surrText.empty() && surrounding.cursor() == utf8::length(surrText);
        if (!isSurrText && realMode != LotusMode::Minecraft) {
            ++expected_backspaces_;
            if (realMode != LotusMode::SuperSmooth) {
                // Enable Autofill detection for all frontends (Wayland/IBus).
                // This fixes the "toôi" duplication bug in Chromium-based search bars.
                // The isAutofillCertain function has been optimized to differentiate
                // between browser autofill and AI ghost text.
                if (isAutofillCertain(surrounding)) {
                    ++expected_backspaces_;
                }
            }
        }
        is_deleting_.store(true, std::memory_order_release);
        if (isSurrText) {
            ic_->deleteSurroundingText(-expected_backspaces_, expected_backspaces_);
            LOTUS_INFO("Delete using surrounding text");
            std::this_thread::sleep_for(std::chrono::milliseconds(4 * expected_backspaces_));
            if (!pending_commit_string_.empty()) {
                ic_->commitString(pending_commit_string_);
                LOTUS_INFO("Commit: " + pending_commit_string_);
                lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                std::this_thread::sleep_for(std::chrono::milliseconds(3 * utf8::length(addedPart)));
            }
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            is_deleting_.store(false);
            replayBufferedKeys();
            return;
        }
        send_backspace_uinput(expected_backspaces_);
        LOTUS_INFO("Send " + std::to_string(expected_backspaces_) + " backspaces");
    }

    bool LotusState::checkForwardSpecialKey(KeyEvent& keyEvent, KeySym& currentSym) {
        if (keyEvent.key().isCursorMove() || currentSym == FcitxKey_Tab || currentSym == FcitxKey_KP_Tab || currentSym == FcitxKey_ISO_Left_Tab || currentSym == FcitxKey_Escape ||
            keyEvent.key().hasModifier()) {
            // Chặn reset ngoài ý muốn: Nếu đang trong tiến trình xóa uinput hoặc trong cửa sổ 50ms sau commit,
            // các phím điều hướng hoặc phím tắt có modifier không được phép reset engine hay xóa buffer preedit,
            // chỉ đơn thuần forward phím ra ngoài ứng dụng.
            if (shouldRejectReset()) {
                return true;
            }
            is_deleting_.store(false, std::memory_order_release);
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();
            return true;
        }

        if (currentSym == FcitxKey_Delete) {
            return true;
        }

        if (currentSym >= FcitxKey_KP_0 && currentSym <= FcitxKey_KP_9) {
            currentSym = static_cast<KeySym>(FcitxKey_0 + (currentSym - FcitxKey_KP_0));
            return false;
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
        return false;
    }

    void LotusState::handleUinputMode(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }

        if (uinput_client_fd_ < 0) {
            setup_uinput();
        }

        if (isBackspace(currentSym) || currentSym == FcitxKey_Return) {
            if (isBackspace(currentSym)) {
                hasHistory_ = true;
                EngineProcessKeyEvent(lotusEngine_.handle(), FcitxKey_BackSpace, 0);
                UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
                oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
            } else {
                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
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

        bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;

        auto commitF = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
        if (commitF && (*commitF.get() != 0)) {
            std::string commitStr = commitF.get();
            std::string deletedPart;
            std::string addedPart;
            compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

            if (!deletedPart.empty()) {
                performReplacement(deletedPart, addedPart);
                keyEvent.filterAndAccept();
            } else {
                bool wasAutoCapitalized = (currentSym != keyEvent.rawKey().sym());
                if (!addedPart.empty() && (keyUtf8 != addedPart || wasAutoCapitalized)) {
                    // Prevent auto-capitalized character replacement from stripping out Vietnamese chars
                    if (addedPart.size() > 1 && addedPart.back() == ' ') {
                        // Stripping the trigger key (space) from addedPart
#if __cplusplus >= 202002L
                        addedPart.resize(addedPart.size() - 1);
#else
                        addedPart = addedPart.substr(0, addedPart.size() - 1);
#endif
                    }
                    ic_->commitString(addedPart);
                    LOTUS_INFO("Commit: " + addedPart);
                    keyEvent.filterAndAccept();
                } else {
                    keyEvent.forward();
                }
            }

            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();

            return;
        }

        if (!processed) {
            UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
            if (!preeditC || (*preeditC.get() == 0)) {
                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
                keyEvent.forward();
            }
            return;
        }

        hasHistory_ = true;
        realtextLen.fetch_add(1, std::memory_order_acq_rel);

        UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
        std::string      preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

        std::string      deletedPart;
        std::string      addedPart;

        if (wa_chromium_flag)
            keyEvent.filterAndAccept();

        if (compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) != 0) {
            if (deletedPart.empty()) {
                bool isCommit           = false;
                bool wasAutoCapitalized = (currentSym != keyEvent.rawKey().sym());
                if (!addedPart.empty()) {
                    oldPreBuffer_ = preeditStr;
                    if (wa_chromium_flag || wasAutoCapitalized || addedPart != keyUtf8) {
                        ic_->commitString(addedPart);
                        LOTUS_INFO("Commit: " + addedPart);
                        if (!wa_chromium_flag) {
                            keyEvent.filterAndAccept();
                            isCommit = true;
                        }
                    }
                }
                if (!wa_chromium_flag && !isCommit) {
                    keyEvent.forward();
                }
            } else {
                if (uinput_client_fd_ < 0) {
                    LOTUS_ERROR("Cannot connect to uinput server, commit rawkey");
                    std::string rawKey = keyEvent.key().toString();
                    if (!rawKey.empty()) {
                        ic_->commitString(rawKey);
                    }
                    return;
                }

                if (is_deleting_.load()) {
                    is_deleting_.store(false, std::memory_order_release);
                }

                if (!wa_chromium_flag)
                    keyEvent.filterAndAccept();
                performReplacement(deletedPart, addedPart);
                oldPreBuffer_ = preeditStr;
            }
        }
    }

    // Lập lịch timer bất đồng bộ trên EventLoop của Fcitx5 để xử lý tuần tự mà không block giao diện
    void LotusState::scheduleUinputBackspace(uint32_t delayMs, std::function<void()> callback) {
        uinput_commit_timer_.reset();
        auto& el  = engine_->instance()->eventLoop();
        auto  now = ::fcitx::now(CLOCK_MONOTONIC);
        uinput_commit_timer_ =
            el.addTimeEvent(CLOCK_MONOTONIC, now + static_cast<uint64_t>(delayMs) * 1000ULL, 0, [this, cb = std::move(callback)](EventSourceTime*, uint64_t) -> bool {
                auto completed = std::move(uinput_commit_timer_);
                cb();
                return false;
            });
    }

    // Thực hiện xóa ký tự cũ qua uinput và commit ký tự mới kèm hàng rào an toàn 20ms
    void LotusState::performUinputBackspaceReplacement(const std::string& deletedPart, const std::string& addedPart) {
        LOTUS_INFO("UinputBackspace replacement: " + deletedPart + " -> " + addedPart);
        int backspaceCount = static_cast<int>(utf8::length(deletedPart));
        if (backspaceCount <= 0) {
            // Không có ký tự nào cần xóa: commit chuỗi mới và áp dụng barrier 20ms để tuần tự hóa phím tiếp theo
            if (!addedPart.empty()) {
                ic_->commitString(addedPart);
                lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                LOTUS_INFO("UinputBackspace commit: " + addedPart);
                is_deleting_.store(true, std::memory_order_release);
                scheduleUinputBackspace(5, [this]() {
                    is_deleting_.store(false);
                    replayBufferedKeys();
                });
            } else {
                replayBufferedKeys();
            }
            return;
        }

        // Bật cờ xóa để đưa các phím người dùng gõ siêu nhanh vào hàng đợi đệm (buffered_keys_)
        is_deleting_.store(true, std::memory_order_release);
        send_backspace_uinput(backspaceCount);
        LOTUS_INFO("UinputBackspace sent " + std::to_string(backspaceCount) + " backspaces");

        int afterWait = *engine_->config().uinputBackspaceAfterWaitMs;
        if (afterWait <= 0) {
            afterWait = 5;
        } else if (afterWait > 10) {
            afterWait = 10;
        }
        // Tính tổng thời gian uinput server gửi phím (mỗi phím 5ms) cộng thêm thời gian chờ ứng dụng cập nhật
        int waitTime = (backspaceCount - 1) * 5 + afterWait;

        scheduleUinputBackspace(waitTime, [this, addedPart]() {
            if (!addedPart.empty()) {
                ic_->commitString(addedPart);
                lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                LOTUS_INFO("UinputBackspace commit: " + addedPart);
            }
            // Hàng rào bảo vệ (Post-commit barrier 5ms): Giữ cờ bận is_deleting_ thêm 5ms sau khi commit
            // để bảo đảm ứng dụng đã hoàn tất nhận chữ trước khi bơm phím tiếp theo ra màn hình
            scheduleUinputBackspace(5, [this]() {
                is_deleting_.store(false);
                replayBufferedKeys();
            });
        });
    }

    // Xử lý sự kiện bàn phím cho mode UinputBackspace
    void LotusState::handleUinputBackspaceMode(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }

        if (uinput_client_fd_ < 0) {
            setup_uinput();
        }

        // Xử lý phím Backspace hoặc Enter người dùng tự bấm
        if (isBackspace(currentSym) || currentSym == FcitxKey_Return) {
            if (isBackspace(currentSym)) {
                // Nếu đang trong tiến trình xóa uinput tự động hoặc trong 50ms sau commit, forward phím và bỏ qua
                // không cho lọt vào Bamboo Engine để tránh xóa nhầm trạng thái ghép từ
                if (shouldRejectReset()) {
                    keyEvent.forward();
                    return;
                }
                hasHistory_ = true;
                EngineProcessKeyEvent(lotusEngine_.handle(), FcitxKey_BackSpace, 0);
                UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
                oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
            } else {
                if (shouldRejectReset()) {
                    keyEvent.forward();
                    return;
                }
                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
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

        // Chặn phím không cho lọt thô ra app, nạp vào Bamboo Engine để xử lý tiếng Việt
        keyEvent.filterAndAccept();

        bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;

        // Trường hợp 1: Bamboo kết thúc từ và trả về chuỗi commit đầy đủ (ví dụ khi gõ phím ngắt từ, dấu câu)
        auto commitF = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
        if (commitF && (*commitF.get() != 0)) {
            std::string commitStr = commitF.get();
            std::string deletedPart;
            std::string addedPart;
            compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();

            if (!deletedPart.empty()) {
                performUinputBackspaceReplacement(deletedPart, addedPart);
                return;
            }
            if (!addedPart.empty()) {
                ic_->commitString(addedPart);
                lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                LOTUS_INFO("UinputBackspace commit: " + addedPart);
                is_deleting_.store(true, std::memory_order_release);
                scheduleUinputBackspace(5, [this]() {
                    is_deleting_.store(false);
                    replayBufferedKeys();
                });
                return;
            }
            return;
        }

        // Trường hợp 2: Bamboo Engine không xử lý phím này (ví dụ phím số, ký tự đặc biệt không ghép từ).
        // Ta commit trực tiếp ký tự ra ứng dụng, reset bộ đệm từ cũ và áp dụng barrier 5ms để tuần tự hóa phím tiếp theo.
        if (!processed) {
            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();
            ic_->commitString(keyUtf8);
            lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
            LOTUS_INFO("UinputBackspace commit: " + keyUtf8);
            is_deleting_.store(true, std::memory_order_release);
            scheduleUinputBackspace(5, [this]() {
                is_deleting_.store(false);
                replayBufferedKeys();
            });
            return;
        }

        hasHistory_ = true;
        realtextLen.fetch_add(1, std::memory_order_acq_rel);

        UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
        std::string      preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

        // Trường hợp 3: So sánh trạng thái preedit cũ và mới để tính toán phần cần xóa và phần cần thêm
        std::string deletedPart;
        std::string addedPart;
        if (compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) != 0) {
            if (deletedPart.empty()) {
                // Chỉ thêm ký tự mới (ví dụ gõ 't' rồi gõ 'h' -> chỉ cần commit thêm 'h')
                if (!addedPart.empty()) {
                    oldPreBuffer_ = preeditStr;
                    ic_->commitString(addedPart);
                    lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                    LOTUS_INFO("UinputBackspace commit: " + addedPart);
                    is_deleting_.store(true, std::memory_order_release);
                    scheduleUinputBackspace(5, [this]() {
                        is_deleting_.store(false);
                        replayBufferedKeys();
                    });
                }
            } else {
                // Cần xóa ký tự cũ (ví dụ bỏ dấu: 't-h-o-w' -> cần xóa 'o' để thay bằng 'ơ')
                if (uinput_client_fd_ < 0) {
                    // Fallback: nếu mất kết nối tới uinput server daemon, commit phím thô để người dùng không bị nuốt phím
                    LOTUS_ERROR("Cannot connect to uinput server, commit rawkey");
                    std::string rawKey = keyEvent.key().toString();
                    if (!rawKey.empty()) {
                        ic_->commitString(rawKey);
                        lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                    }
                    return;
                }

                performUinputBackspaceReplacement(deletedPart, addedPart);
                oldPreBuffer_ = preeditStr;
            }
        }
    }

    void LotusState::handleSurroundingText(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }
        auto* ic = keyEvent.inputContext();
        if ((ic == nullptr) || !ic->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
            LOTUS_WARN("Surrounding text not supported");
            keyEvent.forward();
            return;
        }

        const auto& surrounding = ic->surroundingText();
        if (!surrounding.isValid()) {
            LOTUS_WARN("Surrounding text is invalid");
            keyEvent.forward();
            return;
        }

        if (isBackspace(keyEvent.rawKey().sym())) {
            ResetEngine(lotusEngine_.handle());
            keyEvent.forward();
            return;
        }

        const std::string& text   = surrounding.text();
        unsigned int       cursor = std::min(surrounding.anchor(), surrounding.cursor());

        size_t             textLen = utf8::lengthValidated(text);

        if (textLen == utf8::INVALID_LENGTH || cursor <= 0 || cursor > textLen) {
            processNormalKey(keyEvent, currentSym);
            return;
        }

        {
            auto startIter = utf8::nextNChar(text.begin(), cursor);
            auto endIter   = startIter;

            int  scanCount = 0;
            while (startIter != text.begin() && scanCount < MAX_SCAN_LENGTH) {
                auto prev = startIter;
                if (prev != text.begin()) {
                    --prev;
                    while (prev != text.begin() && ((*prev & 0xC0) == 0x80)) {
                        --prev;
                    }
                }

                uint32_t ucs4 = utf8::getChar(prev, text.end());

                if (isWordBreak(ucs4))
                    break;

                startIter = prev;
                ++scanCount;
            }

            std::string oldWord(startIter, endIter);

            if (oldWord.empty()) {
                processNormalKey(keyEvent, currentSym);
                return;
            }

            EngineRebuildFromText(lotusEngine_.handle(), oldWord.c_str());

            bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;

            if (!processed) {
                keyEvent.forward();
                ResetEngine(lotusEngine_.handle());
                return;
            }

            auto        commitPtr  = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
            auto        preeditPtr = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));

            std::string newWord;
            if (commitPtr && (*commitPtr.get() != 0))
                newWord += commitPtr.get();
            if (preeditPtr && (*preeditPtr.get() != 0))
                newWord += preeditPtr.get();

            std::string deletedPart;
            std::string addedPart;
            compareAndSplitStrings(oldWord, newWord, deletedPart, addedPart);
            if ((deletedPart.empty() || deletedPart == oldWord) && addedPart == keyEvent.key().toString()) {
                ResetEngine(lotusEngine_.handle());
                keyEvent.forward();
                return;
            }

            if (!deletedPart.empty() || !addedPart.empty()) {
                size_t charsToDelete = utf8::length(deletedPart);

                if (charsToDelete > 0) {
                    ic->deleteSurroundingText(-static_cast<int>(charsToDelete), static_cast<int>(charsToDelete));
                    std::this_thread::sleep_for(std::chrono::milliseconds(4 * charsToDelete));
                }

                if (!addedPart.empty()) {
                    ic->commitString(addedPart);
                    LOTUS_INFO("Commit: " + addedPart);
                }

                ResetEngine(lotusEngine_.handle());
                keyEvent.filterAndAccept();
                return;
            }

            ResetEngine(lotusEngine_.handle());
            keyEvent.filterAndAccept();
            return;
        }
    }

    void LotusState::processNormalKey(KeyEvent& keyEvent, KeySym currentSym) {
        auto* ic = keyEvent.inputContext();
        ResetEngine(lotusEngine_.handle());
        bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;
        if (processed) {
            auto        commitPtr  = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
            auto        preeditPtr = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));
            std::string out;
            if (commitPtr && (*commitPtr.get() != 0))
                out += commitPtr.get();
            if (preeditPtr && (*preeditPtr.get() != 0))
                out += preeditPtr.get();

            if (!out.empty()) {
                LOTUS_INFO("Commit: " + out);
                ic->commitString(out);
            }

            ResetEngine(lotusEngine_.handle());
            keyEvent.filterAndAccept();
        } else {
            keyEvent.forward();
        }
    }

    void LotusState::handleDoubleSpaceReplacement() {
        switch (realMode) {
            case LotusMode::SurroundingText: {
                ic_->deleteSurroundingText(-1, 1);
                ic_->commitString(". ");
                LOTUS_INFO("Commit: . ");

                break;
            }
            default: { // Uinput, Smooth, Preedit, etc.
                performReplacement(" ", ". ");
                LOTUS_INFO("Commit: . ");
                break;
            }
        }
        if (*engine_->config().autoCapitalizeAfterPunctuation) {
            isPrevPunctuation_ = true;
            shouldCapitalize_  = true;
        }
    }

    void LotusState::handleDoubleHyphenReplacement() {
        // Em-dash (U+2014)
        std::string emDash = "—";
        switch (realMode) {
            case LotusMode::SurroundingText: {
                ic_->deleteSurroundingText(-1, 1);
                ic_->commitString(emDash);
                LOTUS_INFO("Commit: — (em-dash)");
                break;
            }
            default: { // Uinput, Smooth, Preedit, etc.
                performReplacement("-", emDash);
                LOTUS_INFO("Commit: — (em-dash)");
                break;
            }
        }
    }

    void LotusState::handleOffModeMacro(KeyEvent& keyEvent, KeySym currentSym) {
        if (checkForwardSpecialKey(keyEvent, currentSym)) {
            keyEvent.forward();
            return;
        }

        if (uinput_client_fd_ < 0) {
            connect_uinput_server();
        }

        if (isBackspace(currentSym)) {
            EngineProcessKeyEvent(lotusEngine_.handle(), FcitxKey_BackSpace, 0);
            auto preeditC = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));
            oldPreBuffer_ = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";
            keyEvent.forward();
            return;
        }

        if (currentSym == FcitxKey_Return) {
            if (!oldPreBuffer_.empty()) {
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.forward();
            return;
        }

        std::string keyUtf8 = Key::keySymToUTF8(currentSym);

        bool        processed = EngineProcessKeyEvent(lotusEngine_.handle(), currentSym, keyEvent.rawKey().states()) != 0U;

        auto        commitPtr = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
        if (processed && commitPtr && (*commitPtr.get() != 0)) {
            std::string commitStr = commitPtr.get();

            // Determine if this is a macro expansion or just confirmed typed text
            bool isMacroExpansion = false;
            if (keyUtf8.empty()) {
                isMacroExpansion = (commitStr != oldPreBuffer_);
            } else {
                isMacroExpansion = (commitStr != oldPreBuffer_ + keyUtf8);
            }

            if (isMacroExpansion) {
                LOTUS_INFO("Macro expansion: '" + oldPreBuffer_ + "' -> '" + commitStr + "'");
                // Try uinput replacement first, fallback to deleteSurroundingText, then plain commit
                if (uinput_client_fd_ >= 0 && !oldPreBuffer_.empty()) {
                    performReplacement(oldPreBuffer_, commitStr);
                } else if (ic_->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
                    const auto& surrounding = ic_->surroundingText();
                    if (surrounding.isValid()) {
                        size_t oldLen = utf8::length(oldPreBuffer_);
                        if (oldLen > 0) {
                            ic_->deleteSurroundingText(-static_cast<int>(oldLen), static_cast<int>(oldLen));
                        }
                        ic_->commitString(commitStr);
                    } else {
                        ic_->commitString(commitStr);
                    }
                } else {
                    ic_->commitString(commitStr);
                }
                keyEvent.filterAndAccept();
            } else {
                // No macro: typed text confirmed by engine, just forward trigger key
                keyEvent.forward();
            }

            oldPreBuffer_.clear();
            hasHistory_ = false;
            return;
        }

        // 8. No commit or engine rejected the key
        if (processed || (commitPtr && (*commitPtr.get() != 0))) {
            // Engine processed the key (building shadow state)
            // OR engine rejected the key but committed old text (non-processable key)
            auto preeditPtr = UniqueCPtr<char>(EnginePullPreedit(lotusEngine_.handle()));
            oldPreBuffer_   = (preeditPtr && (*preeditPtr.get() != 0)) ? preeditPtr.get() : "";
            if (!processed) {
                // Engine committed old text but didn't process the new key → forward the key
                oldPreBuffer_.clear();
                hasHistory_ = false;
            }
            keyEvent.forward();
        } else {
            // Engine didn't handle this key
            if (!oldPreBuffer_.empty()) {
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
            }
            keyEvent.forward();
        }
    }

    void LotusState::keyEvent(KeyEvent& keyEvent) {
        if (!lotusEngine_)
            return;
        if (realMode == LotusMode::Preedit) {
            if (keyEvent.rawKey().check(FcitxKey_Shift_L) || keyEvent.rawKey().check(FcitxKey_Shift_R))
                return;
        } else {
            if (keyEvent.rawKey().isModifier()) {
                handleModifierTap(keyEvent);
                return;
            }
            cancelModifierTap();
        }
        if (keyEvent.isRelease())
            return;
        if (uinput_client_fd_ < 0) {
            LOTUS_WARN("Cannot connect to uinput server, reconnecting....");
            connect_uinput_server();
        }
        // Sửa lỗi nghiêm trọng (0 >= 0 bug):
        // Trong UinputBackspace mode, expected_backspaces_ và current_backspace_count_ không được sử dụng và luôn bằng 0.
        // Trước đây điều kiện (0 >= 0 && is_deleting_) luôn đúng, khiến is_deleting_ bị tắt ngay lập tức trên mỗi phím bấm,
        // làm vô hiệu hóa hàng đợi đệm và khiến các phím gõ nhanh đè hỏng trạng thái uinput.
        // Cần loại trừ UinputBackspace và kiểm tra expected_backspaces_ > 0 để chỉ áp dụng cho Smooth/SuperSmooth mode.
        if (realMode != LotusMode::UinputBackspace && expected_backspaces_ > 0 && current_backspace_count_ >= expected_backspaces_ && is_deleting_.load()) {
            is_deleting_.store(false);
            current_backspace_count_ = 0;
            expected_backspaces_     = 0;
            if (!buffered_keys_.empty()) {
                replayBufferedKeys();
            }
        }
        if (needEngineReset.load() && realMode != LotusMode::Off) {
            LOTUS_INFO("Need engine reset");
            oldPreBuffer_.clear();
            hasHistory_ = false;
            ResetEngine(lotusEngine_.handle());
            is_deleting_.store(false);
            current_backspace_count_ = 0;
            isPrevSpace_             = false;
            shouldCapitalize_        = false;
            isPrevPunctuation_       = false;
            needEngineReset.store(false);
        }

        if (g_mouse_clicked.load(std::memory_order_acquire) && !is_deleting_.load(std::memory_order_acquire)) {
            g_mouse_clicked.store(false, std::memory_order_release);
            clearAllBuffers();
        }
        KeySym currentSym = keyEvent.rawKey().sym();
        if (*engine_->config().autoCapitalizeAfterPunctuation && realMode != LotusMode::Off) {
            // Ignore auto-capitalize side-effects if we're processing automated replacement backspaces
            bool isAutomatedBackspace = is_deleting_.load(std::memory_order_acquire) && isBackspace(currentSym);

            if (!isAutomatedBackspace) {
                if (shouldCapitalize_) {
                    if (currentSym >= FcitxKey_a && currentSym <= FcitxKey_z) {
                        auto upperSym = static_cast<KeySym>(currentSym - (FcitxKey_a - FcitxKey_A));
                        currentSym    = upperSym;
                        keyEvent.setKey(Key(upperSym, keyEvent.rawKey().states()));
                        shouldCapitalize_ = false;
                    } else if (currentSym != FcitxKey_space) {
                        shouldCapitalize_ = false;
                    }
                }

                switch (currentSym) {
                    case FcitxKey_period:
                    case FcitxKey_exclam:
                    case FcitxKey_question: isPrevPunctuation_ = true; break;
                    case FcitxKey_Return:
                    case FcitxKey_KP_Enter:
                        shouldCapitalize_  = true;
                        isPrevPunctuation_ = false;
                        break;
                    case FcitxKey_space:
                        if (isPrevPunctuation_) {
                            shouldCapitalize_  = true;
                            isPrevPunctuation_ = false;
                        }
                        break;
                    default:
                        if (currentSym != FcitxKey_space) {
                            isPrevPunctuation_ = false;
                        }
                        break;
                }
            }
        }

        if (is_deleting_.load(std::memory_order_acquire)) {
            if (isBackspace(currentSym)) {
                // Phím Backspace do uinput server bắn vào hệ thống được compositor gửi tới Fcitx5:
                // Đối với UinputBackspace mode, ta forward thẳng ra ứng dụng đích để thực hiện xóa ký tự trên màn hình,
                // tuyệt đối không nạp vào Bamboo Engine hay xử lý đếm số lượng như Smooth mode để tránh lỗi lặp phím.
                if (realMode == LotusMode::UinputBackspace) {
                    keyEvent.forward();
                    return;
                }
                if (realtextLen.load(std::memory_order_acquire) > 0)
                    realtextLen.fetch_sub(1, std::memory_order_acq_rel);
                if (handleUInputKeyPress(keyEvent, currentSym, (realMode == LotusMode::Smooth || realMode == LotusMode::SuperSmooth) ? 2 : 8)) {
                    return;
                }
            } else {
                std::string keyUtf8Check = Key::keySymToUTF8(currentSym);
                if (!keyUtf8Check.empty() && buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                    LOTUS_WARN("Typing so fast, add key to queue");
                    buffered_keys_.push_back({.sym = currentSym, .state = keyEvent.rawKey().states()});
                }
                keyEvent.filterAndAccept();
            }
            return;
        }

        if (*engine_->config().doubleSpaceToPeriod && realMode != LotusMode::Off) {
            bool isSpaceKey = (currentSym == FcitxKey_space || currentSym == FcitxKey_KP_Space);
            if (isSpaceKey && !keyEvent.key().hasModifier()) {
                if (isPrevSpace_) {
                    keyEvent.filterAndAccept();
                    handleDoubleSpaceReplacement();
                    isPrevSpace_ = false;
                    return;
                }
                isPrevSpace_ = true;
            } else {
                isPrevSpace_ = false;
            }
        }

        if (*engine_->config().doubleHyphenToEmDash && realMode != LotusMode::Off) {
            bool isHyphenKey = (currentSym == FcitxKey_minus || currentSym == FcitxKey_KP_Subtract);
            if (isHyphenKey && !keyEvent.key().hasModifier()) {
                if (isPrevHyphen_) {
                    keyEvent.filterAndAccept();
                    handleDoubleHyphenReplacement();
                    isPrevHyphen_ = false;
                    return;
                }
                isPrevHyphen_ = true;
            } else {
                isPrevHyphen_ = false;
            }
        }

        switch (realMode) {
            case LotusMode::Uinput:
            case LotusMode::Smooth:
            case LotusMode::Minecraft:
            case LotusMode::SuperSmooth: {
                handleUinputMode(keyEvent, currentSym);
                break;
            }
            case LotusMode::UinputBackspace: {
                handleUinputBackspaceMode(keyEvent, currentSym);
                break;
            }
            case LotusMode::SurroundingText: {
                handleSurroundingText(keyEvent, currentSym);
                break;
            }
            case LotusMode::Preedit: {
                handlePreeditMode(keyEvent, currentSym);
                break;
            }
            case LotusMode::Emoji: {
                handleEmojiMode(keyEvent);
                break;
            }
            default: {
                if (*engine_->config().enableMacroInOffMode && *engine_->config().enableMacro) {
                    handleOffModeMacro(keyEvent, currentSym);
                }
                break;
            }
        }
        reEnableMacroAfterWordEnd();
    }

    void LotusState::reset(bool isFocusOut) {
        const auto& surrounding = ic_->surroundingText();
        const auto& text        = surrounding.text();
        size_t      textLen     = utf8::length(text);
        realtextLen.store(textLen, std::memory_order_release);
        // Chặn reset ngoài ý muốn:
        // Khi Fcitx5 hoặc ứng dụng phát tín hiệu reset trong lúc đang gửi phím xóa qua uinput
        // hoặc trong cửa sổ 50ms sau commit, ta từ chối reset để không xóa mất ngữ cảnh ghép từ tiếng Việt
        // trong Bamboo Engine (khắc phục lỗi mất dấu / đứt từ khi gõ).
        if (shouldRejectReset()) {
            return;
        }
        resetMacroSkip();

        if (lotusEngine_) {
            isPrevSpace_       = false;
            isPrevHyphen_      = false;
            shouldCapitalize_  = false;
            isPrevPunctuation_ = false;
            if (realMode == LotusMode::Preedit && isFocusOut) {
                EngineCommitPreedit(lotusEngine_.handle());
                UniqueCPtr<char> commit(EnginePullCommit(lotusEngine_.handle()));
                if (commit && (*commit.get() != 0)) {
                    ic_->commitString(commit.get());
                    LOTUS_INFO("Commit: " + std::string(commit.get()));
                }
            }
            ResetEngine(lotusEngine_.handle());
            oldPreBuffer_.clear();
            hasHistory_ = false;
        }
        if (getFrontendName(ic_) != "dbus")
            clearAllBuffers();

        switch (realMode) {
            case LotusMode::Preedit: {
                ic_->inputPanel().reset();
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                ic_->updatePreedit();
                break;
            }
            case LotusMode::SurroundingText:
            case LotusMode::Uinput:
            case LotusMode::Smooth:
            case LotusMode::Minecraft:
            case LotusMode::SuperSmooth:
            case LotusMode::UinputBackspace: {
                ic_->inputPanel().reset();
                break;
            }
            case LotusMode::Emoji: {
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

    void LotusState::commitBuffer() {
        switch (realMode) {
            case LotusMode::Preedit: {
                ic_->inputPanel().reset();
                if (lotusEngine_) {
                    EngineCommitPreedit(lotusEngine_.handle());
                    UniqueCPtr<char> commit(EnginePullCommit(lotusEngine_.handle()));
                    if (commit && (*commit.get() != 0))
                        ic_->commitString(commit.get());
                    ResetEngine(lotusEngine_.handle());
                }
                ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
                ic_->updatePreedit();
                break;
            }
            case LotusMode::Uinput:
            case LotusMode::Smooth:
            case LotusMode::SurroundingText:
            case LotusMode::Minecraft:
            case LotusMode::SuperSmooth:
            case LotusMode::UinputBackspace: {
                if (lotusEngine_) {
                    ResetEngine(lotusEngine_.handle());
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    void LotusState::clearAllBuffers() {
        LOTUS_DEBUG("Clear all buffers");
        // Không xóa buffer nếu đang bận gửi uinput backspace hoặc trong cửa sổ 50ms sau commit
        // nhằm bảo toàn các biến đệm preedit và lịch sử từ đang gõ dở
        if (shouldRejectReset()) {
            return;
        }
        resetMacroSkip();
        oldPreBuffer_.clear();
        hasHistory_ = false;
        if (!is_deleting_.load(std::memory_order_acquire)) {
            expected_backspaces_     = 0;
            current_backspace_count_ = 0;
            pending_commit_string_.clear();
        }
        emojiBuffer_.clear();
        emojiCandidates_.clear();
        buffered_keys_.clear();
        uinput_commit_timer_.reset();
        shouldCapitalize_  = false;
        isPrevSpace_       = false;
        isPrevHyphen_      = false;
        isPrevPunctuation_ = false;
        if (lotusEngine_)
            ResetEngine(lotusEngine_.handle());
    }

    bool LotusState::isEmptyHistory() const {
        return !hasHistory_;
    }

    void LotusState::replayBufferedKeys() {
        LOTUS_INFO("Starting replay buffered keys");
        if (buffered_keys_.empty()) {
            return;
        }
        if (is_deleting_.load(std::memory_order_acquire)) {
            return;
        }

        // Cơ chế Async Serialization cho UinputBackspace mode:
        // Khác với các mode khác dùng vòng lặp for đồng bộ xả hết phím trong 1 lần (dễ gây tranh chấp
        // khi phím mới bắn ra trong lúc uinput backspace chưa kịp tới ứng dụng), UinputBackspace
        // chỉ bốc và xử lý đúng 1 phím đầu hàng đợi (FIFO). Sau khi commit hoặc replace phím đó,
        // thiết lập barrier 5ms rồi mới tiếp tục gọi replayBufferedKeys() để xử lý phím kế tiếp.
        if (realMode == LotusMode::UinputBackspace) {
            auto keyInfo = buffered_keys_.front();
            buffered_keys_.erase(buffered_keys_.begin());

            auto        sym     = static_cast<KeySym>(keyInfo.sym);
            uint32_t    state   = keyInfo.state;
            std::string keyUtf8 = Key::keySymToUTF8(sym);
            if (keyUtf8.empty()) {
                replayBufferedKeys();
                return;
            }

            bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), sym, state) != 0U;

            auto commitF = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
            if (commitF && (*commitF.get() != 0)) {
                std::string commitStr = commitF.get();
                std::string deletedPart;
                std::string addedPart;
                compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();

                if (!deletedPart.empty()) {
                    performUinputBackspaceReplacement(deletedPart, addedPart);
                    return;
                }
                if (!addedPart.empty()) {
                    ic_->commitString(addedPart);
                    LOTUS_INFO("Replay commit: " + addedPart);
                    lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                    is_deleting_.store(true, std::memory_order_release);
                    scheduleUinputBackspace(5, [this]() {
                        is_deleting_.store(false);
                        replayBufferedKeys();
                    });
                    return;
                }
                replayBufferedKeys();
                return;
            }

            if (!processed) {
                ic_->commitString(keyUtf8);
                LOTUS_INFO("Replay commit raw: " + keyUtf8);
                lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                is_deleting_.store(true, std::memory_order_release);
                scheduleUinputBackspace(5, [this]() {
                    is_deleting_.store(false);
                    replayBufferedKeys();
                });
                return;
            }

            hasHistory_ = true;
            realtextLen.fetch_add(1, std::memory_order_acq_rel);

            UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
            std::string      preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

            std::string      deletedPart;
            std::string      addedPart;
            if (compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) != 0) {
                if (deletedPart.empty()) {
                    if (!addedPart.empty()) {
                        ic_->commitString(addedPart);
                        LOTUS_INFO("Replay commit added: " + addedPart);
                        lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                        oldPreBuffer_ = preeditStr;
                        is_deleting_.store(true, std::memory_order_release);
                        scheduleUinputBackspace(5, [this]() {
                            is_deleting_.store(false);
                            replayBufferedKeys();
                        });
                        return;
                    }
                    replayBufferedKeys();
                    return;
                } else {
                    if (uinput_client_fd_ < 0) {
                        ic_->commitString(keyUtf8);
                        lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                        replayBufferedKeys();
                        return;
                    }
                    performUinputBackspaceReplacement(deletedPart, addedPart);
                    oldPreBuffer_ = preeditStr;
                    return;
                }
            }
            replayBufferedKeys();
            return;
        }

        auto keys = std::move(buffered_keys_);
        buffered_keys_.clear();
        for (size_t i = 0; i < keys.size(); ++i) {
            auto        sym     = static_cast<KeySym>(keys[i].sym);
            uint32_t    state   = keys[i].state;
            std::string keyUtf8 = Key::keySymToUTF8(sym);
            if (keyUtf8.empty()) {
                continue;
            }

            bool processed = EngineProcessKeyEvent(lotusEngine_.handle(), sym, state) != 0U;

            auto commitF = UniqueCPtr<char>(EnginePullCommit(lotusEngine_.handle()));
            if (commitF && (*commitF.get() != 0)) {
                std::string commitStr = commitF.get();
                std::string deletedPart;
                std::string addedPart;
                compareAndSplitStrings(oldPreBuffer_, commitStr, deletedPart, addedPart);

                if (!deletedPart.empty()) {
                    // Re-buffer remaining keys for next replay cycle.
                    for (size_t j = i + 1; j < keys.size(); ++j) {
                        if (buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                            buffered_keys_.push_back(keys[j]);
                        }
                    }
                    if (realMode == LotusMode::UinputBackspace) {
                        performUinputBackspaceReplacement(deletedPart, addedPart);
                    } else {
                        performReplacement(deletedPart, addedPart);
                    }
                    hasHistory_ = false;
                    ResetEngine(lotusEngine_.handle());
                    oldPreBuffer_.clear();
                    return;
                }
                if (!addedPart.empty()) {
                    ic_->commitString(addedPart);
                    lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                }

                hasHistory_ = false;
                ResetEngine(lotusEngine_.handle());
                oldPreBuffer_.clear();
                continue;
            }

            if (!processed) {
                ic_->commitString(keyUtf8);
                lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                continue;
            }

            hasHistory_ = true;
            realtextLen.fetch_add(1, std::memory_order_acq_rel);

            UniqueCPtr<char> preeditC(EnginePullPreedit(lotusEngine_.handle()));
            std::string      preeditStr = (preeditC && (*preeditC.get() != 0)) ? preeditC.get() : "";

            std::string      deletedPart;
            std::string      addedPart;
            if (compareAndSplitStrings(oldPreBuffer_, preeditStr, deletedPart, addedPart) != 0) {
                if (deletedPart.empty()) {
                    if (!addedPart.empty()) {
                        ic_->commitString(addedPart);
                        lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                        oldPreBuffer_ = preeditStr;
                    }
                } else {
                    if (uinput_client_fd_ < 0) {
                        ic_->commitString(keyUtf8);
                        lastCommitTimeUsec_.store(fcitx::now(CLOCK_MONOTONIC), std::memory_order_release);
                        continue;
                    }

                    if (is_deleting_.load()) {
                        is_deleting_.store(false, std::memory_order_release);
                    }

                    // Re-buffer remaining keys for next replay cycle.
                    for (size_t j = i + 1; j < keys.size(); ++j) {
                        if (buffered_keys_.size() < MAX_BUFFERED_KEYS) {
                            buffered_keys_.push_back(keys[j]);
                        }
                    }
                    if (realMode == LotusMode::UinputBackspace) {
                        performUinputBackspaceReplacement(deletedPart, addedPart);
                    } else {
                        performReplacement(deletedPart, addedPart);
                    }
                    oldPreBuffer_ = preeditStr;
                    return;
                }
            }
        }
        LOTUS_INFO("Replay buffered keys done");
    }

    bool LotusState::isMacroSkipModifier(KeySym sym) const {
        const auto trigger = engine_->config().macroSkipTriggerModifier.value();
        switch (trigger) {
            case MacroSkipTriggerModifier::Shift: return sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R;
            case MacroSkipTriggerModifier::Ctrl: return sym == FcitxKey_Control_L || sym == FcitxKey_Control_R;
            case MacroSkipTriggerModifier::Alt: return sym == FcitxKey_Alt_L || sym == FcitxKey_Alt_R;
            case MacroSkipTriggerModifier::Disabled:
            default: return false;
        }
    }

    void LotusState::handleModifierTap(const KeyEvent& keyEvent) {
        const auto trigger = engine_->config().macroSkipTriggerModifier.value();
        if (trigger == MacroSkipTriggerModifier::Disabled || !*engine_->config().enableMacro) {
            return;
        }
        if (!isMacroSkipModifier(keyEvent.rawKey().sym())) {
            tracking_modifier_tap_ = false;
            return;
        }
        if (keyEvent.isRelease()) {
            if (tracking_modifier_tap_) {
                tracking_modifier_tap_ = false;
                macro_skip_            = true;
                EngineSetMacroEnabled(lotusEngine_.handle(), 0);
                LOTUS_INFO("Macro skip enabled for next word");
            }
        } else {
            tracking_modifier_tap_ = true;
        }
    }

    void LotusState::cancelModifierTap() {
        tracking_modifier_tap_ = false;
    }

    void LotusState::reEnableMacroAfterWordEnd() {
        if (!macro_skip_) {
            return;
        }
        UniqueCPtr<char> preedit(EnginePullPreedit(lotusEngine_.handle()));
        if (preedit && *preedit.get() != 0) {
            return;
        }
        macro_skip_ = false;
        EngineSetMacroEnabled(lotusEngine_.handle(), *engine_->config().enableMacro ? 1 : 0);
    }

    void LotusState::resetMacroSkip() {
        tracking_modifier_tap_ = false;
        macro_skip_            = false;
        if (lotusEngine_) {
            EngineSetMacroEnabled(lotusEngine_.handle(), *engine_->config().enableMacro ? 1 : 0);
        }
    }
} // namespace fcitx
