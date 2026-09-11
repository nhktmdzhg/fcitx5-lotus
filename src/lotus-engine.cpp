/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus-engine.h"
#include "fcitx-utils/keysym.h"
#include "lotus-config.h"
#include "lotus-state.h"
#include "lotus-candidates.h"
#include "lotus-monitor.h"
#include "lotus-utils.h"
#include "lotus-icon-resolver.h"
#include "ack-apps.h"
#include <optional>
#include <sys/socket.h>
#include <utility>

#include <fcitx-config/iniparser.h>
#include <fcitx/menu.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/utf8.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/misc.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include <fcntl.h>
#include <sstream>

namespace fcitx {
    constexpr const char* CharsetActionPrefix = "lotus-charset-";
    const std::string     CustomKeymapFile    = "conf/lotus-custom-keymap.conf";
    const std::string     MacroTableFile      = "conf/lotus-macro-table.conf";

    int                   modeToInt(LotusMode mode) {
        switch (mode) {
            case LotusMode::Off: return 0;
            case LotusMode::Smooth: return 1;
            case LotusMode::Uinput: return 2;
            case LotusMode::SuperSmooth: return 3;
            case LotusMode::SurroundingText: return 4;
            case LotusMode::Preedit: return 5;
            case LotusMode::Emoji: return 6;
            case LotusMode::Minecraft: return 8;
            case LotusMode::UinputBackspace: return 9;
            default: return 0;
        }
    }

    LotusMode intToMode(int mode) {
        switch (mode) {
            case 0: return LotusMode::Off;
            case 1: return LotusMode::Smooth;
            case 2: return LotusMode::Uinput;
            case 3: return LotusMode::SuperSmooth;
            case 4: return LotusMode::SurroundingText;
            case 5: return LotusMode::Preedit;
            case 6: return LotusMode::Emoji;
            case 8: return LotusMode::Minecraft;
            case 9: return LotusMode::UinputBackspace;
            default: return LotusMode::Off;
        }
    }

    // Returns the KeySym that triggers the "Type hotkey char" action in the mode
    // menu.  If the hotkey itself conflicts with a reserved menu key, falls back
    // to FcitxKey_f.
    static bool isAppModeMenuReservedKey(KeySym sym, const lotusConfig& config) {
        if (sym == Key(*config.shortcutSmooth).sym() || sym == Key(*config.shortcutUinput).sym() || sym == Key(*config.shortcutMinecraft).sym() ||
            sym == Key(*config.shortcutSurroundingText).sym() || sym == Key(*config.shortcutPreedit).sym() || sym == Key(*config.shortcutEmoji).sym() ||
            sym == Key(*config.shortcutOff).sym() || sym == Key(*config.shortcutSuperSmooth).sym() || sym == Key(*config.shortcutDefault).sym() ||
            sym == Key(*config.shortcutUinputBackspace).sym()) {
            return true;
        }

        switch (sym) {
            case FcitxKey_Escape:
            case FcitxKey_Tab:
            case FcitxKey_ISO_Left_Tab:
            case FcitxKey_Return:
            case FcitxKey_space:
            case FcitxKey_Up:
            case FcitxKey_Down: return true;
            default: return false;
        }
    }

    static KeySym typeKeyForModeMenuHotkey(KeySym hotkeySym, const lotusConfig& config) {
        return isAppModeMenuReservedKey(hotkeySym, config) ? FcitxKey_f : hotkeySym;
    }

