/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/**
 * @file lotus-config.h
 * @brief Configuration definitions for fcitx5-lotus input method.
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

    /**
     * @brief Operating modes for the Lotus input method.
     */
    enum class LotusMode {
        Off             = 0,
        Smooth          = 1,
        Uinput          = 2,
        UinputHC        = 3,
        SurroundingText = 4,
        Preedit         = 5,
        Emoji           = 6,
        NoMode          = 7,
    };

    /**
     * @brief Converts LotusMode enum to display string.
     * @param mode The mode to convert.
     * @return Human-readable mode name.
     */
    inline std::string modeEnumToString(LotusMode mode) {
        switch (mode) {
            case LotusMode::Off: return "OFF";
            case LotusMode::Uinput: return "Uinput (Slow)";
            case LotusMode::SurroundingText: return "Surrounding Text";
            case LotusMode::Preedit: return "Preedit";
            case LotusMode::UinputHC: return "Uinput (Hardcore)";
            case LotusMode::Emoji: return "Emoji Picker";
            case LotusMode::Smooth: return "Uinput (Smooth)";
            default: return "";
        }
    }

    /**
     * @brief Converts mode string to LotusMode enum.
     * @param mode The string to parse.
     * @return Corresponding LotusMode value.
     */
    inline LotusMode modeStringToEnum(const std::string& mode) {
        static const std::unordered_map<std::string_view, LotusMode> modeMap = {
            {"OFF", LotusMode::Off},
            {"Uinput (Slow)", LotusMode::Uinput},
            {"Surrounding Text", LotusMode::SurroundingText},
            {"Preedit", LotusMode::Preedit},
            {"Uinput (Hardcore)", LotusMode::UinputHC},
            {"Emoji Picker", LotusMode::Emoji},
            {"Uinput (Smooth)", LotusMode::Smooth},
        };
        auto it = modeMap.find(mode);
        return it != modeMap.end() ? it->second : LotusMode::NoMode;
    }

    struct InputMethodConstrain;
    struct InputMethodAnnotation;

    using InputMethodOption = Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation>;

    /**
     * @brief Annotation for string list options in configuration UI.
     */
    struct StringListAnnotation : public EnumAnnotation {
        /**
         * @brief Sets the string list.
         * @param list Vector of strings to set.
         */
        void setList(std::vector<std::string> list) {
            list_ = std::move(list);
        }

        /**
         * @brief Gets the string list.
         * @return Reference to the list.
         */
        const auto& list() {
            return list_;
        }

        /**
         * @brief Dumps description to config.
         * @param config Config to write to.
         */
        void dumpDescription(RawConfig& config) const {
            EnumAnnotation::dumpDescription(config);
            for (size_t i = 0; i < list_.size(); ++i) {
                config.setValueByPath("Enum/" + std::to_string(i), list_[i]);
            }
        }

      protected:
        std::vector<std::string> list_;
    };

    /**
     * @brief Annotation for input method selection with sub-config support.
     */
    struct InputMethodAnnotation : public StringListAnnotation {
        /**
         * @brief Dumps description with sub-config paths.
         * @param config Config to write to.
         */
        void dumpDescription(RawConfig& config) const {
            StringListAnnotation::dumpDescription(config);
            config.setValueByPath("LaunchSubConfig", "True");
            for (size_t i = 0; i < list_.size(); ++i) {
                config.setValueByPath("SubConfigPath/" + std::to_string(i), stringutils::concat("fcitx://config/addon/lotus/macro/", list_[i]));
            }
        }
    };

    /**
     * @brief Annotation for mode list with predefined options.
     */
    struct ModeListAnnotation : public StringListAnnotation {
        /**
         * @brief Initializes with default mode list.
         */
        ModeListAnnotation() {
            list_ = {"Uinput (Smooth)", "Uinput (Slow)", "Surrounding Text", "Preedit", "Uinput (Hardcore)", "OFF"};
        }
    };

    /**
     * @brief Constraint validator for input method options.
     */
    struct InputMethodConstrain {
        using Type = std::string;

        /**
         * @brief Constructs with option pointer.
         * @param option Pointer to input method option.
         */
        InputMethodConstrain(const InputMethodOption* option) : option_(option) {}

        /**
         * @brief Validates if name is in the allowed list.
         * @param name Name to check.
         * @return True if valid.
         */
        bool check(const std::string& name) const {
            const auto& list = option_->annotation().list();
            if (list.empty()) {
                return true;
            }
            return std::find(list.begin(), list.end(), name) != list.end();
        }

        /**
         * @brief Dumps description (no-op).
         * @param config Unused.
         */
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

    /**
     * @brief Main configuration structure for Lotus input method.
     */
    FCITX_CONFIGURATION(
        lotusConfig,

        OptionWithAnnotation<std::string, ModeListAnnotation> mode{this, "Mode", _("Mode"), "Uinput (Smooth)", {}, {}, ModeListAnnotation()};
        Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation> inputMethod{
            this, "InputMethod", _("Input Method"), "Telex", InputMethodConstrain(&inputMethod), {}, InputMethodAnnotation()};
        OptionWithAnnotation<std::string, StringListAnnotation> outputCharset{this, "OutputCharset", _("Output Charset"), "Unicode", {}, {}, StringListAnnotation()};
        Option<bool> spellCheck{this, "SpellCheck", _("Enable spell check"), true}; Option<bool> macro{this, "Macro", _("Enable Macro"), true};
        Option<bool>                                                                             capitalizeMacro{this, "CapitalizeMacro", _("Capitalize Macro"), true};
        Option<bool>    autoNonVnRestore{this, "AutoNonVnRestore", _("Auto restore keys with invalid words"), true};
        Option<bool>    modernStyle{this, "ModernStyle", _("Use oà, _uý (instead of òa, úy)"), true};
        Option<bool>    freeMarking{this, "FreeMarking", _("Allow type with more freedom"), true};
        Option<bool>    fixUinputWithAck{this, "FixUinputWithAck", _("Fix uinput mode with ack"), false};
        Option<bool>    useLotusIcons{this, "UseLotusIcons", _("Use Lotus status icons"), false};
        SubConfigOption customKeymap{this, "CustomKeymap", _("Custom Keymap"), "fcitx://config/addon/lotus/custom_keymap"}; KeyListOption modeMenuKey{
            this, "ModeMenuKey", _("Mode menu hotkey"), {Key("grave")}, KeyListConstrain({KeyConstrainFlag::AllowModifierLess, KeyConstrainFlag::AllowModifierOnly})};);

} // namespace fcitx

#endif
