/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#ifndef _FCITX5_LOTUS_CONFIG_H_
#define _FCITX5_LOTUS_CONFIG_H_

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

    enum class LotusMode {
        Off             = 0,
        Uinput          = 1,
        SurroundingText = 2,
        Preedit         = 3,
        UinputHC        = 4,
        NoMode          = 5,
        Emoji           = 6,
        Smooth          = 7,
    };

    // Convert mode enum to string for UI display
    inline std::string modeEnumToString(LotusMode mode) {
        switch (mode) {
            case LotusMode::Off: return "Off";
            case LotusMode::Uinput: return "uinput";
            case LotusMode::SurroundingText: return "surrounding";
            case LotusMode::Preedit: return "preedit";
            case LotusMode::UinputHC: return "uinputhc";
            case LotusMode::Emoji: return "emoji";
            case LotusMode::Smooth: return "smooth";
            default: return "";
        }
    }

    // Convert mode string to enum
    inline LotusMode modeStringToEnum(const std::string& mode) {
        static const std::unordered_map<std::string_view, LotusMode> modeMap = {
            {"uinput", LotusMode::Uinput},   {"surrounding", LotusMode::SurroundingText},
            {"preedit", LotusMode::Preedit}, {"uinputhc", LotusMode::UinputHC},
            {"Off", LotusMode::Off},         {"emoji", LotusMode::Emoji},
            {"smooth", LotusMode::Smooth},
        };
        auto it = modeMap.find(mode);
        return it != modeMap.end() ? it->second : LotusMode::NoMode;
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
                config.setValueByPath("SubConfigPath/" + std::to_string(i), stringutils::concat("fcitx://config/addon/lotus/macro/", list_[i]));
            }
        }
    };
    struct ModeListAnnotation : public StringListAnnotation {
        ModeListAnnotation() {
            list_ = {"smooth", "uinput", "surrounding", "preedit", "uinputhc"};
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

    FCITX_CONFIGURATION(lotusKeymap, Option<std::string> key{this, "Key", _("Key"), ""}; Option<std::string> value{this, "Value", _("Value"), ""};);

    FCITX_CONFIGURATION(lotusMacroTable,
                        OptionWithAnnotation<std::vector<lotusKeymap>, ListDisplayOptionAnnotation> macros{
                            this, "Macro", _("Macro"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(lotusCustomKeymap,
                        OptionWithAnnotation<std::vector<lotusKeymap>, ListDisplayOptionAnnotation> customKeymap{
                            this, "CustomKeymap", _("Custom Keymap"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(
        lotusConfig,

        OptionWithAnnotation<std::string, ModeListAnnotation>                                            mode{this, "Mode", _("Mode"), "smooth", {}, {}, ModeListAnnotation()};
        Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation> inputMethod{
            this, "InputMethod", _("Input Method"), "Telex", InputMethodConstrain(&inputMethod), {}, InputMethodAnnotation()};
        OptionWithAnnotation<std::string, StringListAnnotation> outputCharset{this, "OutputCharset", _("Output Charset"), "Unicode", {}, {}, StringListAnnotation()};
        Option<bool> spellCheck{this, "SpellCheck", _("Enable spell check"), true}; Option<bool> macro{this, "Macro", _("Enable Macro"), true};
        Option<bool>                                                                             capitalizeMacro{this, "CapitalizeMacro", _("Capitalize Macro"), true};
        Option<bool>    autoNonVnRestore{this, "AutoNonVnRestore", _("Auto restore keys with invalid words"), true};
        Option<bool>    modernStyle{this, "ModernStyle", _("Use oà, _uý (instead of òa, úy)"), true};
        Option<bool>    freeMarking{this, "FreeMarking", _("Allow type with more freedom"), true};
        Option<bool>    fixUinputWithAck{this, "FixUinputWithAck", _("Fix uinput mode with ack"), false};
        SubConfigOption customKeymap{this, "CustomKeymap", _("Custom Keymap"), "fcitx://config/addon/lotus/custom_keymap"};);

} // namespace fcitx

#endif