    bool LotusEngine::isDarkMode() {
        // Each probe spawns subprocesses, and subModeIconImpl calls this on
        // every tray update while IconTheme is Auto.  Cache the result briefly
        // so the cost is paid at most once per few seconds.
        static int64_t lastCheckMs = 0;
        static bool    cachedValue = false;
        const int64_t  now         = now_ms();
        if (now - lastCheckMs < 5000) {
            return cachedValue;
        }
        lastCheckMs = now;
        cachedValue = false;

        // GTK_THEME is honored by lightweight DEs that lack the settings
        // portal; covers XFCE, openbox, etc. with a dark theme.
        if (const char* theme = std::getenv("GTK_THEME")) {
            std::string t(theme);
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            if (t.find("dark") != std::string::npos) {
                cachedValue = true;
                return cachedValue;
            }
        }

        FILE* pipe = popen("dbus-send --session --dest=org.freedesktop.portal.Desktop --print-reply /org/freedesktop/portal/desktop org.freedesktop.portal.Settings.ReadOne "
                           "string:'org.freedesktop.appearance' string:'color-scheme' 2>/dev/null",
                           "r");
        if (pipe != nullptr) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                uint32_t value = 0;
                if (sscanf(buffer, "%*[^v]variant uint32 %u", &value) == 1) {
                    pclose(pipe);
                    cachedValue = value == 1;
                    return cachedValue;
                }
            }
            pclose(pipe);
        }

        pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
        if (pipe != nullptr) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                pclose(pipe);
                cachedValue = strstr(buffer, "prefer-dark") != nullptr;
                return cachedValue;
            }
            pclose(pipe);
        }

        // GTK settings file — covers DEs where the dark preference is stored
        // there instead of being exposed via portal/gsettings.
        if (const char* home = std::getenv("HOME")) {
            std::ifstream settingsFile(std::string(home) + "/.config/gtk-3.0/settings.ini");
            if (settingsFile.is_open()) {
                std::string line;
                while (std::getline(settingsFile, line)) {
                    if (line.find("gtk-application-prefer-dark-theme=1") != std::string::npos) {
                        cachedValue = true;
                        return cachedValue;
                    }
                }
            }
            settingsFile.close();
        }

        return cachedValue;
    }

    static inline uintptr_t newMacroTable(const lotusMacroTable& macroTable) {
        const auto&        macros = *macroTable.macros;
        std::vector<char*> charArray;
        charArray.reserve((macros.size() * 2) + 1);
        for (const auto& keymap : macros) {
            // External C API doesn't use const, but doesn't modify data
            charArray.push_back(const_cast<char*>(keymap.key->data()));   //NOLINT
            charArray.push_back(const_cast<char*>(keymap.value->data())); //NOLINT
        }
        charArray.push_back(nullptr);
        return NewMacroTable(charArray.data());
    }

    static inline std::vector<std::string> convertToStringList(char** list) {
        std::vector<std::string> result;
        if (list != nullptr) {
            for (size_t i = 0; list[i] != nullptr; ++i) { //NOLINT
                result.emplace_back(list[i]);             //NOLINT
                free(list[i]);                            //NOLINT
            }
            free(list); //NOLINT
        }
        return result;
    }

    uintptr_t LotusEngine::macroTable() const {
        if (config_.inputMethod.value().empty()) {
            return 0;
        }
        return macroTableObject_.handle();
    }

    LotusEngine::LotusEngine(Instance* instance) : instance_(instance), factory_([this](InputContext& ic) { return new LotusState(this, &ic); }) { //NOLINT
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
        isGnome_            = (desktop != nullptr) && std::string(desktop).find("GNOME") != std::string::npos;
        // emptyCustomKeymap_.customKeymap is implicitly initialized to empty by fcitx::Option default value macro.
        Init();
        {
            auto imNames = convertToStringList(GetInputMethodNames());
            imNames.push_back("Custom");
            imNames_ = std::move(imNames);
        }
        config_.inputMethod.annotation().setList(imNames_);

        auto& uiManager = instance_->userInterfaceManager();

        charsetAction_ = std::make_unique<SimpleAction>();
        charsetAction_->setShortText(_("Charset"));
        charsetAction_->setIcon("character-set");
        uiManager.registerAction("lotus-charset", charsetAction_.get());
        charsetMenu_ = std::make_unique<Menu>();
        charsetAction_->setMenu(charsetMenu_.get());

        auto charsets = convertToStringList(GetCharsetNames());
        for (const auto& charset : charsets) {
            charsetSubAction_.emplace_back(std::make_unique<SimpleAction>());
            auto* action = charsetSubAction_.back().get();
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

        initToggleAction(spellCheckAction_, config_.spellCheck, "lotus-spellcheck", "tools-check-spelling", _("Spell Check"), _("Spell Check"), uiManager);
        initToggleAction(macroAction_, config_.enableMacro, "lotus-macro", "document-edit", _("Macro"), _("Macro"), uiManager);
        initToggleAction(capitalizeMacroAction_, config_.capitalizeMacro, "lotus-capitalizemacro", "format-text-uppercase", _("Capitalize Macro"), _("Capitalize Macro"),
                         uiManager);
        initToggleAction(autoNonVnRestoreAction_, config_.autoNonVnRestore, "lotus-autonvnrestore", "edit-undo", _("Auto Restore Invalid Words"), _("Auto Non-VN Restore"),
                         uiManager);
        initToggleAction(enableDictionaryAction_, config_.enableDictionary, "lotus-dictionary", "accessories-dictionary", _("Custom Dictionary"), _("Custom Dictionary"),
                         uiManager);

        settingsAction_ = std::make_unique<SimpleAction>();
        settingsAction_->setShortText(_("Settings"));
        settingsAction_->setIcon("configure");
        connections_.emplace_back(settingsAction_->connect<SimpleAction::Activated>([](InputContext*) { startProcess({FCITX5_LOTUS_SETTINGS_PATH}); }));
        uiManager.registerAction("lotus-settings", settingsAction_.get());

#if LOTUS_USE_MODERN_FCITX_API
        std::string configDir = (StandardPaths::global().userDirectory(StandardPathsType::Config) / "fcitx5" / "conf").string();
#else
        std::string configDir = StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
#endif

        if (!std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
        }
        reloadConfig();
        instance_->inputContextManager().registerProperty("LotusState", &factory_);
        appRulesPath_ = configDir + "/lotus-app-rules.conf";
        loadAppRules();
        toggleActions_ = {charsetAction_.get(),          spellCheckAction_.get(),       macroAction_.get(),   capitalizeMacroAction_.get(),
                          autoNonVnRestoreAction_.get(), enableDictionaryAction_.get(), settingsAction_.get()};
    }

    void LotusEngine::initToggleAction(std::unique_ptr<SimpleAction>& action, Option<bool>& option, const std::string& actionId, const std::string& iconName,
                                       const std::string& textLong, const std::string& textOnOff, UserInterfaceManager& uiManager) {
        action = std::make_unique<SimpleAction>();
        action->setShortText(textLong);
        action->setIcon(iconName);
        action->setCheckable(false);
        connections_.emplace_back(action->connect<SimpleAction::Activated>([this, &action, &option, textOnOff](InputContext* ic) {
            option.setValue(!option.value());
            saveConfig();
            refreshOption();
            updateAction(ic, action, option, textOnOff);
        }));
        uiManager.registerAction(actionId, action.get());
    }

    void LotusEngine::updateAction(InputContext* ic, std::unique_ptr<SimpleAction>& action, Option<bool>& option, const std::string& textOnOff) {
        action->setShortText((option.value() ? "✔ " : "✖ ") + textOnOff);
        if (ic != nullptr) {
            action->update(ic);
        }
    }

    LotusEngine::~LotusEngine() {
        stop_flag_monitor.store(true, std::memory_order_release);
        int fd = mouse_socket_fd.load(std::memory_order_acquire);
        if (fd >= 0) {
            shutdown(fd, SHUT_RDWR);
        }
        if (mouse_thread.joinable()) {
            mouse_thread.join();
        }
        int old_fd = uinput_client_fd_.exchange(-1);
        if (old_fd != -1) {
            close(old_fd);
        }
        LOTUS_INFO("Engine destroyed.");
    }

    const lotusCustomKeymap& LotusEngine::customKeymap() const {
        if (config_.enableCustomKeymap.value()) {
            return customKeymap_;
        }
        return emptyCustomKeymap_;
    }

    void LotusEngine::reloadConfig() {
        readAsIni(config_, "conf/lotus.conf");
        readAsIni(customKeymap_, CustomKeymapFile);
        readAsIni(macroTables_, MacroTableFile);
        macroTableObject_.reset(newMacroTable(macroTables_));
        if (config_.enableDictionary.value()) {
#if LOTUS_USE_MODERN_FCITX_API
            auto fd = StandardPaths::global().open(StandardPathsType::PkgData, "lotus/vietnamese.cm.dict");
#else
            auto fd = StandardPath::global().open(StandardPath::Type::PkgData, "lotus/vietnamese.cm.dict", O_RDONLY);
#endif
            if (fd.isValid()) {
                dictionary_.reset(NewDictionary(fd.release()));
            }
        } else {
#if LOTUS_USE_MODERN_FCITX_API
            auto paths = StandardPaths::global().locateAll(StandardPathsType::PkgData, "lotus/vietnamese.cm.dict");
#else
            auto paths = StandardPath::global().locateAll(StandardPath::Type::PkgData, "lotus/vietnamese.cm.dict");
#endif
            for (const auto& p : paths) {
#if LOTUS_USE_MODERN_FCITX_API
                if (!isStartsWith(p.string(), "/home/")) {
                    auto fd = fcitx::UnixFD(::open(p.c_str(), O_RDONLY));
                    if (fd.isValid()) {
                        dictionary_.reset(NewDictionary(fd.release()));
#else
                if (!isStartsWith(p, "home/")) {
                    int fd = ::open(p.c_str(), O_RDONLY);
                    if (fd != -1) {
                        dictionary_.reset(NewDictionary(fd));
#endif
                        break;
                    }
                }
            }
        }
        loadAppRules();
        populateConfig();
    }

    const Configuration* LotusEngine::getSubConfig(const std::string& path) const {
        if (path == "custom_keymap")
            return &customKeymap_;
        if (path == "lotus-macro") {
            return &macroTables_;
        }
        if (path == "app_rules") {
            return &appRulesTables_;
        }
        return nullptr;
    }

    void LotusEngine::setConfig(const RawConfig& config) {
        config_.load(config, true);
        saveConfig();
        populateConfig();
    }

    void LotusEngine::populateConfig() {
        refreshEngine();
        refreshOption();
        updateCharsetAction(nullptr);
        updateAction(nullptr, spellCheckAction_, config_.spellCheck, _("Spell Check"));
        updateAction(nullptr, macroAction_, config_.enableMacro, _("Macro"));
        updateAction(nullptr, capitalizeMacroAction_, config_.capitalizeMacro, _("Capitalize Macro"));
        updateAction(nullptr, autoNonVnRestoreAction_, config_.autoNonVnRestore, _("Auto Non-VN Restore"));
        updateAction(nullptr, enableDictionaryAction_, config_.enableDictionary, _("Custom Dictionary"));
    }

    void LotusEngine::setSubConfig(const std::string& path, const RawConfig& config) {
        if (path == "custom_keymap") {
            customKeymap_.load(config, true);
            safeSaveAsIni(customKeymap_, CustomKeymapFile);
            refreshEngine();
        } else if (path == "lotus-macro") {
            macroTables_.load(config, true);
            safeSaveAsIni(macroTables_, MacroTableFile);
            macroTableObject_.reset(newMacroTable(macroTables_));
            refreshEngine();
        } else if (path == "app_rules") {
            appRulesTables_.load(config, true);
            {
                std::lock_guard<std::mutex> lock(appRulesMutex_);
                for (auto it = appRules_.begin(); it != appRules_.end();) {
                    if (!isStartsWith(it->first, "ctx_")) {
                        it = appRules_.erase(it);
                    } else {
                        ++it;
                    }
                }
                for (const auto& rule : *appRulesTables_.rules) {
                    appRules_[*rule.app] = intToMode(*rule.mode);
                }
            }
            saveAppRules();
            refreshEngine();
        }
    }

    std::string LotusEngine::subMode(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        return *config_.inputMethod;
    }

    void LotusEngine::activate(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
        auto*                    ic        = event.inputContext();
        const bool               surrvalid = ic->surroundingText().isValid();
        const bool               is_dbus   = getFrontendName(ic) == "dbus";
        static std::atomic<bool> mouseThreadStarted{false};
        if (!mouseThreadStarted.exchange(true))
            startMouseReset();

        auto& statusArea = event.inputContext()->statusArea();
        if (ic->capabilityFlags().test(CapabilityFlag::Preedit))
            instance_->inputContextManager().setPreeditEnabledByDefault(true);

        std::string appName = getProgramName(ic);
        LOTUS_INFO("App name: " + appName);

        const LotusMode targetMode = getAppRule(appName);
        LOTUS_INFO("Target mode: " + LotusModeI18NAnnotation::toString(targetMode));

        updateCharsetAction(event.inputContext());

        setMode(targetMode, event.inputContext());

        auto* state = ic->propertyFor(&factory_);

        // Workaround for chromium wayland issue where suggestions cause a doubled
        // first character. Forwarding may prevent BS from being sent
        // to the client.
        //
        // Note that with chromium x11 we can't do anything to fixes this because
        // it not support surrounding text so can't know when it show suggestions
        //
        // TODO: Properly fixes instead ugly WA
        state->wa_chromium_flag = false;

        state->waitAck_ = false;
        if (*config_.fixUinputWithAck) {
            if (targetMode == LotusMode::Uinput || targetMode == LotusMode::Smooth || targetMode == LotusMode::Minecraft || targetMode == LotusMode::SuperSmooth || targetMode == LotusMode::UinputBackspace) {
#if __cplusplus >= 202002L
                std::ranges::transform(appName, appName.begin(), ::tolower);
#else
                std::transform(appName.begin(), appName.end(), appName.begin(), ::tolower);
#endif
                for (const auto& ackApp : ack_apps) {
                    if (appName.find(ackApp) != std::string::npos) {
                        if (is_dbus) {
                            state->waitAck_ = true;
                            LOTUS_INFO(ackApp + " detected, waiting for ack");
                        }
                        state->wa_chromium_flag = true;
                        break;
                    }
                }
            }
        }
        if (event.type() == EventType::InputContextFocusIn && is_dbus && !surrvalid) {
            LOTUS_INFO("Skip clearAllBuffers");
        } else if (surrvalid && !state->oldPreBuffer_.empty() && (now_ms() - state->lastDeactivateTime_) >= 100) {
            state->clearAllBuffers();
        }
        is_deleting_.store(false);
        needEngineReset.store(false);
        if (targetMode == LotusMode::Emoji) {
            state->updateEmojiPreedit();
        } else {
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            if (realMode == LotusMode::Preedit || realMode == LotusMode::SurroundingText)
                ic->updatePreedit();
        }
        for (const auto& action : toggleActions_) {
            statusArea.addAction(StatusGroup::InputMethod, action);
        }
    }

    void LotusEngine::keyEvent(const InputMethodEntry& /*entry*/, KeyEvent& keyEvent) {
        auto* ic = keyEvent.inputContext();

        if (isSelectingAppMode_ && g_mouse_clicked.load(std::memory_order_acquire)) {
            closeAppModeMenu();
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            auto* state = ic->propertyFor(&factory_);
            state->commitBuffer();
            state->reset();
        }

        if (isSelectingAppMode_) {
            if (keyEvent.isRelease())
                return;

            auto   baseList = ic->inputPanel().candidateList();
            auto   menuList = std::dynamic_pointer_cast<CommonCandidateList>(baseList);
            KeySym keySym   = keyEvent.key().sym();

            auto   moveCursor = [&](int delta) {
                if (!menuList || menuList->empty()) {
                    return false;
                }

                int totalSize = menuList->totalSize();
                if (totalSize <= 1) {
                    return false;
                }

                int cursorIndex = menuList->globalCursorIndex();
                if (cursorIndex < 0 || cursorIndex >= totalSize) {
                    cursorIndex = 0;
                }

                int nextIndex = cursorIndex + delta;
                if (nextIndex < 0) {
                    nextIndex = totalSize - 1;
                } else if (nextIndex >= totalSize) {
                    nextIndex = 0;
                }

                menuList->setGlobalCursorIndex(nextIndex);
                ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                return true;
            };

            keyEvent.filterAndAccept();

            std::optional<LotusMode> selectedMode  = std::nullopt;
            bool                     selectionMade = false;

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
                        if (selectedIndex < 0 || selectedIndex >= menuList->totalSize()) {
                            selectedIndex = 0;
                        }
                        menuList->candidateFromAll(selectedIndex).select(ic);
                        return;
                    }
                    break;
                }
                case FcitxKey_Escape: {
                    selectionMade = true;
                    break;
                }
                default: {
                    auto it = modeMenuMapping_.find(keySym);
                    if (it != modeMenuMapping_.end()) {
                        selectedMode = it->second;
                    }

                    if (selectedMode == std::nullopt) {
                        const auto& kl = *config_.modeMenuKey;
                        if (kl.size() == 1 && !kl[0].hasModifier()) {
                            std::string charStr = Key::keySymToUTF8(kl[0].sym());
                            if (!charStr.empty()) {
                                if (keySym == typeKeyForModeMenuHotkey(kl[0].sym(), config_)) {
                                    isSelectingAppMode_ = false;
                                    ic->inputPanel().reset();
                                    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                                    auto* state = ic->propertyFor(&factory_);
                                    state->commitBuffer();
                                    state->reset();
                                    ic->commitString(charStr);
                                    return;
                                }
                            }
                        }
                    }
                    break;
                }
            }

            if (selectedMode != std::nullopt) {
                LOTUS_INFO("Selected mode: " + LotusModeI18NAnnotation::toString(selectedMode.value()));
                if (selectedMode != LotusMode::Emoji) {
                    if (keySym == Key(*config_.shortcutDefault).sym()) { // Default Typing key
                        clearAppRule(currentConfigureApp_);
                    } else {
                        setAppRule(currentConfigureApp_, selectedMode.value());
                        if (!isStartsWith(currentConfigureApp_, "ctx_")) {
                            saveAppRules();
                        }
                    }
                }
                selectionMade = true;
            }

            if (selectionMade) {
                isSelectingAppMode_ = false;
                ic->inputPanel().reset();
                ic->updateUserInterface(UserInterfaceComponent::InputPanel);
                auto* state = ic->propertyFor(&factory_);

                if (selectedMode != std::nullopt) {
                    state->commitBuffer();
                    state->reset();
                    setMode(selectedMode.value(), ic);
                    if (selectedMode == LotusMode::Emoji) {
                        state->updateEmojiPreedit();
                    }
                }
            }
            return;
        }

        if (!keyEvent.isRelease() && !config_.cycleModeKey->empty() && keyEvent.key().checkKeyList(*config_.cycleModeKey)) {
            LOTUS_INFO("Cycle mode key pressed");
            std::string                               appName  = getProgramName(ic);
            LotusMode                                 realMode = getAppRule(appName);

            auto                                      order      = stringutils::split(*config_.modeOrder, ",");
            std::vector<std::pair<std::string, bool>> visibility = {{"Smooth", *config_.showModeSmooth},
                                                                    {"Uinput", *config_.showModeUinput},
                                                                    {"Minecraft", *config_.showModeMinecraft},
                                                                    {"SurroundingText", *config_.showModeSurroundingText},
                                                                    {"Preedit", *config_.showModePreedit},
                                                                    {"Emoji", *config_.showModeEmoji},
                                                                    {"Off", *config_.showModeOff},
                                                                    {"SuperSmooth", *config_.showModeSuperSmooth},
                                                                    {"UinputBackspace", *config_.showModeUinputBackspace},
                                                                    {"Default", *config_.showModeDefault}};

            std::vector<LotusMode>                    enabledModes;
            for (const auto& name : order) {
                bool visible = false;
                for (const auto& v : visibility) {
                    if (v.first == name) {
                        visible = v.second;
                        break;
                    }
                }
                if (visible) {
                    std::optional<LotusMode> mode = std::nullopt;
                    if (name == "Smooth")
                        mode = LotusMode::Smooth;
                    else if (name == "Uinput")
                        mode = LotusMode::Uinput;
                    else if (name == "Minecraft")
                        mode = LotusMode::Minecraft;
                    else if (name == "SurroundingText")
                        mode = LotusMode::SurroundingText;
                    else if (name == "Preedit")
                        mode = LotusMode::Preedit;
                    else if (name == "Emoji")
                        mode = LotusMode::Emoji;
                    else if (name == "Off")
                        mode = LotusMode::Off;
                    else if (name == "SuperSmooth")
                        mode = LotusMode::SuperSmooth;
                    else if (name == "UinputBackspace")
                        mode = LotusMode::UinputBackspace;
                    else if (name == "Default")
                        mode = config().mode.value();
                    else
                        continue;

                    bool duplicate = false;
                    for (auto m : enabledModes) {
                        if (m == mode) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        enabledModes.push_back(mode.value());
                    }
                }
            }

            if (!enabledModes.empty()) {
                size_t currentIdx = 0;
                bool   found      = false;
                for (size_t i = 0; i < enabledModes.size(); ++i) {
                    if (enabledModes[i] == realMode) {
                        currentIdx = i;
                        found      = true;
                        break;
                    }
                }

                LotusMode nextMode = found ? enabledModes[(currentIdx + 1) % enabledModes.size()] : enabledModes[0];
                setMode(nextMode, ic);
                setAppRule(appName, nextMode);
                showCycleModeNotification(nextMode, ic);
            }

            keyEvent.filterAndAccept();
            return;
        }

        if (!keyEvent.isRelease() && !config_.modeMenuKey->empty() && keyEvent.key().checkKeyList(*config_.modeMenuKey)) {
            LOTUS_INFO("Mode menu key pressed");
            currentConfigureApp_ = getProgramName(ic);
            g_mouse_clicked.store(false, std::memory_order_release);
            std::string appName = getProgramName(ic);
            setMode(getAppRule(appName), ic);
            showAppModeMenu(ic);
            keyEvent.filterAndAccept();
            return;
        }
        auto* state = keyEvent.inputContext()->propertyFor(&factory_);
        state->keyEvent(keyEvent);
        const auto&  s       = ic->surroundingText();
        const auto&  text    = s.text();
        size_t       textLen = fcitx_utf8_strlen(text.c_str());
        unsigned int cursor  = s.cursor();
        if (textLen == static_cast<size_t>(cursor))
            realtextLen.store(static_cast<unsigned int>(textLen), std::memory_order_release);
    }

    void LotusEngine::reset(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
        // Chặn reset ngoài ý muốn: Khi vừa gửi Backspace qua uinput hoặc trong cửa sổ 50ms sau commit,
        // các app/compositor thường phát sự kiện reset về Fcitx5. Nếu reset lúc này sẽ xóa trắng Bamboo Engine
        // và làm mất ngữ cảnh đang ghép từ tiếng Việt.
        if (shouldRejectReset()) {
            LOTUS_INFO("app reset rejected (active deletion or post-commit window)");
            return;
        }
        LOTUS_INFO("Reset engine");
        auto* state = event.inputContext()->propertyFor(&factory_);
        if (!state->isEmptyHistory() && event.type() != EventType::InputContextFocusOut) {
            return;
        }

        if (event.type() == EventType::InputContextFocusOut || event.type() == EventType::InputContextReset) {
            state->reset(event.type() == EventType::InputContextFocusOut);
        }
    }

    void LotusEngine::deactivate(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
        auto*      ic              = event.inputContext();
        auto*      state           = ic->propertyFor(&factory_);
        const bool surrvalid       = ic->surroundingText().isValid();
        const bool is_dbus         = getFrontendName(ic) == "dbus";
        state->lastDeactivateTime_ = now_ms();
        if (realMode == LotusMode::Preedit && event.type() != EventType::InputContextFocusOut) {
            state->commitBuffer();
        } else {
            if (event.type() == EventType::InputContextFocusOut && is_dbus && !surrvalid) {
                state->lastDeactivateTime_ = now_ms();
                LOTUS_INFO("Skip clearAllBuffers");
            } else {
                // Không xóa buffer nếu đang bận gửi uinput backspace để tránh xóa mất từ đang thay thế
                if (surrvalid && !state->oldPreBuffer_.empty() && !is_deleting_.load(std::memory_order_acquire))
                    state->clearAllBuffers();
            }
            // Không xóa cờ needEngineReset nếu đang trong quá trình xóa uinput
            if (!is_deleting_.load(std::memory_order_acquire)) {
                needEngineReset.store(false);
            }
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            if (realMode == LotusMode::Preedit || realMode == LotusMode::Emoji || realMode == LotusMode::SurroundingText)
                ic->updatePreedit();
        }
    }

    void LotusEngine::refreshEngine() {
        if (!factory_.registered())
            return;
        instance_->inputContextManager().foreach ([this](InputContext* ic) {
            auto* state = ic->propertyFor(&factory_);
            state->setEngine();
            if (ic->hasFocus())
                state->reset();
            return true;
        });
    }

    void LotusEngine::refreshOption() {
        if (!factory_.registered())
            return;
        instance_->inputContextManager().foreach ([this](InputContext* ic) {
            auto* state = ic->propertyFor(&factory_);
            state->setOption();
            if (ic->hasFocus())
                state->reset();
            return true;
        });
    }

    void LotusEngine::updateCharsetAction(InputContext* ic) {
        auto name = stringutils::concat(CharsetActionPrefix, *config_.outputCharset);
        for (const auto& action : charsetSubAction_) {
            action->setChecked(action->name() == name);
            if (ic != nullptr)
                action->update(ic);
        }
    }

    void LotusEngine::loadAppRules() {
        {
            std::lock_guard<std::mutex>                lock(appRulesMutex_);
            std::unordered_map<std::string, LotusMode> ctxRules;
            for (const auto& [app, mode] : appRules_) {
                if (isStartsWith(app, "ctx_")) {
                    ctxRules[app] = mode;
                }
            }
            appRules_ = std::move(ctxRules);
        }
        auto loadFromFile = [this](const std::string& path) {
            if (path.empty()) {
                LOTUS_WARN("App rules path is empty, skipping load");
                return;
            }
            std::ifstream file(path);
            if (!file.is_open())
                return;

            std::unordered_map<std::string, LotusMode> tempRules;
            std::string                                line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#')
                    continue;
                auto delimiterPos = line.find('=');
                if (delimiterPos != std::string::npos) {
                    std::string app  = line.substr(0, delimiterPos);
                    std::string mode = line.substr(delimiterPos + 1);
                    try {
                        tempRules[app] = intToMode(std::stoi(mode));
                    } catch (const std::exception&) { LOTUS_WARN("Invalid mode value for app: " + app); }
                }
            }
            file.close();

            std::lock_guard<std::mutex> lock(appRulesMutex_);
            for (const auto& [app, mode] : tempRules) {
                appRules_[app] = mode;
            }
        };
        loadFromFile(appRulesPath_);

        std::lock_guard<std::mutex> lock(appRulesMutex_);
        std::vector<lotusAppRule>   rules;
        for (const auto& pair : appRules_) {
            if (pair.first.find("ctx_") == 0)
                continue;
            lotusAppRule rule;
            rule.app.setValue(pair.first);
            rule.mode.setValue(modeToInt(pair.second));
            rules.push_back(std::move(rule));
        }
        appRulesTables_.rules.setValue(std::move(rules));
    }

    void LotusEngine::saveAppRules() const {
        // Method is const but locks mutable appRulesMutex_ to safely read appRules_ state
        std::ofstream file(appRulesPath_, std::ios::trunc);
        if (!file.is_open())
            return;

        file << "# Lotus Per-App Configuration\n";
        file << "# 0 = Off, 1 = Uinput (Smooth), 2 = Uinput (Slow), 3 = Uinput (Super Smooth), 4 = Surrounding Text, 5 = Preedit, 6 = Emoji Picker, 8 = Minecraft\n";
        std::lock_guard<std::mutex> lock(appRulesMutex_);
        for (const auto& pair : appRules_) {
            bool currentIsCtx = isStartsWith(pair.first, "ctx_");
            if (!currentIsCtx) {
                file << pair.first << "=" << modeToInt(pair.second) << "\n";
            }
        }
        file.close();
    }

    LotusMode LotusEngine::getAppRule(const std::string& appName) const {
        std::lock_guard<std::mutex> lock(appRulesMutex_);
        auto                        it = appRules_.find(appName);
        if (it != appRules_.end()) {
            return it->second;
        }
        return config_.mode.value();
    }

    void LotusEngine::setAppRule(const std::string& appName, LotusMode mode) {
        auto rules = *appRulesTables_.rules;

        bool found = false;
        for (auto& rule : rules) {
            if (*rule.app == appName) {
                rule.mode.setValue(modeToInt(mode));
                found = true;
                break;
            }
        }

        if (!found) {
            lotusAppRule newRule;
            newRule.app.setValue(appName);
            newRule.mode.setValue(modeToInt(mode));
            rules.push_back(std::move(newRule));
        }

        {
            std::lock_guard<std::mutex> lock(appRulesMutex_);
            appRules_[appName] = mode;
        }
        appRulesTables_.rules.setValue(std::move(rules));
    }

    void LotusEngine::closeAppModeMenu() {
        isSelectingAppMode_ = false;
        g_mouse_clicked.store(false, std::memory_order_release);
    }

    void LotusEngine::showAppModeMenu(InputContext* ic) {
        isSelectingAppMode_ = true;

        auto candidateList = std::make_unique<CommonCandidateList>();

        candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
        candidateList->setPageSize(10);

        auto getLabel = [&](const LotusMode& modeName, const std::string& modeLabel) {
            if (modeName == realMode) {
                return Text(">> " + modeLabel);
            }
            return Text("   " + modeLabel);
        };

        auto cleanup = [this](InputContext* ic) {
            isSelectingAppMode_ = false;
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            auto* state = ic->propertyFor(&factory_);
            state->commitBuffer();
            state->reset();
        };

        auto applyMode = [this, cleanup](LotusMode mode, bool isDefault = false) {
            return [this, mode, cleanup, isDefault](InputContext* ic) {
                if (mode != LotusMode::Emoji) {
                    if (isDefault) {
                        clearAppRule(currentConfigureApp_);
                    } else {
                        setAppRule(currentConfigureApp_, mode);
                        if (!isStartsWith(currentConfigureApp_, "ctx_")) {
                            saveAppRules();
                        }
                    }
                }

                cleanup(ic);
                setMode(mode, ic);
                if (mode == LotusMode::Emoji) {
                    auto* state = ic->propertyFor(&factory_);
                    state->updateEmojiPreedit();
                }
            };
        };

        struct ModeInfo {
            LotusMode   mode;
            std::string label;
            KeySym      key;
            bool        visible;
        };

        auto                                      getShortcut = [](const std::string& shortcut) { return Key(shortcut).sym(); };

        std::unordered_map<std::string, ModeInfo> modeMap = {
            {"Smooth", {LotusMode::Smooth, _("Uinput (Smooth)"), getShortcut(*config_.shortcutSmooth), *config_.showModeSmooth}},
            {"Uinput", {LotusMode::Uinput, _("Uinput (Slow)"), getShortcut(*config_.shortcutUinput), *config_.showModeUinput}},
            {"Minecraft", {LotusMode::Minecraft, _("Minecraft"), getShortcut(*config_.shortcutMinecraft), *config_.showModeMinecraft}},
            {"SurroundingText", {LotusMode::SurroundingText, _("Surrounding Text"), getShortcut(*config_.shortcutSurroundingText), *config_.showModeSurroundingText}},
            {"Preedit", {LotusMode::Preedit, _("Preedit"), getShortcut(*config_.shortcutPreedit), *config_.showModePreedit}},
            {"Emoji", {LotusMode::Emoji, _("Emoji Picker"), getShortcut(*config_.shortcutEmoji), *config_.showModeEmoji}},
            {"Off", {LotusMode::Off, _("OFF"), getShortcut(*config_.shortcutOff), *config_.showModeOff}},
            {"SuperSmooth", {LotusMode::SuperSmooth, _("Uinput (Super Smooth)"), getShortcut(*config_.shortcutSuperSmooth), *config_.showModeSuperSmooth}},
            {"UinputBackspace", {LotusMode::UinputBackspace, _("Uinput (Backspace)"), getShortcut(*config_.shortcutUinputBackspace), *config_.showModeUinputBackspace}},
            {"Default", {config_.mode.value(), _("Default Typing"), getShortcut(*config_.shortcutDefault), *config_.showModeDefault}}};

        std::vector<ModeInfo> allModes;
        auto                  order = stringutils::split(*config_.modeOrder, ",");
        for (const auto& name : order) {
            auto it = modeMap.find(name);
            if (it != modeMap.end()) {
                allModes.push_back(it->second);
            }
        }

        // Fallback for missing modes
        for (const auto& [name, info] : modeMap) {
            bool found = false;
            for (const auto& orderedName : order) {
                if (orderedName == name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                allModes.push_back(info);
            }
        }

        int                        activeSelectionIdx  = -1;
        int                        currentCandidateIdx = 0;
        std::unordered_set<KeySym> usedModeKeys;

        modeMenuMapping_.clear();
        const LotusMode defaultMode = config_.mode.value();

        for (const auto& info : allModes) {
            if (info.visible) {
                const bool hasShortcut = info.key != FcitxKey_None && info.key != FcitxKey_VoidSymbol;
                if (hasShortcut && usedModeKeys.insert(info.key).second) {
                    modeMenuMapping_[info.key] = info.mode;
                }

                const bool        isDefaultItem = (info.label == _("Default Typing"));
                const std::string keyUtf8       = Key::keySymToUTF8(info.key);
                std::string       keyLabel      = keyUtf8.empty() ? "" : "[" + keyUtf8 + "] ";
                candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(info.mode, keyLabel + info.label), applyMode(info.mode, isDefaultItem)));

                if (info.mode == realMode && !isDefaultItem) {
                    activeSelectionIdx = currentCandidateIdx;
                } else if (isDefaultItem && getAppRule(currentConfigureApp_) == defaultMode) {
#if __cplusplus >= 202002L
                    if (!appRules_.contains(currentConfigureApp_)) {
#else
                    if (appRules_.find(currentConfigureApp_) == appRules_.end()) {
#endif
                        activeSelectionIdx = currentCandidateIdx;
                    }
                }
                currentCandidateIdx++;
            }
        }

        {
            const auto& kl = *config_.modeMenuKey;
            if (kl.size() == 1 && !kl[0].hasModifier()) {
                std::string charStr = Key::keySymToUTF8(kl[0].sym());
                if (!charStr.empty()) {
                    KeySym      typeKeySym   = typeKeyForModeMenuHotkey(kl[0].sym(), config_);
                    std::string typeKeyLabel = Key::keySymToUTF8(typeKeySym);
                    std::string label        = "[" + typeKeyLabel + "] " + _("Type") + " " + charStr;
                    candidateList->append(std::make_unique<AppModeCandidateWord>(Text(label), [cleanup, charStr](InputContext* ic) {
                        cleanup(ic);
                        ic->commitString(charStr);
                    }));
                }
            }
        }

        if (activeSelectionIdx != -1) {
            candidateList->setGlobalCursorIndex(activeSelectionIdx);
        } else if (candidateList->totalSize() > 0) {
            candidateList->setGlobalCursorIndex(0);
        }

        ic->inputPanel().reset();
        ic->inputPanel().setCandidateList(std::move(candidateList));
        ic->inputPanel().setAuxDown(Text(_("App: ") + currentConfigureApp_));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void LotusEngine::showCycleModeNotification(LotusMode mode, InputContext* ic) {
        auto candidateList = std::make_unique<CommonCandidateList>();
        candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
        candidateList->setPageSize(1);

        auto cleanup = [](InputContext* ic) {
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        };

        // Map mode to label
        std::string modeLabel;
        switch (mode) {
            case LotusMode::Smooth: modeLabel = _("Uinput (Smooth)"); break;
            case LotusMode::Uinput: modeLabel = _("Uinput (Slow)"); break;
            case LotusMode::Minecraft: modeLabel = _("Minecraft"); break;
            case LotusMode::SurroundingText: modeLabel = _("Surrounding Text"); break;
            case LotusMode::Preedit: modeLabel = _("Preedit"); break;
            case LotusMode::Emoji: modeLabel = _("Emoji Picker"); break;
            case LotusMode::Off: modeLabel = _("OFF"); break;
            case LotusMode::SuperSmooth: modeLabel = _("Uinput (Super Smooth)"); break;
            case LotusMode::UinputBackspace: modeLabel = _("Uinput (Backspace)"); break;
            default: modeLabel = _("Unknown Mode"); break;
        }

        auto setCurrentMode = [cleanup](LotusMode) { return [cleanup](InputContext* ic) { cleanup(ic); }; };

        candidateList->append(std::make_unique<AppModeCandidateWord>(Text("✓ " + modeLabel), setCurrentMode(mode)));

        candidateList->setGlobalCursorIndex(0);

        ic->inputPanel().reset();
        ic->inputPanel().setCandidateList(std::move(candidateList));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);

        // Cancel previous timer if any
        cycleModeNotificationTimer_.reset();

        // Schedule auto-close using EventLoop::addTimeEvent
        auto& eventLoop    = instance_->eventLoop();
        auto  now_time     = ::fcitx::now(CLOCK_MONOTONIC);
        auto  timeout_time = now_time + CYCLE_MODE_NOTIFICATION_TIMEOUT_USEC;

        cycleModeNotificationTimer_ = eventLoop.addTimeEvent(CLOCK_MONOTONIC, timeout_time, 0, [icRef = ic->watch()](EventSourceTime*, uint64_t) {
            if (auto* ic = icRef.get(); ic && ic->hasFocus()) {
                ic->inputPanel().reset();
                ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            }
            return false;
        });
    }

    void LotusEngine::setMode(LotusMode mode, InputContext* ic) {
        realMode = mode;
        if (ic != nullptr) {
            if (auto* state = ic->propertyFor(&factory_)) {
                state->clearAllBuffers();
            }
            ic->updateUserInterface(UserInterfaceComponent::StatusArea);
        }
    }

    std::string LotusEngine::subModeIconImpl(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        std::string baseIconName;
        switch (realMode) {
            case LotusMode::Off: baseIconName = "fcitx-lotus-off"; break;
            case LotusMode::Emoji: baseIconName = "fcitx-lotus-emoji"; break;
            default: baseIconName = "fcitx-lotus"; break;
        }

        std::string iconName;
        if (*config_.useLotusIcons) {
            iconName = baseIconName;
        } else {
            const auto& iconTheme = config_.iconTheme.value();
            if (iconTheme == IconTheme::Light) {
                iconName = baseIconName + "-default-black";
            } else if (iconTheme == IconTheme::Dark) {
                iconName = baseIconName + "-default";
            } else {
                iconName = baseIconName + (isDarkMode() ? "-default" : "-default-black");
            }
        }

        // ── Cinnamon: return icon NAME (not absolute path) ──────────────────
        // Cinnamon's tray uses XApp Status Applet (SNI).  The IconName property
        // is sent over D-Bus and resolved via Gtk.IconTheme — which only
        // understands theme icon names, not filesystem paths.
        //
        // On KDE and GNOME, absolute paths work correctly — their compositors
        // or SNI hosts handle filesystem paths in IconName.
        static const bool kIsCinnamon = [] {
            const char* de = std::getenv("XDG_CURRENT_DESKTOP");
            if (!de)
                de = std::getenv("DESKTOP_SESSION");
            return de && (std::string(de) == "cinnamon" || std::string(de) == "X-Cinnamon");
        }();

        if (kIsCinnamon) {
            return iconName;
        }

        // Cache keyed on the resolved icon name — mode/theme changes
        // re-resolve automatically, no manual invalidation needed.
        if (iconCacheName_ == iconName && !iconCachePath_.empty()) {
            return iconCachePath_;
        }
        iconCacheName_ = iconName;

        // ── Default: resolve to absolute path (KDE, GNOME, etc.) ───────────
        // Return absolute path to bypass XDG icon theme lookup, which fails on
        // many non-Breeze icon themes despite the icon being installed in
        // hicolor and breeze fallback directories.
        LotusIconSearchPaths paths;
        // hicolor status/apps dirs; SVG preferred, PNG only as raster fallback.
        paths.systemDirs = {
            "/usr/share/icons/hicolor/22x22/status",  "/usr/share/icons/hicolor/24x24/status", "/usr/share/icons/hicolor/scalable/status",
            "/usr/share/icons/hicolor/scalable/apps", "/usr/share/icons/hicolor/48x48/apps",
        };
        paths.fallbackDir = FCITX_LOTUS_ICON_DIR; // compile-time install dir

        iconCachePath_ = resolveLotusIconPath({iconName, baseIconName}, paths);
        return iconCachePath_;
    }

    std::string LotusEngine::subModeLabelImpl(const InputMethodEntry& /*entry*/, InputContext& /*inputContext*/) {
        switch (realMode) {
            case LotusMode::Off: return _("Lotus - Off");
            case LotusMode::Emoji: return "😄";
            default: return isGnome_ ? "vi" : "🪷";
        }
    }

    std::string LotusEngine::getProgramName(InputContext* ic) {
        if (ic == nullptr) {
            return "unknown-app";
        }
        std::string programName = ic->program();
        if (programName.empty() || programName == "wayland" || programName == "x11") {
            // Fallback: InputContext address-based resolution
            // This ensures at least per-window separation.
            std::ostringstream oss;
            oss << "ctx_" << static_cast<const void*>(ic);
            programName = oss.str();
        }
        return programName;
    }

    void LotusEngine::clearAppRule(const std::string& appName) {
        {
            std::lock_guard<std::mutex> lock(appRulesMutex_);
            appRules_.erase(appName);
        }
        auto rules = *appRulesTables_.rules;
        rules.erase(std::remove_if(rules.begin(), rules.end(), [&appName](const auto& rule) { return *rule.app == appName; }), rules.end());
        appRulesTables_.rules.setValue(std::move(rules));
        if (!isStartsWith(appName, "ctx_")) {
            saveAppRules();
        }
    }
} // namespace fcitx
