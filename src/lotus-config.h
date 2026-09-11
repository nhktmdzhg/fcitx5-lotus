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

#include <cstdint>
#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/stringutils.h>

namespace fcitx {

    /**
     * @brief Operating modes for the Lotus input method.
     */
    enum class LotusMode : std::uint8_t {
        Off,
        Smooth,
        SuperSmooth,
        Uinput,
        SurroundingText,
        Preedit,
        Emoji,
        Minecraft,
        UinputBackspace,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(LotusMode, N_("OFF"), N_("Uinput (Smooth)"), N_("Uinput (Super Smooth)"), N_("Uinput (Slow)"), N_("Surrounding Text"), N_("Preedit"),
                                     N_("Emoji Picker"), N_("Minecraft"), N_("Uinput (Backspace)"));

    /**
     * @brief Converts LotusMode to int and vice versa.
     */
    int       modeToInt(LotusMode mode);
    LotusMode intToMode(int mode);

    /**
     * @brief W2U mode for w to ư conversion.
     */
    enum class W2UMode : std::uint8_t {
        Disabled   = 0,
        NonStart   = 1,
        Everywhere = 2,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(W2UMode, N_("Disabled"), N_("Non-Start"), N_("Everywhere"));

    /**
     * @brief Bracket transform mode for [ -> ơ, ] -> ư conversion.
     */
    enum class BracketTransformMode : std::uint8_t {
        Disabled   = 0,
        NonStart   = 1,
        Everywhere = 2,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(BracketTransformMode, N_("Disabled"), N_("Non-Start"), N_("Everywhere"));

    /**
     * @brief Modifier that toggles macro skip for the next word.
     */
    enum class MacroSkipTriggerModifier : std::uint8_t {
        Disabled = 0,
        Shift    = 1,
        Ctrl     = 2,
        Alt      = 3,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(MacroSkipTriggerModifier, N_("Disabled"), N_("Shift"), N_("Ctrl"), N_("Alt"));

    /**
     * @brief Icon theme options.
     */
    enum class IconTheme : std::uint8_t {
        Auto,
        Light,
        Dark,
    };

    FCITX_CONFIG_ENUM_NAME_WITH_I18N(IconTheme, N_("Auto"), N_("Light"), N_("Dark"));

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
            config.setValueByPath("IsEnum", "True");
            for (size_t i = 0; i < list_.size(); ++i) {
                config.setValueByPath("Enum/" + std::to_string(i), list_[i]);
            }
        }

      protected:
        std::vector<std::string> list_; // NOLINT
    };

    struct InputMethodAnnotation : public StringListAnnotation {
        /**
         * @brief Dumps description with sub-config paths.
         * @param config Config to write to.
         */
        void dumpDescription(RawConfig& config) const {
            StringListAnnotation::dumpDescription(config);
            config.setValueByPath("LaunchSubConfig", "True");
        }
    };

    /**
     * @brief Annotation for time format list.
     */
    struct TimeFormatAnnotation : public StringListAnnotation {
        TimeFormatAnnotation() {
            list_ = {"%H:%M", "%H:%M:%S", "%I:%M:%S %p", "%I:%M %p", ""};
        }
    };

    /**
     * @brief Annotation for date format list.
     */
    struct DateFormatAnnotation : public StringListAnnotation {
        DateFormatAnnotation() {
            list_ = {"%d/%m/%Y", "%m/%d/%Y", "%Y-%m-%d", "%d/%m/%y", "%y-%m-%d", ""};
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
        void dumpDescription(RawConfig& /*unused*/) const {}

      private:
        const InputMethodOption* option_;
    };

    FCITX_CONFIGURATION(lotusKeymap, Option<std::string> key{this, "Key", _("Key"), ""}; Option<std::string> value{this, "Value", _("Value"), ""};);

