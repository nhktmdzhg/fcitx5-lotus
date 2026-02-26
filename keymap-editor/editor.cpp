/*
 * SPDX-FileCopyrightText: 2012-2018 CSSlayer <wengxt@gmail.com>
 * SPDX-FileCopyrightText: 2026 Nguyễn Hoàng Kỳ  <nhktmdzhg@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "editor.h"
#include "lotus-config.h"
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/standardpath.h>
#include <QHeaderView>
#include <qtmetamacros.h>
#include <QLabel>
#include <QMessageBox>

namespace fcitx::lotus {
    KeymapEditor::KeymapEditor(QWidget* parent) : FcitxQtConfigUIWidget(parent) {
        auto* mainLayout = new QVBoxLayout(this);

        auto* presetLayout = new QHBoxLayout();
        comboPreset_       = new QComboBox(this);
        for (const auto& preset : presets_) {
            comboPreset_->addItem(preset.first);
        }
        btnLoadPreset_ = new QPushButton(_("Import from existing keymap"), this);

        presetLayout->addWidget(new QLabel(_("Original input method"), this));
        presetLayout->addWidget(comboPreset_);
        presetLayout->addWidget(btnLoadPreset_);
        presetLayout->addStretch();

        mainLayout->addLayout(presetLayout);

        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        mainLayout->addWidget(line);

        auto* addLayout = new QHBoxLayout();
        inputKey_       = new QLineEdit(this);
        inputKey_->setPlaceholderText(_("Key (Example: s)"));
        inputKey_->setMaxLength(1);

        comboAction_ = new QComboBox(this);
        for (const auto& action : bambooActions_) {
            comboAction_->addItem(action.second, action.first);
        }

        btnAdd_ = new QPushButton(_("Add/ Update"), this);
        addLayout->addWidget(inputKey_);
        addLayout->addWidget(comboAction_);
        addLayout->addWidget(btnAdd_);
        mainLayout->addLayout(addLayout);

        tableWidget_ = new QTableWidget(0, 2, this);
        tableWidget_->setHorizontalHeaderLabels({_("Key"), _("Action")});
        tableWidget_->horizontalHeader()->setStretchLastSection(true);
        tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);

        tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        mainLayout->addWidget(tableWidget_);

        btnRemove_ = new QPushButton(_("Remove selection entry"), this);
        mainLayout->addWidget(btnRemove_);

        connect(btnAdd_, &QPushButton::clicked, this, &KeymapEditor::onAddClicked);
        connect(btnRemove_, &QPushButton::clicked, this, &KeymapEditor::onRemoveClicked);
        connect(btnLoadPreset_, &QPushButton::clicked, this, &KeymapEditor::onLoadPresetClicked);
        load();
    }

    QString KeymapEditor::title() {
        return _("Lotus Custom Keymap");
    }

    QString KeymapEditor::icon() {
        return "fcitx-lotus";
    }

    void KeymapEditor::onAddClicked() {
        QString key = inputKey_->text().trimmed();
        if (key.isEmpty())
            return;

        for (int i = 0; i < tableWidget_->rowCount(); ++i) {
            if (tableWidget_->item(i, 0)->text() == key) {
                auto* cellCombo = qobject_cast<QComboBox*>(tableWidget_->cellWidget(i, 1));
                if (cellCombo) {
                    cellCombo->setCurrentIndex(comboAction_->currentIndex());
                }
                emit changed(true);
                return;
            }
        }

        int row = tableWidget_->rowCount();
        tableWidget_->insertRow(row);
        tableWidget_->setItem(row, 0, new QTableWidgetItem(key));

        auto* cellCombo = new QComboBox();
        for (const auto& action : bambooActions_) {
            cellCombo->addItem(action.second, action.first);
        }
        cellCombo->setCurrentIndex(comboAction_->currentIndex());

        connect(cellCombo, &QComboBox::currentIndexChanged, this, [this]() { emit changed(true); });

        tableWidget_->setCellWidget(row, 1, cellCombo);
        emit changed(true);
    }

    void KeymapEditor::onRemoveClicked() {
        int row = tableWidget_->currentRow();
        if (row >= 0) {
            tableWidget_->removeRow(row);
            emit changed(true);
        }
    }

    void KeymapEditor::onLoadPresetClicked() {
        QString                     presetName = comboPreset_->currentText();

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(
            this, _("Confirm"), _("This operation will delete all existing keys on the current keymap and replace them with the input method ") + presetName + _(". Are you sure?"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) {
            return;
        }

        tableWidget_->setRowCount(0);

        const auto& keyList = presets_.at(presetName);

        for (const auto& pair : keyList) {
            QString key        = pair.first;
            QString actionCode = pair.second;

            int     row = tableWidget_->rowCount();
            tableWidget_->insertRow(row);
            tableWidget_->setItem(row, 0, new QTableWidgetItem(key));

            auto* cellCombo = new QComboBox();
            for (const auto& action : bambooActions_) {
                cellCombo->addItem(action.second, action.first);
            }

            int idx = cellCombo->findData(actionCode);
            if (idx >= 0) {
                cellCombo->setCurrentIndex(idx);
            }

            connect(cellCombo, &QComboBox::currentIndexChanged, this, [this]() { emit changed(true); });

            tableWidget_->setCellWidget(row, 1, cellCombo);
        }

        emit changed(true);
    }

    void KeymapEditor::load() {
        tableWidget_->setRowCount(0);

        lotusCustomKeymap config;
#if LOTUS_USE_MODERN_FCITX_API
        std::string configDir = (StandardPaths::global().userDirectory(StandardPathsType::Config) / "fcitx5" / "conf").string();
#else
        std::string configDir = StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
#endif
        std::string path = configDir + "/lotus-custom-keymap.conf";
        fcitx::readAsIni(config, path);

        for (const auto& item : config.customKeymap.value()) {
            QString key        = QString::fromStdString(item.key.value());
            QString actionCode = QString::fromStdString(item.value.value());

            int     row = tableWidget_->rowCount();
            tableWidget_->insertRow(row);
            tableWidget_->setItem(row, 0, new QTableWidgetItem(key));

            auto* cellCombo = new QComboBox();
            for (const auto& action : bambooActions_) {
                cellCombo->addItem(action.second, action.first);
            }

            int idx = cellCombo->findData(actionCode);
            if (idx >= 0) {
                cellCombo->setCurrentIndex(idx);
            }

            connect(cellCombo, &QComboBox::currentIndexChanged, this, [this]() { emit changed(true); });

            tableWidget_->setCellWidget(row, 1, cellCombo);
        }
        emit changed(false);
    }

    void KeymapEditor::save() {
        lotusCustomKeymap        config;
        std::vector<lotusKeymap> newList;

        for (int i = 0; i < tableWidget_->rowCount(); ++i) {
            QString key = tableWidget_->item(i, 0)->text();

            auto*   cellCombo = qobject_cast<QComboBox*>(tableWidget_->cellWidget(i, 1));
            if (!cellCombo)
                continue;

            QString     action = cellCombo->currentData().toString();

            lotusKeymap item;
            item.key.setValue(key.toStdString());
            item.value.setValue(action.toStdString());

            newList.push_back(item);
        }

        config.customKeymap.setValue(newList);
#if LOTUS_USE_MODERN_FCITX_API
        std::string configDir = (StandardPaths::global().userDirectory(StandardPathsType::Config) / "fcitx5" / "conf").string();
#else
        std::string configDir = StandardPath::global().userDirectory(StandardPath::Type::Config) + "/fcitx5/conf";
#endif
        std::string path = configDir + "/lotus-custom-keymap.conf";
        fcitx::safeSaveAsIni(config, path);

        emit changed(false);
    }
} // namespace fcitx::lotus