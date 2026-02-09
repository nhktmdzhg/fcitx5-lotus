/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#ifndef _FCITX5_vmk_vmkCONFIG_H_
#define _FCITX5_vmk_vmkCONFIG_H_

#include <algorithm>
#include <fcitx-config/configuration.h>
#include <fcitx-config/option.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/stringutils.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fcitx {

    enum class VMKMode {
        Off       = 0,
        VMK1      = 1,
        VMK2      = 2,
        Preedit   = 3,
        VMK1HC    = 4,
        NoMode    = 5,
        Emoji     = 6,
        VMKSmooth = 7,
    };

    // Convert mode enum to string for UI display
    inline std::string modeEnumToString(VMKMode mode) {
        switch (mode) {
            case VMKMode::Off: return "Off";
            case VMKMode::VMK1: return "vmk1";
            case VMKMode::VMK2: return "vmk2";
            case VMKMode::Preedit: return "vmkpre";
            case VMKMode::VMK1HC: return "vmk1hc";
            case VMKMode::Emoji: return "emoji";
            case VMKMode::VMKSmooth: return "vmksmooth";
            default: return "";
        }
    }

    // Convert mode string to enum
    inline VMKMode modeStringToEnum(const std::string& mode) {
        static const std::unordered_map<std::string_view, VMKMode> modeMap = {
            {"vmk1", VMKMode::VMK1}, {"vmk2", VMKMode::VMK2},   {"vmkpre", VMKMode::Preedit},      {"vmk1hc", VMKMode::VMK1HC},
            {"Off", VMKMode::Off},   {"emoji", VMKMode::Emoji}, {"vmksmooth", VMKMode::VMKSmooth},
        };
        auto it = modeMap.find(mode);
        return it != modeMap.end() ? it->second : VMKMode::NoMode;
    }

    struct InputMethodConstrain;
    struct InputMethodAnnotation;

    using InputMethodOption = Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation>;

    struct StringListAnnotation : public EnumAnnotation {
        void setList(std::vector<std::string> list) {
            list_ = std::move(list);
        }
        const auto& list() {
            return list_;
        }
        void dumpDescription(RawConfig& config) const {
            EnumAnnotation::dumpDescription(config);
            for (size_t i = 0; i < list_.size(); ++i) {
                config.setValueByPath("Enum/" + std::to_string(i), list_[i]);
            }
        }

      protected:
        std::vector<std::string> list_;
    };

    struct InputMethodAnnotation : public StringListAnnotation {
        void dumpDescription(RawConfig& config) const {
            StringListAnnotation::dumpDescription(config);
            config.setValueByPath("LaunchSubConfig", "True");
            for (size_t i = 0; i < list_.size(); ++i) {
                config.setValueByPath("SubConfigPath/" + std::to_string(i), stringutils::concat("fcitx://config/addon/vmk/macro/", list_[i]));
            }
        }
    };
    struct ModeListAnnotation : public StringListAnnotation {
        ModeListAnnotation() {
            list_ = {"vmksmooth", "vmk1", "vmk2", "vmk1hc", "vmkpre"};
        }
    };

    struct InputMethodConstrain {
        using Type = std::string;

        InputMethodConstrain(const InputMethodOption* option) : option_(option) {}

        bool check(const std::string& name) const {
            // Avoid check during initialization
            const auto& list = option_->annotation().list();
            if (list.empty()) {
                return true;
            }
            return std::find(list.begin(), list.end(), name) != list.end();
        }
        void dumpDescription(RawConfig&) const {}

      protected:
        const InputMethodOption* option_;
    };

    FCITX_CONFIGURATION(vmkKeymap, Option<std::string> key{this, "Key", _("Key"), ""}; Option<std::string> value{this, "Value", _("Value"), ""};);

    FCITX_CONFIGURATION(vmkMacroTable,
                        OptionWithAnnotation<std::vector<vmkKeymap>, ListDisplayOptionAnnotation> macros{
                            this, "Macro", _("Macro"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(vmkCustomKeymap,
                        OptionWithAnnotation<std::vector<vmkKeymap>, ListDisplayOptionAnnotation> customKeymap{
                            this, "CustomKeymap", _("Custom Keymap"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(
        vmkConfig,

        OptionWithAnnotation<std::string, ModeListAnnotation>                                            mode{this, "Mode", _("Mode"), "vmk1smooth", {}, {}, ModeListAnnotation()};
        Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation> inputMethod{
            this, "InputMethod", _("Input Method"), "Telex", InputMethodConstrain(&inputMethod), {}, InputMethodAnnotation()};
        OptionWithAnnotation<std::string, StringListAnnotation> outputCharset{this, "OutputCharset", _("Output Charset"), "Unicode", {}, {}, StringListAnnotation()};
        Option<bool> spellCheck{this, "SpellCheck", _("Enable spell check"), true}; Option<bool> macro{this, "Macro", _("Enable Macro"), true};
        Option<bool>                                                                             capitalizeMacro{this, "CapitalizeMacro", _("Capitalize Macro"), true};
        Option<bool>    autoNonVnRestore{this, "AutoNonVnRestore", _("Auto restore keys with invalid words"), true};
        Option<bool>    modernStyle{this, "ModernStyle", _("Use oà, _uý (instead of òa, úy)"), false};
        Option<bool>    freeMarking{this, "FreeMarking", _("Allow type with more freedom"), true};
        SubConfigOption customKeymap{this, "CustomKeymap", _("Custom Keymap"), "fcitx://config/addon/vmk/custom_keymap"};);

} // namespace fcitx

#endif