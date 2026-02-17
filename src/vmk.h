/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#ifndef _FCITX5_vmk_vmk_H_
#define _FCITX5_vmk_vmk_H_

#include "bamboo-core.h"
#include "vmk-config.h"
#include "emoji.h"
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/i18n.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fcitx {

    /**
 * @brief RAII wrapper for CGo handle (uintptr_t).
 */
    class CGoObject {
      public:
        CGoObject(std::optional<uintptr_t> handle = std::nullopt) : handle_(handle) {}

        ~CGoObject() {
            if (handle_) {
                DeleteObject(*handle_);
            }
        }

        CGoObject(const CGoObject&)            = delete;
        CGoObject& operator=(const CGoObject&) = delete;

        CGoObject(CGoObject&& other)            = default;
        CGoObject& operator=(CGoObject&& other) = default;

        void       reset(std::optional<uintptr_t> handle = std::nullopt) {
            clear();
            handle_ = handle;
        }

        uintptr_t handle() const {
            return handle_.value_or(0);
        }

        uintptr_t release() {
            if (handle_) {
                uintptr_t v = *handle_;
                handle_     = std::nullopt;
                return v;
            }
            return 0;
        }

        explicit operator bool() const {
            return handle_.has_value() && *handle_ != 0;
        }

      private:
        void clear() {
            if (handle_) {
                DeleteObject(*handle_);
                handle_ = std::nullopt;
            }
        }

        std::optional<uintptr_t> handle_;
    };

    class VMKState;

    class vmkEngine final : public InputMethodEngine {
      public:
        Instance* instance() const {
            return instance_;
        }

        vmkEngine(Instance* instance);
        ~vmkEngine();

        void                 activate(const InputMethodEntry& entry, InputContextEvent& event) override;
        void                 deactivate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override;
        void                 keyEvent(const InputMethodEntry& entry, KeyEvent& keyEvent) override;

        void                 reset(const InputMethodEntry& entry, InputContextEvent& event) override;

        void                 reloadConfig() override;

        const Configuration* getConfig() const override {
            return &config_;
        }

        const Configuration* getSubConfig(const std::string& path) const override;

        void                 setConfig(const RawConfig& config) override;

        void                 setSubConfig(const std::string& path, const RawConfig& config) override;

        std::string          subMode(const fcitx::InputMethodEntry& entry, fcitx::InputContext& inputContext) override;

        std::string          overrideIcon(const fcitx::InputMethodEntry& entry) override;

        const auto&          config() const {
            return config_;
        }
        const auto& customKeymap() const {
            return customKeymap_;
        }

        uintptr_t dictionary() const {
            return dictionary_.handle();
        }

        uintptr_t macroTable() const {
            if (config_.inputMethod.value().empty()) {
                return 0;
            }
            auto it = macroTableObject_.find(*config_.inputMethod);
            if (it != macroTableObject_.end()) {
                return it->second.handle();
            }
            return 0;
        }

        void refreshEngine();
        void refreshOption();

        void saveConfig() {
            safeSaveAsIni(config_, "conf/vmk.conf");
        }
        void updateModeAction(InputContext* ic);
        void updateSpellAction(InputContext* ic);
        void updateMacroAction(InputContext* ic);
        void updateCapitalizeMacroAction(InputContext* ic);
        void updateAutoNonVnRestoreAction(InputContext* ic);
        void updateModernStyleAction(InputContext* ic);
        void updateFreeMarkingAction(InputContext* ic);
        void updateFixVmk1WithAckAction(InputContext* ic);
        void updateInputMethodAction(InputContext* ic);
        void updateCharsetAction(InputContext* ic);
        void populateConfig();
        // ibus-bamboo mode save/load
        void         loadAppRules();
        void         saveAppRules();
        void         showAppModeMenu(InputContext* ic);
        void         closeAppModeMenu();
        EmojiLoader& emojiLoader() {
            return emojiLoader_;
        }
        void setMode(VMKMode mode, InputContext* ic);

      private:
        Instance*                                         instance_;
        vmkConfig                                         config_;
        vmkCustomKeymap                                   customKeymap_;

        std::unordered_map<std::string, vmkMacroTable>    macroTables_;
        std::unordered_map<std::string, CGoObject>        macroTableObject_;

        FactoryFor<VMKState>                              factory_;
        std::vector<std::string>                          imNames_;

        std::unique_ptr<SimpleAction>                     inputMethodAction_;
        std::vector<std::unique_ptr<SimpleAction>>        inputMethodSubAction_;
        std::unique_ptr<Menu>                             inputMethodMenu_;
        std::unique_ptr<fcitx::SimpleAction>              modeAction_;
        std::unique_ptr<fcitx::Menu>                      modeMenu_;
        std::vector<std::unique_ptr<fcitx::SimpleAction>> modeSubAction_;
        std::unique_ptr<SimpleAction>                     charsetAction_;
        std::vector<std::unique_ptr<SimpleAction>>        charsetSubAction_;
        std::unique_ptr<Menu>                             charsetMenu_;

        std::unique_ptr<SimpleAction>                     spellCheckAction_;
        std::unique_ptr<SimpleAction>                     macroAction_;
        std::unique_ptr<SimpleAction>                     capitalizeMacroAction_;
        std::unique_ptr<SimpleAction>                     autoNonVnRestoreAction_;
        std::unique_ptr<SimpleAction>                     modernStyleAction_;
        std::unique_ptr<SimpleAction>                     freeMarkingAction_;
        std::unique_ptr<SimpleAction>                     fixVmk1WithAckAction_;
        std::vector<ScopedConnection>                     connections_;
        CGoObject                                         dictionary_;
        // ibus-bamboo mode save/load
        std::unordered_map<std::string, fcitx::VMKMode> appRules_;
        std::string                                     appRulesPath_;
        bool                                            isSelectingAppMode_ = false;
        std::string                                     currentConfigureApp_;
        VMKMode                                         globalMode_;
        EmojiLoader                                     emojiLoader_;
    };

    class vmkFactory : public AddonFactory {
      public:
        AddonInstance* create(AddonManager* manager) override {
            registerDomain("fcitx5-vmk", FCITX_INSTALL_LOCALEDIR);
            return new vmkEngine(manager->instance());
        }
    };

} // namespace fcitx

#endif // _FCITX5_vmk_vmk_H_