    FCITX_CONFIGURATION(lotusMacroTable,
                        OptionWithAnnotation<std::vector<lotusKeymap>, ListDisplayOptionAnnotation> macros{
                            this, "Macro", _("Macro"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(lotusCustomKeymap,
                        OptionWithAnnotation<std::vector<lotusKeymap>, ListDisplayOptionAnnotation> customKeymap{
                            this, "CustomKeymap", _("Custom Keymap"), {}, {}, {}, ListDisplayOptionAnnotation("Key")};);

    FCITX_CONFIGURATION(lotusAppRule, Option<std::string> app{this, "App", _("App"), ""}; Option<int> mode{this, "Mode", _("Mode"), 0};);
    FCITX_CONFIGURATION(lotusAppRules,
                        OptionWithAnnotation<std::vector<lotusAppRule>, ListDisplayOptionAnnotation> rules{
                            this, "Rules", _("Rules"), {}, {}, {}, ListDisplayOptionAnnotation("App")};);

    /**
     * @brief Main configuration structure for Lotus input method.
     */
    FCITX_CONFIGURATION(
        lotusConfig,

        OptionWithAnnotation<LotusMode, LotusModeI18NAnnotation>                                         mode{this, "Mode", _("Mode"), LotusMode::Preedit};
        Option<std::string, InputMethodConstrain, DefaultMarshaller<std::string>, InputMethodAnnotation> inputMethod{
            this, "InputMethod", _("Input Method"), "Telex", InputMethodConstrain(&inputMethod), {}, InputMethodAnnotation()};
        OptionWithAnnotation<std::string, StringListAnnotation> outputCharset{this, "OutputCharset", _("Output Charset"), "Unicode", {}, {}, StringListAnnotation()};
        KeyListOption                                           modeMenuKey{
            this, "ModeMenuKey", _("Mode Menu Hotkey"), {Key("grave")}, KeyListConstrain({KeyConstrainFlag::AllowModifierLess, KeyConstrainFlag::AllowModifierOnly})};
        KeyListOption cycleModeKey{this, "CycleModeKey", _("Cycle Mode Hotkey"), {}, KeyListConstrain({KeyConstrainFlag::AllowModifierLess, KeyConstrainFlag::AllowModifierOnly})};
        SubConfigOption                                                                appRules{this, "AppRules", _("App Rules"), "fcitx://config/addon/lotus/app_rules"};
        OptionWithAnnotation<W2UMode, W2UModeI18NAnnotation>                           w2u{this, "W2U", _("Type w to Produce ư"), W2UMode::NonStart};
        OptionWithAnnotation<BracketTransformMode, BracketTransformModeI18NAnnotation> bracketTransform{this, "BracketTransform", _("Type [ -> ơ, ] -> ư, { -> Ơ, } -> Ư"),
                                                                                                        BracketTransformMode::Disabled};

        Option<bool> spellCheck{this, "SpellCheck", _("Enable Spell Check Using Dictionary"), true}; Option<bool> enableMacro{this, "EnableMacro", _("Enable Macro"), true};
        Option<bool>                                                                           capitalizeMacro{this, "CapitalizeMacro", _("Capitalize Macro"), true};
        OptionWithAnnotation<MacroSkipTriggerModifier, MacroSkipTriggerModifierI18NAnnotation> macroSkipTriggerModifier{
            this, "MacroSkipTriggerModifier", _("Modifier to Skip Macro for Next Word"), MacroSkipTriggerModifier::Disabled};
        Option<bool>        autoCapitalizeAfterPunctuation{this, "AutoCapitalizeAfterPunctuation", _("Auto capitalize after sentence-ending punctuation (. ! ? Enter)"), false};
        Option<bool>        doubleSpaceToPeriod{this, "DoubleSpaceToPeriod", _("Double Space to Period"), false};
        Option<bool>        doubleHyphenToEmDash{this, "DoubleHyphenToEmDash", _("Double Hyphen to Em-Dash (--)"), false};
        Option<bool>        autoNonVnRestore{this, "AutoNonVnRestore", _("Auto Restore Invalid Words"), true};
        Option<bool>        modernStyle{this, "ModernStyle", _("Use oà, uý (Instead Of òa, úy)"), true};
        Option<bool>        freeMarking{this, "FreeMarking", _("Allow Type With More Freedom"), true};
        Option<bool>        ddFreeStyle{this, "DdFreeStyle", _("Allow dd To Produce đ When Auto Restore Invalid Words Is On"), true};
        Option<bool>        fixUinputWithAck{this, "FixUinputWithAck", _("Fix Uinput Mode With Ack"), false};
        Option<bool>        useLotusIcons{this, "UseLotusIcons", _("Use Lotus Status Icons"), false};

        Option<bool>        enableDictionary{this, "EnableDictionary", _("Custom Dictionary"), false};
        Option<bool>        enableCustomKeymap{this, "EnableCustomKeymap", _("Custom Keymap"), false};

        Option<bool>        showModeSmooth{this, "ShowModeSmooth", _("Show Uinput (Smooth)"), true};
        Option<std::string> shortcutSmooth{this, "ShortcutSmooth", _("Shortcut for Uinput (Smooth)"), "1"};
        Option<bool>        showModeUinput{this, "ShowModeUinput", _("Show Uinput (Slow)"), true};
        Option<std::string> shortcutUinput{this, "ShortcutUinput", _("Shortcut for Uinput (Slow)"), "2"};
        Option<bool>        showModeSuperSmooth{this, "ShowModeSuperSmooth", _("Show Uinput (Super Smooth)"), true};
        Option<std::string> shortcutSuperSmooth{this, "ShortcutSuperSmooth", _("Shortcut for Uinput (Super Smooth)"), "a"};
        Option<bool>        showModeMinecraft{this, "ShowModeMinecraft", _("Show Minecraft"), true};
        Option<std::string> shortcutMinecraft{this, "ShortcutMinecraft", _("Shortcut for Minecraft"), "3"};
        Option<bool>        showModeSurroundingText{this, "ShowModeSurroundingText", _("Show Surrounding Text"), true};
        Option<std::string> shortcutSurroundingText{this, "ShortcutSurroundingText", _("Shortcut for Surrounding Text"), "4"};
        Option<bool>        showModePreedit{this, "ShowModePreedit", _("Show Preedit"), true};
        Option<std::string> shortcutPreedit{this, "ShortcutPreedit", _("Shortcut for Preedit"), "q"};
        Option<bool>        showModeEmoji{this, "ShowModeEmoji", _("Show Emoji Picker"), true};
        Option<std::string> shortcutEmoji{this, "ShortcutEmoji", _("Shortcut for Emoji Picker"), "w"}; Option<bool> showModeOff{this, "ShowModeOff", _("Show OFF"), true};
        Option<std::string> shortcutOff{this, "ShortcutOff", _("Shortcut for OFF"), "e"}; Option<bool> showModeDefault{this, "ShowModeDefault", _("Show Default Typing"), true};
        Option<std::string> shortcutDefault{this, "ShortcutDefault", _("Shortcut for Default Typing"), "r"};
        Option<bool>        showModeUinputBackspace{this, "ShowModeUinputBackspace", _("Show Uinput (Backspace)"), true};
        Option<std::string> shortcutUinputBackspace{this, "ShortcutUinputBackspace", _("Shortcut for Uinput (Backspace)"), "b"};
        Option<int>         uinputBackspaceAfterWaitMs{this, "UinputBackspaceAfterWaitMs", _("After Backspace Wait Time (ms)"), 10};
        Option<bool>        enableMacroInOffMode{this, "EnableMacroInOffMode", _("Allow Macro in Off Mode"), false};

        Option<bool>        useSurroundingTextIfPossible{this, "useSurroundingTextIfPossible", _("Use Surrounding Text if possible"), false};

        Option<std::string> modeOrder{this, "ModeOrder", _("Mode Order"), "Smooth,Uinput,Minecraft,SurroundingText,Preedit,Emoji,Off,SuperSmooth,UinputBackspace,Default"};

        OptionWithAnnotation<std::string, TimeFormatAnnotation>  timeFormat{this, "TimeFormat", _("Time Format ($TIME in macro)"), "%H:%M", {}, {}, TimeFormatAnnotation()};
        OptionWithAnnotation<std::string, DateFormatAnnotation>  dateFormat{this, "DateFormat", _("Date Format ($DATE in macro)"), "%d/%m/%Y", {}, {}, DateFormatAnnotation()};

        SubConfigOption                                          macroEditor{this, "MacroEditor", _("Macro"), "fcitx://config/addon/lotus/lotus-macro"};
        SubConfigOption                                          customKeymap{this, "CustomKeymap", _("Custom Keymap"), "fcitx://config/addon/lotus/custom_keymap"};
        OptionWithAnnotation<IconTheme, IconThemeI18NAnnotation> iconTheme{this, "IconTheme", _("Icon Color"), IconTheme::Auto};);

} // namespace fcitx

#endif
