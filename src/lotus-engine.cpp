/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2025 Võ Ngô Hoàng Thành <thanhpy2009@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "lotus.h"
#include "lotus-engine.h"
#include "lotus-state.h"
#include "lotus-candidates.h"
#include "lotus-monitor.h"
#include "lotus-utils.h"
#include "ack-apps.h"

#include <fcntl.h>

#include <cstdlib>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/standardpath.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/menu.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterface.h>
#include <fcitx/userinterfacemanager.h>

#include <filesystem>
#include <fstream>

namespace fcitx {
    constexpr const char*     InputMethodActionPrefix = "lotus-input-method-";
    constexpr const char*     CharsetActionPrefix     = "lotus-charset-";
    constexpr const char*     MacroPrefix             = "macro/";
    const std::string         CustomKeymapFile        = "conf/lotus-custom-keymap.conf";

    static inline std::string macroFile(const std::string& imName) {
        return stringutils::concat("conf/lotus-macro-", imName, ".conf");
    }

    static inline uintptr_t newMacroTable(const lotusMacroTable& macroTable) {
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

    static inline std::vector<std::string> convertToStringList(char** list) {
        std::vector<std::string> result;
        if (list) {
            for (size_t i = 0; list[i]; ++i) {
                result.emplace_back(list[i]);
                free(list[i]);
            }
            free(list);
        }
        return result;
    }

    uintptr_t LotusEngine::macroTable() const {
        if (config_.inputMethod.value().empty()) {
            return 0;
        }
        auto it = macroTableObject_.find(*config_.inputMethod);
        if (it != macroTableObject_.end()) {
            return it->second.handle();
        }
        return 0;
    }

    LotusEngine::LotusEngine(Instance* instance) : instance_(instance), factory_([this](InputContext& ic) { return new LotusState(this, &ic); }) {
        startMonitoringOnce();
        Init();
        {
            auto imNames = convertToStringList(GetInputMethodNames());
            imNames.push_back("Custom");
            imNames_ = std::move(imNames);
        }
        config_.inputMethod.annotation().setList(imNames_);
#if LOTUS_USE_MODERN_FCITX_API
        auto fd = StandardPaths::global().open(StandardPathsType::PkgData, "lotus/vietnamese.cm.dict");
#else
        auto fd = StandardPath::global().open(StandardPath::Type::PkgData, "lotus/vietnamese.cm.dict", O_RDONLY);
#endif
        if (!fd.isValid())
            throw std::runtime_error("Failed to load dictionary");
        dictionary_.reset(NewDictionary(fd.release()));

        auto& uiManager = instance_->userInterfaceManager();
        modeAction_     = std::make_unique<SimpleAction>();
        modeAction_->setIcon("preferences-system");
        modeAction_->setShortText(_("Typing Mode"));
        uiManager.registerAction("lotus-mode", modeAction_.get());
        modeMenu_ = std::make_unique<Menu>();
        modeAction_->setMenu(modeMenu_.get());

        std::vector<LotusMode> modes = {LotusMode::Smooth, LotusMode::Uinput, LotusMode::SurroundingText, LotusMode::Preedit, LotusMode::UinputHC, LotusMode::Off};
        for (const auto& mode : modes) {
            auto action = std::make_unique<SimpleAction>();
            action->setShortText(modeEnumToString(mode));
            action->setCheckable(true);
            uiManager.registerAction("lotus-mode-" + modeEnumToString(mode), action.get());
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
        uiManager.registerAction("lotus-input-method", inputMethodAction_.get());
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
        uiManager.registerAction("lotus-charset", charsetAction_.get());
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
        uiManager.registerAction("lotus-spellcheck", spellCheckAction_.get());

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
        uiManager.registerAction("lotus-macro", macroAction_.get());

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
        uiManager.registerAction("lotus-capitalizemacro", capitalizeMacroAction_.get());

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
        uiManager.registerAction("lotus-autonvnrestore", autoNonVnRestoreAction_.get());

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
        uiManager.registerAction("lotus-modernstyle", modernStyleAction_.get());

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
        uiManager.registerAction("lotus-freemarking", freeMarkingAction_.get());

        fixUinputWithAckAction_ = std::make_unique<SimpleAction>();
        fixUinputWithAckAction_->setLongText(_("Fix uinput mode with ack"));
        fixUinputWithAckAction_->setIcon("network-transmit-receive");
        fixUinputWithAckAction_->setCheckable(true);
        connections_.emplace_back(fixUinputWithAckAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.fixUinputWithAck.setValue(!*config_.fixUinputWithAck);
            saveConfig();
            refreshOption();
            updateFixUinputWithAckAction(ic);
        }));
        uiManager.registerAction("lotus-fixuinputwithack", fixUinputWithAckAction_.get());

        lotusIconsAction_ = std::make_unique<SimpleAction>();
        lotusIconsAction_->setLongText(_("Use Lotus status icons"));
        lotusIconsAction_->setIcon("emblem-default");
        lotusIconsAction_->setCheckable(true);
        connections_.emplace_back(lotusIconsAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.useLotusIcons.setValue(!*config_.useLotusIcons);
            saveConfig();
            refreshOption();
            updateLotusIconsAction(ic);
        }));
        uiManager.registerAction("lotus-icons", lotusIconsAction_.get());
        typingModeMenuAction_ = std::make_unique<SimpleAction>();
        typingModeMenuAction_->setLongText(_("Open typing mode menu"));
        typingModeMenuAction_->setIcon("input-keyboard");
        typingModeMenuAction_->setCheckable(true);
        connections_.emplace_back(typingModeMenuAction_->connect<SimpleAction::Activated>([this](InputContext* ic) {
            config_.modeMenu.setValue(!*config_.modeMenu);
            saveConfig();
            updateTypingModeMenuAction(ic);
        }));
        uiManager.registerAction("lotus-modemenu", typingModeMenuAction_.get());

        reloadConfig();
        globalMode_ = modeStringToEnum(config_.mode.value());
        updateModeAction(nullptr);
        instance_->inputContextManager().registerProperty("LotusState", &factory_);

