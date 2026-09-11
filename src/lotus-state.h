/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-state.h
 * @brief Input context state management for fcitx5-lotus.
 */

#ifndef _FCITX5_LOTUS_STATE_H_
#define _FCITX5_LOTUS_STATE_H_

#include "lotus.h"
#include "emoji-entry.h"
#include "lotus-utils.h"

#include <cstddef>
#include <functional>
#include <fcitx-utils/event.h>
#include <fcitx-utils/misc.h>
#include <fcitx/inputcontext.h>

struct EmojiEntry;

namespace fcitx {
    class LotusEngine;
    class CommonCandidateList;
    class SurroundingText;

    /**
     * @brief Per-input-context state for Lotus input method.
     *
     * Manages the input state, buffers, and mode-specific handling for each input context.
     */
    class LotusState final : public InputContextProperty {
      public:
        /**
         * @brief Constructs a new state instance.
         * @param engine Pointer to the Lotus engine.
         * @param ic Pointer to the input context.
         */
        LotusState(LotusEngine* engine, InputContext* ic);

        /**
         * @brief Initializes the bamboo engine for this state.
         */
        void setEngine();

        /**
         * @brief Applies current options to the engine.
         */
        void setOption();

        /**
         * @brief Main key event handler.
         * @param keyEvent The key event to process.
         */
        void keyEvent(KeyEvent& keyEvent);

        /**
         * @brief Resets the input state.
         * @param isFocusOut If true, indicates the reset is due to a focus-out event, which may trigger committing the preedit text.
         */
        void reset(bool isFocusOut = false);

        /**
         * @brief Commits the current buffer.
         */
        void commitBuffer();

        /**
         * @brief Clears all internal buffers.
         */
        void clearAllBuffers();

        /**
         * @brief Checks if history buffer is empty.
         * @return True if no history.
         */
        bool isEmptyHistory() const;
        friend class EmojiCandidateWord;
        friend class LotusEngine;

      private:
        static constexpr size_t MAX_BUFFERED_KEYS = 50;

        LotusEngine*            engine_;
        InputContext*           ic_;
        CGoObject               lotusEngine_;
        std::string             oldPreBuffer_;
        bool                    hasHistory_              = false;
        int                     expected_backspaces_     = 0;
        int                     current_backspace_count_ = 0;
        std::string             pending_commit_string_;
        std::string             emojiBuffer_;
        std::vector<EmojiEntry> emojiCandidates_;
        bool                    waitAck_ = false;
        std::unique_ptr<EventSourceTime> uinput_commit_timer_;
        std::vector<KeyEntry>   buffered_keys_; ///< Keystrokes buffered during replacement
        bool                    isPrevSpace_           = false;
        bool                    isPrevHyphen_          = false;
        bool                    shouldCapitalize_      = false;
        bool                    isPrevPunctuation_     = false;
        int64_t                 lastDeactivateTime_    = 0;
        bool                    wa_chromium_flag       = false;
        bool                    tracking_modifier_tap_ = false; ///< Selected modifier held, waiting for consecutive keyup
        bool                    macro_skip_            = false; ///< Macro disabled for the current word

        /**
         * @brief Connects to the uinput server.
         * @return True if connection successful.
         */
        static bool connect_uinput_server();

        /**
         * @brief Sets up uinput device.
         * @return File descriptor or -1 on error.
         */
        static int setup_uinput();

        /**
         * @brief Sends backspace key events via uinput.
         * @param count Number of backspaces to send.
         */
        void send_backspace_uinput(int count) const;

        /**
         * @brief Checks if autofill is certain for surrounding text.
         * @param s The surrounding text.
         * @return True if autofill should proceed.
         */
        bool isAutofillCertain(const SurroundingText& s);

        /**
         * @brief Handles key events in preedit mode.
         * @param keyEvent The key event to process.
         * @param currentSym Current key symbol.
         */
        void handlePreeditMode(KeyEvent& keyEvent, KeySym currentSym);

