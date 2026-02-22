/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-engine.h
 * @brief Main engine implementation for fcitx5-lotus Vietnamese input method.
 */

#ifndef _FCITX5_LOTUS_ENGINE_H_
#define _FCITX5_LOTUS_ENGINE_H_

#include "lotus-config.h"
#include "emoji.h"
#include "lotus.h"
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/i18n.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fcitx {

    class CGoObject;
    class LotusState;

    /**
     * @brief Main engine class for Lotus input method.
     *
     * Handles input processing, configuration management, and UI actions.
     * Implements fcitx InputMethodEngine interface.
     */
    class LotusEngine final : public InputMethodEngine {
      public:
        /**
         * @brief Gets the fcitx instance.
         * @return Pointer to the fcitx instance.
         */
        Instance* instance() const {
            return instance_;
        }

        /**
         * @brief Constructs the Lotus engine.
         * @param instance Pointer to fcitx instance.
         */
        LotusEngine(Instance* instance);

        /**
         * @brief Destroys the engine and releases resources.
         */
        ~LotusEngine();

        /**
         * @brief Activates the input method for an input context.
         * @param entry Input method entry.
         * @param event Activation event.
         */
        void activate(const InputMethodEntry& entry, InputContextEvent& event) override;

        /**
         * @brief Deactivates the input method.
         * @param entry Input method entry.
         * @param event Deactivation event.
         */
        void deactivate(const InputMethodEntry& entry, InputContextEvent& event) override;

        /**
         * @brief Processes key events.
         * @param entry Input method entry.
         * @param keyEvent The key event to process.
         */
        void keyEvent(const InputMethodEntry& entry, KeyEvent& keyEvent) override;

        /**
         * @brief Resets the input method state.
         * @param entry Input method entry.
         * @param event Reset event.
         */
        void reset(const InputMethodEntry& entry, InputContextEvent& event) override;

        /**
         * @brief Reloads configuration from disk.
         */
        void reloadConfig() override;

        /**
         * @brief Gets the current configuration.
         * @return Pointer to configuration object.
         */
        const Configuration* getConfig() const override {
            return &config_;
        }

        /**
         * @brief Gets a sub-configuration by path.
         * @param path Sub-config path.
         * @return Pointer to sub-configuration.
         */
        const Configuration* getSubConfig(const std::string& path) const override;

        /**
         * @brief Applies configuration changes.
         * @param config New configuration.
         */
        void setConfig(const RawConfig& config) override;

        /**
         * @brief Applies sub-configuration changes.
         * @param path Sub-config path.
         * @param config New sub-configuration.
         */
        void setSubConfig(const std::string& path, const RawConfig& config) override;

        /**
         * @brief Gets the current sub-mode label.
         * @param entry Input method entry.
         * @param inputContext Current input context.
         * @return Mode label string.
         */
        std::string subMode(const InputMethodEntry& entry, InputContext& inputContext) override;

        /**
         * @brief Gets the override icon name.
         * @param entry Input method entry.
         * @return Icon name string.
         */
        std::string overrideIcon(const InputMethodEntry& entry) override;

        /**
         * @brief Gets the current configuration.
         * @return Reference to lotus configuration.
         */
        const auto& config() const {
            return config_;
        }

        /**
         * @brief Gets the custom keymap configuration.
         * @return Reference to custom keymap.
         */
        const auto& customKeymap() const {
            return customKeymap_;
        }

        /**
         * @brief Gets the dictionary handle.
         * @return CGo handle for the dictionary.
         */
        uintptr_t dictionary() const {
            return dictionary_.handle();
        }

        /**
         * @brief Gets the macro table handle.
         * @return CGo handle for the macro table.
         */
        uintptr_t macroTable() const;

        /**
         * @brief Refreshes the bamboo engine with current settings.
         */
        void refreshEngine();

        /**
         * @brief Refreshes engine options from configuration.
         */
        void refreshOption();

        /**
         * @brief Saves current configuration to disk.
         */
        void saveConfig() {
            safeSaveAsIni(config_, "conf/lotus.conf");
        }

        /**
         * @brief Updates the mode action UI.
         * @param ic Current input context.
         */
        void updateModeAction(InputContext* ic);

        /**
         * @brief Updates the spell check action UI.
         * @param ic Current input context.
         */
        void updateSpellAction(InputContext* ic);

        /**
         * @brief Updates the macro action UI.
         * @param ic Current input context.
         */
        void updateMacroAction(InputContext* ic);

        /**
         * @brief Updates the capitalize macro action UI.
         * @param ic Current input context.
         */
        void updateCapitalizeMacroAction(InputContext* ic);

        /**
         * @brief Updates the auto non-VN restore action UI.
         * @param ic Current input context.
         */
        void updateAutoNonVnRestoreAction(InputContext* ic);

        /**
         * @brief Updates the modern style action UI.
         * @param ic Current input context.
         */
        void updateModernStyleAction(InputContext* ic);

        /**
         * @brief Updates the free marking action UI.
         * @param ic Current input context.
         */
        void updateFreeMarkingAction(InputContext* ic);

        /**
         * @brief Updates the fix uinput with ACK action UI.
         * @param ic Current input context.
         */
        void updateFixUinputWithAckAction(InputContext* ic);

        /**
         * @brief Updates the Lotus icons toggle action UI.
         * @param ic Current input context.
         */
        void updateLotusIconsAction(InputContext* ic);
         * @brief Updates the mode-menu action UI.
         * @param ic Current input context.
         */
        void updateTypingModeMenuAction(InputContext* ic);

        /**
         * @brief Updates the input method action UI.
         * @param ic Current input context.
         */
        void updateInputMethodAction(InputContext* ic);

        /**
         * @brief Updates the charset action UI.
         * @param ic Current input context.
         */
        void updateCharsetAction(InputContext* ic);

        /**
         * @brief Populates input method names from bamboo core.
         */
        void populateConfig();

        /**
         * @brief Loads application-specific mode rules.
         */
        void loadAppRules();

        /**
         * @brief Saves application-specific mode rules.
         */
        void saveAppRules();

        /**
         * @brief Shows the application mode selection menu.
         * @param ic Current input context.
         */
        void showAppModeMenu(InputContext* ic);

        /**
         * @brief Closes the application mode selection menu.
         */
        void closeAppModeMenu();

        /**
         * @brief Gets the emoji loader.
         * @return Reference to emoji loader instance.
         */
        EmojiLoader& emojiLoader() {
            return emojiLoader_;
        }

        /**
         * @brief Sets the current input mode.
         * @param mode The mode to set.
         * @param ic Current input context.
         */
        void setMode(LotusMode mode, InputContext* ic);

      private:
        Instance*                                        instance_;
        lotusConfig                                      config_;
        lotusCustomKeymap                                customKeymap_;

        std::unordered_map<std::string, lotusMacroTable> macroTables_;
        std::unordered_map<std::string, CGoObject>       macroTableObject_;

        FactoryFor<LotusState>                           factory_;
        std::vector<std::string>                         imNames_;

        std::unique_ptr<SimpleAction>                    inputMethodAction_;
        std::vector<std::unique_ptr<SimpleAction>>       inputMethodSubAction_;
        std::unique_ptr<Menu>                            inputMethodMenu_;
        std::unique_ptr<SimpleAction>                    modeAction_;
        std::unique_ptr<Menu>                            modeMenu_;
        std::vector<std::unique_ptr<SimpleAction>>       modeSubAction_;
        std::unique_ptr<SimpleAction>                    charsetAction_;
        std::vector<std::unique_ptr<SimpleAction>>       charsetSubAction_;
        std::unique_ptr<Menu>                            charsetMenu_;

        std::unique_ptr<SimpleAction>                    spellCheckAction_;
        std::unique_ptr<SimpleAction>                    macroAction_;
        std::unique_ptr<SimpleAction>                    capitalizeMacroAction_;
        std::unique_ptr<SimpleAction>                    autoNonVnRestoreAction_;
        std::unique_ptr<SimpleAction>                    modernStyleAction_;
        std::unique_ptr<SimpleAction>                    freeMarkingAction_;
        std::unique_ptr<SimpleAction>                    fixUinputWithAckAction_;
        std::unique_ptr<SimpleAction>                    lotusIconsAction_;
        std::unique_ptr<SimpleAction>                    typingModeMenuAction_;
        std::vector<ScopedConnection>                    connections_;
        CGoObject                                        dictionary_;
        std::unordered_map<std::string, LotusMode>       appRules_;
        std::string                                      appRulesPath_;
        bool                                             isSelectingAppMode_ = false;
        std::string                                      currentConfigureApp_;
        LotusMode                                        globalMode_;
        EmojiLoader                                      emojiLoader_;
    };

    /**
     * @brief Factory class for creating LotusEngine instances.
     */
    class LotusFactory : public AddonFactory {
      public:
        /**
         * @brief Creates a new LotusEngine instance.
         * @param manager Pointer to addon manager.
         * @return New engine instance.
         */
        AddonInstance* create(AddonManager* manager) override {
            registerDomain("fcitx5-lotus", FCITX_INSTALL_LOCALEDIR);
            return new LotusEngine(manager->instance());
        }
    };

} // namespace fcitx

#endif // _FCITX5_LOTUS_ENGINE_H_
