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
        mainLayout->addWidget(tableWidget_);

        btnRemove_ = new QPushButton(_("Remove selection entry"), this);
        mainLayout->addWidget(btnRemove_);

        connect(btnAdd_, &QPushButton::clicked, this, &KeymapEditor::onAddClicked);
        connect(btnRemove_, &QPushButton::clicked, this, &KeymapEditor::onRemoveClicked);
        connect(btnLoadPreset_, &QPushButton::clicked, this, &KeymapEditor::onLoadPresetClicked);
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
                tableWidget_->item(i, 1)->setText(comboAction_->currentText());
                tableWidget_->item(i, 1)->setData(Qt::UserRole, comboAction_->currentData());
                emit changed(true);
                return;
            }
        }

        int row = tableWidget_->rowCount();
        tableWidget_->insertRow(row);
        tableWidget_->setItem(row, 0, new QTableWidgetItem(key));

        auto* actionItem = new QTableWidgetItem(comboAction_->currentText());
        actionItem->setData(Qt::UserRole, comboAction_->currentData());
        tableWidget_->setItem(row, 1, actionItem);
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
        reply = QMessageBox::question(this, _("Confirm"),
                                      _("This operation will delete all existing keys on the keyboard and replace them with the input method ") + presetName + _(". Are you sure?"),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) {
            return;
        }

        tableWidget_->setRowCount(0);

        const auto& keyList = presets_.at(presetName);

        for (const auto& pair : keyList) {
            QString key        = pair.first;
            QString actionCode = pair.second;

            QString displayAction = actionCode;
            for (const auto& action : bambooActions_) {
                if (action.first == actionCode) {
                    displayAction = action.second;
                    break;
                }
            }

            int row = tableWidget_->rowCount();
            tableWidget_->insertRow(row);
            tableWidget_->setItem(row, 0, new QTableWidgetItem(key));

            auto* actionItem = new QTableWidgetItem(displayAction);
            actionItem->setData(Qt::UserRole, actionCode);
            tableWidget_->setItem(row, 1, actionItem);
        }

        emit changed(true);
    }

    void KeymapEditor::load() {
        tableWidget_->setRowCount(0);

        lotusCustomKeymap config;
        fcitx::readAsIni(config, "conf/lotus-custom-keymap.conf");

        for (const auto& item : config.customKeymap.value()) {
            QString key        = QString::fromStdString(item.key.value());
            QString actionCode = QString::fromStdString(item.value.value());

            QString displayAction = actionCode;
            for (const auto& action : bambooActions_) {
                if (action.first == actionCode) {
                    displayAction = action.second;
                    break;
                }
            }

            int row = tableWidget_->rowCount();
            tableWidget_->insertRow(row);
            tableWidget_->setItem(row, 0, new QTableWidgetItem(key));

            auto* actionItem = new QTableWidgetItem(displayAction);
            actionItem->setData(Qt::UserRole, actionCode);
            tableWidget_->setItem(row, 1, actionItem);
        }
        emit changed(false);
    }

    void KeymapEditor::save() {
        lotusCustomKeymap        config;
        std::vector<lotusKeymap> newList;

        for (int i = 0; i < tableWidget_->rowCount(); ++i) {
            QString     key    = tableWidget_->item(i, 0)->text();
            QString     action = tableWidget_->item(i, 1)->data(Qt::UserRole).toString();

            lotusKeymap item;
            item.key.setValue(key.toStdString());
            item.value.setValue(action.toStdString());

            newList.push_back(item);
        }

        config.customKeymap.setValue(newList);
        fcitx::safeSaveAsIni(config, "conf/lotus-custom-keymap.conf");

        emit changed(false);
    }
} // namespace fcitx::lotus