        /**
         * @brief Updates emoji page status in candidate list.
         * @param commonList The candidate list to update.
         */
        void updateEmojiPageStatus(CommonCandidateList* commonList);

        /**
         * @brief Handles key events in emoji mode.
         * @param keyEvent The key event to process.
         */
        void handleEmojiMode(KeyEvent& keyEvent);

        /**
         * @brief Updates preedit display for emoji mode.
         */
        void updateEmojiPreedit();

        /**
         * @brief Handles key press in uinput mode.
         * @param event The key event.
         * @param currentSym Current key symbol.
         * @param sleepTime Delay in microseconds.
         * @return True if event was handled.
         */
        bool handleUInputKeyPress(KeyEvent& event, KeySym currentSym, int sleepTime);

        /**
         * @brief Performs text replacement via uinput.
         * @param deletedPart Text to delete.
         * @param addedPart Text to insert.
         */
        void performReplacement(const std::string& deletedPart, const std::string& addedPart);

        /**
         * @brief Handles the double space to period replacement.
         */
        void handleDoubleSpaceReplacement();

        /**
         * @brief Handles the double hyphen to em-dash replacement.
         */
        void handleDoubleHyphenReplacement();

        /**
         * @brief Checks and forwards special keys.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol (may be modified).
         * @return True if key was forwarded.
         */
        bool checkForwardSpecialKey(KeyEvent& keyEvent, KeySym& currentSym);

        /**
         * @brief Handles uinput mode processing.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol.
         * @param sleepTime Delay in microseconds.
         */
        void handleUinputMode(KeyEvent& keyEvent, KeySym currentSym);

        /**
         * @brief Handles Uinput (Backspace) mode processing.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol.
         */
        void handleUinputBackspaceMode(KeyEvent& keyEvent, KeySym currentSym);

        /**
         * @brief Performs replacement in UinputBackspace mode using server + calculated delay.
         * @param deletedPart Text to delete.
         * @param addedPart Text to insert.
         */
        void performUinputBackspaceReplacement(const std::string& deletedPart, const std::string& addedPart);
        void scheduleUinputBackspace(uint32_t delayMs, std::function<void()> callback);

        /**
         * @brief Handles surrounding text mode.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol.
         */
        void handleSurroundingText(KeyEvent& keyEvent, KeySym currentSym);

        /**
         * @brief Handles Off mode with macro shadow processing.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol.
         */
        void handleOffModeMacro(KeyEvent& keyEvent, KeySym currentSym);

        /**
         * @brief Handles processing normal key events.
         * @param keyEvent The key event.
         * @param currentSym Current key symbol.
         */
        void processNormalKey(KeyEvent& keyEvent, KeySym currentSym);

        /**
         * @brief Replays keystrokes buffered during replacement.
         *
         * When is_deleting_ is true, non-special keystrokes are buffered
         * instead of being discarded. This method replays them after the
         * replacement completes.
         */
        void replayBufferedKeys();

        /**
         * @brief Checks if the key symbol matches the configured macro-skip modifier.
         * @param sym Key symbol to check.
         * @return True if the key is the configured trigger modifier (left/right same).
         */
        bool isMacroSkipModifier(KeySym sym) const;

        /**
         * @brief Tracks a modifier tap (keydown then consecutive keyup) to skip macro.
         * @param keyEvent The modifier key event.
         */
        void handleModifierTap(const KeyEvent& keyEvent);

        /**
         * @brief Cancels an in-progress modifier tap when another key arrives.
         */
        void cancelModifierTap();

        /**
         * @brief Re-enables macro when the current word ends (engine preedit empty).
         */
        void reEnableMacroAfterWordEnd();

        /**
         * @brief Clears the macro-skip state and re-syncs the engine.
         */
        void resetMacroSkip();
    };

} // namespace fcitx

#endif // _FCITX5_LOTUS_STATE_H_