#if LOTUS_USE_MODERN_FCITX_API
        std::string configDir = (StandardPaths::global().userDirectory(StandardPathsType::Config) / "fcitx5" / "conf").string();
#else
        std::string configDir = StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
#endif

        if (!std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
        }
        appRulesPath_ = configDir + "/lotus-app-rules.conf";
        loadAppRules();
    }

    LotusEngine::~LotusEngine() {
        stop_flag_monitor.store(true, std::memory_order_release);
        monitor_cv.notify_all();
        int fd = mouse_socket_fd.load(std::memory_order_acquire);
        if (fd >= 0) {
            close(fd);
        }
    }

    void LotusEngine::reloadConfig() {
        readAsIni(config_, "conf/lotus.conf");
        readAsIni(customKeymap_, CustomKeymapFile);
        for (const auto& imName : imNames_) {
            auto& table = macroTables_[imName];
            readAsIni(table, macroFile(imName));
            macroTableObject_[imName].reset(newMacroTable(table));
        }
        populateConfig();
    }

    const Configuration* LotusEngine::getSubConfig(const std::string& path) const {
        if (path == "custom_keymap")
            return &customKeymap_;
#if __cplusplus >= 202002L
        if (path.starts_with(MacroPrefix)) {
#else
        if (stringutils::startsWith(path, MacroPrefix)) {
#endif
            const auto imName = path.substr(strlen(MacroPrefix));
            if (auto iter = macroTables_.find(imName); iter != macroTables_.end())
                return &iter->second;
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
        updateModeAction(nullptr);
        updateInputMethodAction(nullptr);
        updateCharsetAction(nullptr);
        updateSpellAction(nullptr);
        updateMacroAction(nullptr);
        updateCapitalizeMacroAction(nullptr);
        updateAutoNonVnRestoreAction(nullptr);
        updateModernStyleAction(nullptr);
        updateFreeMarkingAction(nullptr);
        updateFixUinputWithAckAction(nullptr);
        updateLotusIconsAction(nullptr);
        updateTypingModeMenuAction(nullptr);
    }

    void LotusEngine::setSubConfig(const std::string& path, const RawConfig& config) {
        if (path == "custom_keymap") {
            customKeymap_.load(config, true);
            safeSaveAsIni(customKeymap_, CustomKeymapFile);
            refreshEngine();
#if __cplusplus >= 202002L
        } else if (path.starts_with(MacroPrefix)) {
#else
        } else if (stringutils::startsWith(path, MacroPrefix)) {
#endif
            const auto imName = path.substr(strlen(MacroPrefix));
            if (auto iter = macroTables_.find(imName); iter != macroTables_.end()) {
                iter->second.load(config, true);
                safeSaveAsIni(iter->second, macroFile(imName));
                macroTableObject_[imName].reset(newMacroTable(iter->second));
                refreshEngine();
            }
        }
    }

    std::string LotusEngine::subMode(const InputMethodEntry&, InputContext&) {
        return *config_.inputMethod;
    }

    void LotusEngine::activate(const InputMethodEntry& entry, InputContextEvent& event) {
        FCITX_UNUSED(entry);
        auto                     ic = event.inputContext();
        static std::atomic<bool> mouseThreadStarted{false};
        if (!mouseThreadStarted.exchange(true))
            startMouseReset();

        auto& statusArea = event.inputContext()->statusArea();
        if (ic->capabilityFlags().test(CapabilityFlag::Preedit))
            instance_->inputContextManager().setPreeditEnabledByDefault(true);

        std::string appName = ic->program();
        LotusMode   targetMode;

        if (!appRules_.empty() && appRules_.count(appName)) {
            targetMode = appRules_[appName];
        } else {
            targetMode = globalMode_;
        }
        reloadConfig();
        updateModeAction(event.inputContext());
        updateInputMethodAction(event.inputContext());
        updateCharsetAction(event.inputContext());

        setMode(targetMode, event.inputContext());

        auto state = ic->propertyFor(&factory_);

        state->waitAck_ = false;
        if (*config_.fixUinputWithAck) {
            if (targetMode == LotusMode::Uinput || targetMode == LotusMode::UinputHC || targetMode == LotusMode::Smooth) {
                for (const auto& ackApp : ack_apps) {
                    if (appName.find(ackApp) != std::string::npos) {
                        state->waitAck_ = true;
                        break;
                    }
                }
            }
        }

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
        statusArea.addAction(StatusGroup::InputMethod, fixUinputWithAckAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, lotusIconsAction_.get());
        statusArea.addAction(StatusGroup::InputMethod, typingModeMenuAction_.get());
    }

    void LotusEngine::keyEvent(const InputMethodEntry& entry, KeyEvent& keyEvent) {
        FCITX_UNUSED(entry);
        auto ic = keyEvent.inputContext();

        if (isSelectingAppMode_ && g_mouse_clicked.load(std::memory_order_relaxed)) {
            closeAppModeMenu();
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            auto state = ic->propertyFor(&factory_);
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
                if (cursorIndex < 1 || cursorIndex >= totalSize) {
                    cursorIndex = 1;
                }

                int nextIndex = cursorIndex + delta;
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

            LotusMode selectedMode  = LotusMode::NoMode;
            bool      selectionMade = false;

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
                case FcitxKey_1: {
                    selectedMode = LotusMode::Smooth;
                    break;
                }
                case FcitxKey_2: {
                    selectedMode = LotusMode::Uinput;
                    break;
                }
                case FcitxKey_3: {
                    selectedMode = LotusMode::UinputHC;
                    break;
                }
                case FcitxKey_4: {
                    selectedMode = LotusMode::SurroundingText;
                    break;
                }
                case FcitxKey_q: {
                    selectedMode = LotusMode::Preedit;
                    break;
                }
                case FcitxKey_w: {
                    selectedMode = LotusMode::Emoji;
                    break;
                }
                case FcitxKey_e: {
                    selectedMode = LotusMode::Off;
                    break;
                }
                case FcitxKey_r: {
                    if (appRules_.count(currentConfigureApp_)) {
                        appRules_.erase(currentConfigureApp_);
                        saveAppRules();
                    }
                    selectedMode  = globalMode_;
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

            if (selectedMode != LotusMode::NoMode) {
                if (selectedMode != LotusMode::Emoji) {
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

        if (!keyEvent.isRelease() && keyEvent.rawKey().check(FcitxKey_grave) && *config_.modeMenu) {
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

    void LotusEngine::reset(const InputMethodEntry& entry, InputContextEvent& event) {
        FCITX_UNUSED(entry);
        auto state = event.inputContext()->propertyFor(&factory_);
        if (!state->isEmptyHistory() && event.type() != EventType::InputContextFocusOut) {
            return;
        }

        if (event.type() == EventType::InputContextFocusOut) {
            state->reset();
        }
    }

    void LotusEngine::deactivate(const InputMethodEntry& entry, InputContextEvent& event) {
        FCITX_UNUSED(entry);
        auto state = event.inputContext()->propertyFor(&factory_);
        if (realMode == LotusMode::Preedit) {
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

    void LotusEngine::refreshEngine() {
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

    void LotusEngine::refreshOption() {
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

    void LotusEngine::updateModeAction(InputContext* ic) {
        std::string currentModeStr = config_.mode.value();
        LotusMode   newMode        = modeStringToEnum(currentModeStr);
        globalMode_                = newMode;
        setMode(newMode, ic);

        for (const auto& action : modeSubAction_) {
            action->setChecked(action->name() == "lotus-mode-" + currentModeStr);
            if (ic)
                action->update(ic);
        }
        modeAction_->setLongText(_("Typing Mode: ") + currentModeStr);

        if (ic) {
            modeAction_->update(ic);
        }
    }

    void LotusEngine::updateInputMethodAction(InputContext* ic) {
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

    void LotusEngine::updateCharsetAction(InputContext* ic) {
        auto name = stringutils::concat(CharsetActionPrefix, *config_.outputCharset);
        for (const auto& action : charsetSubAction_) {
            action->setChecked(action->name() == name);
            if (ic)
                action->update(ic);
        }
    }

    void LotusEngine::updateSpellAction(InputContext* ic) {
        spellCheckAction_->setChecked(*config_.spellCheck);
        spellCheckAction_->setShortText(*config_.spellCheck ? _("Spell Check: On") : _("Spell Check: Off"));
        if (ic) {
            spellCheckAction_->update(ic);
        }
    }

    void LotusEngine::updateMacroAction(InputContext* ic) {
        macroAction_->setChecked(*config_.macro);
        macroAction_->setShortText(*config_.macro ? _("Macro: On") : _("Macro: Off"));
        if (ic) {
            macroAction_->update(ic);
        }
    }

    void LotusEngine::updateCapitalizeMacroAction(InputContext* ic) {
        capitalizeMacroAction_->setChecked(*config_.capitalizeMacro);
        capitalizeMacroAction_->setShortText(*config_.capitalizeMacro ? _("Capitalize Macro: On") : _("Capitalize Macro: Off"));
        if (ic) {
            capitalizeMacroAction_->update(ic);
        }
    }

    void LotusEngine::updateAutoNonVnRestoreAction(InputContext* ic) {
        autoNonVnRestoreAction_->setChecked(*config_.autoNonVnRestore);
        autoNonVnRestoreAction_->setShortText(*config_.autoNonVnRestore ? _("Auto Non-VN Restore: On") : _("Auto Non-VN Restore: Off"));
        if (ic) {
            autoNonVnRestoreAction_->update(ic);
        }
    }

    void LotusEngine::updateModernStyleAction(InputContext* ic) {
        modernStyleAction_->setChecked(*config_.modernStyle);
        modernStyleAction_->setShortText(*config_.modernStyle ? _("Modern Style: On") : _("Modern Style: Off"));
        if (ic) {
            modernStyleAction_->update(ic);
        }
    }

    void LotusEngine::updateFreeMarkingAction(InputContext* ic) {
        freeMarkingAction_->setChecked(*config_.freeMarking);
        freeMarkingAction_->setShortText(*config_.freeMarking ? _("Free Marking: On") : _("Free Marking: Off"));
        if (ic) {
            freeMarkingAction_->update(ic);
        }
    }

    void LotusEngine::updateFixUinputWithAckAction(InputContext* ic) {
        fixUinputWithAckAction_->setChecked(*config_.fixUinputWithAck);
        fixUinputWithAckAction_->setShortText(*config_.fixUinputWithAck ? _("Fix Uinput With Ack: On") : _("Fix Uinput With Ack: Off"));
        if (ic) {
            fixUinputWithAckAction_->update(ic);
        }
    }

    void LotusEngine::updateLotusIconsAction(InputContext* ic) {
        lotusIconsAction_->setChecked(*config_.useLotusIcons);
        lotusIconsAction_->setShortText(*config_.useLotusIcons ? _("Lotus Icons: On") : _("Lotus Icons: Off"));
        if (ic) {
            lotusIconsAction_->update(ic);
        }
    }

    void LotusEngine::updateTypingModeMenuAction(InputContext* ic) {
        typingModeMenuAction_->setChecked(*config_.modeMenu);
        typingModeMenuAction_->setShortText(*config_.modeMenu ? _("Typing Mode Menu: On") : _("Typing Mode Menu: Off"));
        if (ic) {
            typingModeMenuAction_->update(ic);
        }
    }

    void LotusEngine::loadAppRules() {
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
                std::string app  = line.substr(0, delimiterPos);
                std::string mode = line.substr(delimiterPos + 1);
                appRules_[app]   = static_cast<LotusMode>(std::stoi(mode));
            }
        }
        file.close();
    }

    void LotusEngine::saveAppRules() {
        std::ofstream file(appRulesPath_, std::ios::trunc);
        if (!file.is_open())
            return;

        file << "# Lotus Per-App Configuration\n";
        file << "# 0 = Off, 1 = Uinput (Smooth), 2 = Uinput (Slow), 3 = Uinput (Hardcore), 4 = Surrounding Text, 5 = Preedit, 6 = Emoji Picker\n";
        for (const auto& pair : appRules_) {
            file << pair.first << "=" << static_cast<int>(pair.second) << "\n";
        }
        file.close();
    }

    void LotusEngine::closeAppModeMenu() {
        isSelectingAppMode_ = false;
        g_mouse_clicked.store(false, std::memory_order_relaxed);
    }

    void LotusEngine::showAppModeMenu(InputContext* ic) {
        isSelectingAppMode_ = true;

        auto candidateList = std::make_unique<CommonCandidateList>();

        candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
        candidateList->setPageSize(10);

        auto getLabel = [&](const LotusMode& modeName, const std::string& modeLabel) {
            if (modeName == realMode) {
                return Text(">> " + modeLabel);
            } else {
                return Text("   " + modeLabel);
            }
        };

        auto cleanup = [this](InputContext* ic) {
            isSelectingAppMode_ = false;
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            auto state = ic->propertyFor(&factory_);
            state->reset();
        };

        auto applyMode = [this, cleanup](LotusMode mode) {
            return [this, mode, cleanup](InputContext* ic) {
                if (mode != LotusMode::Emoji) {
                    appRules_[currentConfigureApp_] = mode;
                    saveAppRules();
                }

                setMode(mode, ic);
                cleanup(ic);
            };
        };

        candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(Text(_("App: ") + currentConfigureApp_)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(LotusMode::Smooth, _("[1] Uinput (Smooth)")), applyMode(LotusMode::Smooth)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(LotusMode::Uinput, _("[2] Uinput (Slow)")), applyMode(LotusMode::Uinput)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(LotusMode::UinputHC, _("[3] Uinput (Hardcore)")), applyMode(LotusMode::UinputHC)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(LotusMode::SurroundingText, _("[4] Surrounding Text")), applyMode(LotusMode::SurroundingText)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(LotusMode::Preedit, _("[Q] Preedit")), applyMode(LotusMode::Preedit)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(LotusMode::Emoji, _("[W] Emoji Picker")), applyMode(LotusMode::Emoji)));
        candidateList->append(std::make_unique<AppModeCandidateWord>(getLabel(LotusMode::Off, _("[E] OFF")), applyMode(LotusMode::Off)));

        candidateList->append(std::make_unique<AppModeCandidateWord>(Text(_("[R] Default Typing")), [this, cleanup](InputContext* ic) {
            if (appRules_.count(currentConfigureApp_)) {
                appRules_.erase(currentConfigureApp_);
                saveAppRules();
            }
            setMode(globalMode_, ic);
            cleanup(ic);
        }));

        candidateList->append(std::make_unique<AppModeCandidateWord>(Text(_("[`] Type `")), [cleanup](InputContext* ic) {
            cleanup(ic);
            ic->commitString("`");
        }));

        int selectedIndex = 1;
        switch (realMode) {
            case LotusMode::Smooth: selectedIndex = 1; break;
            case LotusMode::Uinput: selectedIndex = 2; break;
            case LotusMode::UinputHC: selectedIndex = 3; break;
            case LotusMode::SurroundingText: selectedIndex = 4; break;
            case LotusMode::Preedit: selectedIndex = 5; break;
            case LotusMode::Emoji: selectedIndex = 6; break;
            case LotusMode::Off: selectedIndex = 7; break;
            default: selectedIndex = 1; break;
        }
        candidateList->setGlobalCursorIndex(selectedIndex);

        ic->inputPanel().reset();
        ic->inputPanel().setCandidateList(std::move(candidateList));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void LotusEngine::setMode(LotusMode mode, InputContext* ic) {
        realMode = mode;
        if (ic) {
            ic->updateUserInterface(UserInterfaceComponent::StatusArea);
        }
    }

    std::string LotusEngine::overrideIcon(const InputMethodEntry& /*entry*/) {
        if (!*config_.useLotusIcons) {
            switch (realMode) {
                case LotusMode::Off: return "fcitx-lotus-off-default";
                case LotusMode::Emoji: return "fcitx-lotus-emoji-default";
                default: return "fcitx-lotus-default";
            }
        }
        switch (realMode) {
            case LotusMode::Off: return "fcitx-lotus-off";
            case LotusMode::Emoji: return "fcitx-lotus-emoji";
            default: return "fcitx-lotus";
        }
    }

} // namespace fcitx
