# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Dynamic Settings Page with Card-based Layout matching modern guidelines.
"""

from enum import Enum

from core.dbus_handler import LotusDBusHandler
from i18n import _
from qtpy.QtCore import QSize, Qt, QTimer
from qtpy.QtGui import QIcon
from qtpy.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QScrollArea,
    QVBoxLayout,
    QWidget,
)

from ui.components import (
    HotkeyEditorWidget,
    SingleKeyCaptureWidget,
)
from ui.helpers import add_help_icon


class SettingsCategory(Enum):
    GENERAL = "general"
    APPEARANCE = "appearance"
    TYPING = "typing"
    SHORTCUTS = "shortcuts"
    INTERFACE = "interface"


# Mapping of settings keys to categories and groups
SETTINGS_MAP = {
    SettingsCategory.GENERAL: {
        "INPUT METHOD": ["InputMethod", "Mode", "OutputCharset"],
        "TYPING": ["W2U", "BracketTransform"],
    },
    SettingsCategory.APPEARANCE: {
        "THEME & ICONS": ["UseLotusIcons", "IconTheme"],
    },
    SettingsCategory.TYPING: {
        "SPELLING & CORRECTIONS": ["SpellCheck", "AutoNonVnRestore", "DdFreeStyle"],
        "TYPING OPTIONS": [
            "ModernStyle",
            "FreeMarking",
            "FixUinputWithAck",
            "DoubleSpaceToPeriod",
            "DoubleHyphenToEmDash",
            "AutoCapitalizeAfterPunctuation",
            "useSurroundingTextIfPossible",
        ],
    },
    SettingsCategory.SHORTCUTS: {
        "MAIN SHORTCUTS": ["ModeMenuKey", "CycleModeKey"],
        "MODE SWITCHING": [
            "ShortcutSmooth",
            "ShortcutUinput",
            "ShortcutSuperSmooth",
            "ShortcutUinputBackspace",
            "ShortcutMinecraft",
            "ShortcutSurroundingText",
            "ShortcutPreedit",
            "ShortcutEmoji",
            "ShortcutOff",
            "ShortcutDefault",
        ],
    },
}

CATEGORY_DESCRIPTIONS = {
    SettingsCategory.GENERAL: _("Configure basic input method settings and behaviors."),
    SettingsCategory.APPEARANCE: _(
        "Customize the look and feel of the Lotus status icons and theme."
    ),
    SettingsCategory.TYPING: _("Fine-tune spelling corrections and advanced typing options."),
    SettingsCategory.SHORTCUTS: _("Manage input mode shortcuts, display order, and fast cycling."),
}

GROUP_DESCRIPTIONS = {
    "MAIN SHORTCUTS": _(
        "Assign hotkeys to open the mode menu or quickly cycle through enabled modes."
    ),
}


MODE_SHORTCUT_TO_VISIBILITY = {
    "ShortcutSmooth": "ShowModeSmooth",
    "ShortcutUinput": "ShowModeUinput",
    "ShortcutSuperSmooth": "ShowModeSuperSmooth",
    "ShortcutUinputBackspace": "ShowModeUinputBackspace",
    "ShortcutMinecraft": "ShowModeMinecraft",
    "ShortcutSurroundingText": "ShowModeSurroundingText",
    "ShortcutPreedit": "ShowModePreedit",
    "ShortcutEmoji": "ShowModeEmoji",
    "ShortcutOff": "ShowModeOff",
    "ShortcutDefault": "ShowModeDefault",
}

MODE_KEY_TO_INTERNAL_NAME = {
    "ShortcutSmooth": "Smooth",
    "ShortcutUinput": "Uinput",
    "ShortcutSuperSmooth": "SuperSmooth",
    "ShortcutUinputBackspace": "UinputBackspace",
    "ShortcutMinecraft": "Minecraft",
    "ShortcutSurroundingText": "SurroundingText",
    "ShortcutPreedit": "Preedit",
    "ShortcutEmoji": "Emoji",
    "ShortcutOff": "Off",
    "ShortcutDefault": "Default",
}

MODE_SHORTCUT_KEYS = list(MODE_SHORTCUT_TO_VISIBILITY.keys())


class CardWidget(QFrame):
    """A visual container (Card) for grouping related settings."""

    def __init__(self, title: str, parent=None):
        super().__init__(parent)
        self.setObjectName("SettingCard")

        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(16, 16, 16, 16)
        self.main_layout.setSpacing(12)

        if title:
            title_label = QLabel(title)
            title_label.setObjectName("CardTitle")
            self.main_layout.addWidget(title_label)

        self.content_layout = QVBoxLayout()
        self.content_layout.setSpacing(10)
        self.main_layout.addLayout(self.content_layout)


class DynamicSettingsPage(QWidget):
    def __init__(
        self,
        dbus_handler: LotusDBusHandler,
        category: SettingsCategory = SettingsCategory.GENERAL,
        parent=None,
    ):
        super().__init__(parent)
        self.dbus = dbus_handler
        self.category = category
        self.current_values = {}
        self.initial_values = {}
        self.modified_values = {}
        self.button_groups = []
        self.shortcut_labels = {}
        self.shortcut_warning_labels = {}
        self.validation_errors = []
        self.list_widgets = []  # Track list widgets for layout refresh

        self._setup_ui()
        self.load_config()

    def showEvent(self, event):
        """Force layout refresh when page becomes visible to fix initialization overflow."""
        super().showEvent(event)
        QTimer.singleShot(0, self._refresh_list_layouts)

    def _refresh_list_layouts(self):
        """Recalculate heights for all QListWidgets once the window has final dimensions."""
        for lw in self.list_widgets:
            total_h = 0
            for i in range(lw.count()):
                item = lw.item(i)
                widget = lw.itemWidget(item)
                if widget:
                    h = widget.sizeHint().height()
                    item.setSizeHint(QSize(100, h))
                    total_h += h + 4
            lw.setFixedHeight(total_h + 15)
            lw.update()

    def _setup_ui(self):
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)

        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll.setFrameShape(QFrame.NoFrame)

        self.container = QWidget()
        self.container_layout = QVBoxLayout(self.container)
        self.container_layout.setContentsMargins(30, 20, 30, 20)
        self.container_layout.setSpacing(20)

        self.scroll.setWidget(self.container)
        self.layout.addWidget(self.scroll)

    def load_config(self):
        self.blockSignals(True)
        try:
            while self.container_layout.count():
                item = self.container_layout.takeAt(0)
                if item.widget():
                    item.widget().deleteLater()
            self.button_groups.clear()
            self.modified_values.clear()
            self.shortcut_labels.clear()
            self.shortcut_warning_labels.clear()
            self.list_widgets.clear()

            config_data = self.dbus.get_config()
            if not config_data:
                self.container_layout.addWidget(QLabel(_("Failed to load configuration.")))
                return

            self.current_values = config_data.get("values", {})
            metadata_list = config_data.get("metadata", [])
            if not metadata_list:
                return

            # Flat map all items for easy lookup
            self.all_metadata = {}
            for group in metadata_list:
                for item in group[1]:
                    self.all_metadata[item[0]] = item

            # Render based on SETTINGS_MAP
            title_text = self.category.name.capitalize()
            title = QLabel(_(title_text))
            title.setObjectName("CategoryTitle")
            self.container_layout.addWidget(title)

            # Add category description
            desc_text = CATEGORY_DESCRIPTIONS.get(self.category)
            if desc_text:
                desc = QLabel(desc_text)
                desc.setObjectName("CategoryDescription")
                desc.setWordWrap(True)
                desc.setStyleSheet("color: gray; font-size: 13px; margin-bottom: 10px;")
                self.container_layout.addWidget(desc)

            category_groups = SETTINGS_MAP.get(self.category, {})
            for group_name, keys in category_groups.items():
                # Convert ALL CAPS to Title Case
                header_text = group_name.title() if group_name.isupper() else group_name
                header = QLabel(_(header_text))
                header.setObjectName("GroupHeader")
                self.container_layout.addWidget(header)

                if group_name == "MODE SWITCHING":
                    # Add specific instructions for mode switching
                    mode_info = QLabel(
                        _(
                            "Drag the handle on the left to reorder modes in the menu. Use checkboxes to toggle visibility, and click the buttons to reassign shortcuts."
                        )
                    )
                    mode_info.setWordWrap(True)
                    mode_info.setStyleSheet("color: gray; font-size: 13px; margin-bottom: 5px;")
                    self.container_layout.addWidget(mode_info)

                    self._render_mode_list(card_layout=self.container_layout)
                    continue

                card = CardWidget("")
                found_any = False
                for k in keys:
                    item = self.all_metadata.get(k)
                    if not item:
                        continue

                    found_any = True
                    type_str = item[1]
                    if k in ["ModeMenuKey", "CycleModeKey"] or type_str == "Hotkey":
                        self._render_hotkey(item, card.content_layout)
                    elif "Enum" in item[4]:
                        self._render_combobox(item, card.content_layout)
                    elif type_str == "Boolean":
                        self._render_checkbox(item, card.content_layout)
                    elif type_str == "String":
                        self._render_string(item, card.content_layout)

                if found_any:
                    self.container_layout.addWidget(card)

            if self.category == SettingsCategory.INTERFACE and not category_groups:
                self.container_layout.addWidget(QLabel(_("No interface settings available yet.")))

            self.initial_values = self.current_values.copy()
            self.container_layout.addStretch()
        finally:
            self.blockSignals(False)

    def is_modified_from_default(self):
        if not hasattr(self, "all_metadata"):
            return False
        for key, val in self.current_values.items():
            meta = self.all_metadata.get(key)
            if meta:
                default_val = meta[3]
                # Handle cases where default_val might be a dict (like hotkeys)
                if isinstance(default_val, dict) and isinstance(val, dict):
                    if str(val.get("0")) != str(default_val.get("0")):
                        return True
                elif str(val) != str(default_val):
                    return True
        return False

    def is_modified(self):
        """Returns True if the current values differ from the initial loaded values."""
        return self.current_values != self.initial_values

    def _render_hotkey(self, item, layout):
        key, type_str, label, default, annotations = item
        val = self.current_values.get(key, default)

        hotkey_str = val.get("0", "") if isinstance(val, dict) else ""

        row_layout = QHBoxLayout()
        row_layout.addWidget(QLabel(_(label)))
        row_layout.addStretch()

        hk_btn = HotkeyEditorWidget(hotkey_str)
        hk_btn.setFixedWidth(235)
        hk_btn.textChanged.connect(lambda text, k=key: self.update_config(k, {"0": text}))

        row_layout.addWidget(hk_btn)
        layout.addLayout(row_layout)

    def _render_combobox(self, item, layout):
        key, type_str, label, default, annotations = item
        val = str(self.current_values.get(key, default))

        if "Enum" not in annotations:
            return

        row_layout = QHBoxLayout()
        label_widget = QLabel(_(label))
        row_layout.addWidget(label_widget)
        add_help_icon(row_layout, key)
        row_layout.addStretch()

        combo = QComboBox()
        combo.setFixedWidth(200)
        enum_dict = annotations.get("Enum", {})
        sorted_keys = sorted(enum_dict.keys(), key=lambda x: int(x) if str(x).isdigit() else x)

        for k in sorted_keys:
            rb_text = str(enum_dict[k])
            combo.addItem(_(rb_text), rb_text)

        idx = combo.findData(val)
        if idx >= 0:
            combo.setCurrentIndex(idx)

        combo.currentTextChanged.connect(
            lambda text, k=key: self.update_config(k, combo.currentData())
        )
        row_layout.addWidget(combo)
        layout.addLayout(row_layout)

    def _render_checkbox(self, item, layout):
        key, type_str, label, default, annotations = item
        val = self.current_values.get(key, default)

        row_layout = QHBoxLayout()
        row_layout.setContentsMargins(0, 0, 0, 0)
        cb = QCheckBox(_(label))
        is_checked = str(val).lower() == "true"
        cb.setChecked(is_checked)

        cb.toggled.connect(
            lambda checked, k=key: self.update_config(k, "True" if checked else "False")
        )
        row_layout.addWidget(cb)
        add_help_icon(row_layout, key)
        row_layout.addStretch()
        layout.addLayout(row_layout)

    def _render_string(self, item, layout):
        key, type_str, label, default, annotations = item
        val = str(self.current_values.get(key, default))

        wrapper = QVBoxLayout()
        wrapper.setSpacing(2)
        wrapper.setContentsMargins(0, 0, 0, 0)

        row_layout = QHBoxLayout()
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.setSpacing(8)

        # If it's a shortcut key, add the corresponding visibility checkbox
        if key in MODE_SHORTCUT_TO_VISIBILITY:
            visibility_key = MODE_SHORTCUT_TO_VISIBILITY[key]
            visibility_val = self.current_values.get(visibility_key, "True")

            cb = QCheckBox()
            cb.setChecked(str(visibility_val).lower() == "true")
            cb.toggled.connect(
                lambda checked, k=visibility_key: self.update_config(
                    k, "True" if checked else "False"
                )
            )
            row_layout.addWidget(cb)

        label_widget = QLabel(_(label))
        row_layout.addWidget(label_widget)
        row_layout.addStretch()

        if key in MODE_SHORTCUT_KEYS:
            # Use single key capture for shortcuts
            capture_btn = SingleKeyCaptureWidget(val)
            capture_btn.setFixedWidth(100)
            capture_btn.textChanged.connect(lambda text, k=key: self.update_config(k, text))
            row_layout.addWidget(capture_btn)
        else:
            edit = QLineEdit(val)
            edit.setFixedWidth(56)
            edit.setMaxLength(1)
            edit.setPlaceholderText("-")
            edit.textChanged.connect(lambda text, k=key: self.update_config(k, text))
            row_layout.addWidget(edit)

        wrapper.addLayout(row_layout)

        if key in MODE_SHORTCUT_KEYS:
            warning = QLabel()
            warning.setObjectName("ShortcutWarning")
            warning.setWordWrap(True)
            warning.hide()
            wrapper.addWidget(warning)
            self.shortcut_labels[key] = label_widget
            self.shortcut_warning_labels[key] = warning

        layout.addLayout(wrapper)

    def _render_mode_list(self, card_layout):
        from qtpy.QtWidgets import QAbstractItemView, QListWidget, QListWidgetItem

        card = CardWidget("")
        card.content_layout.setContentsMargins(4, 4, 4, 4)

        list_widget = QListWidget()
        list_widget.setDragDropMode(QAbstractItemView.InternalMove)
        list_widget.setSelectionMode(QAbstractItemView.SingleSelection)
        list_widget.setFocusPolicy(Qt.NoFocus)
        list_widget.setFrameShape(QFrame.NoFrame)
        list_widget.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        list_widget.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        list_widget.setStyleSheet(
            "QListWidget { background: transparent; } QListWidget::item { margin: 2px 0; }"
        )

        # Get current order from config
        order_str = self.current_values.get(
            "ModeOrder",
            "Smooth,Uinput,Minecraft,SurroundingText,Preedit,Emoji,Off,SuperSmooth,UinputBackspace,Default",
        )
        order = order_str.split(",")

        # Ensure all modes are present
        all_internal_names = list(MODE_KEY_TO_INTERNAL_NAME.values())
        for name in all_internal_names:
            if name not in order:
                order.append(name)

        # Map internal name back to shortcut key
        internal_to_key = {v: k for k, v in MODE_KEY_TO_INTERNAL_NAME.items()}

        total_height = 0
        for name in order:
            key = internal_to_key.get(name)
            if not key:
                continue

            item_meta = self.all_metadata.get(key)
            if not item_meta:
                continue

            list_item = QListWidgetItem(list_widget)
            container = QWidget()
            row_layout = QHBoxLayout(container)
            row_layout.setContentsMargins(4, 2, 4, 2)
            row_layout.setSpacing(4)

            # Drag handle icon with fallback
            handle = QLabel()
            icon = QIcon.fromTheme("list-drag-handle")
            if icon.isNull():
                icon = QIcon.fromTheme("view-restore")
            if icon.isNull():
                icon = QIcon.fromTheme("grabber")

            if not icon.isNull():
                handle.setPixmap(icon.pixmap(16, 16))
            else:
                handle.setText("☰")
                handle.setStyleSheet("font-size: 14px; color: palette(mid); font-weight: bold;")

            handle.setFixedSize(24, 24)
            handle.setAlignment(Qt.AlignCenter)
            row_layout.addWidget(handle)

            # Use the existing render logic but into our row
            self._render_string(item_meta, row_layout)

            hint = container.sizeHint()
            # ONLY use the height from hint, use small width to let QListWidget expand it properly.
            # Large width hints from QHBoxLayout+Stretch cause overflow.
            list_item.setSizeHint(QSize(100, hint.height()))
            total_height += hint.height() + 4  # 4 for margins/spacing

            list_widget.addItem(list_item)
            list_widget.setItemWidget(list_item, container)

            # Store internal name in the item's data for reordering
            list_item.setData(Qt.UserRole, name)

        self.list_widgets.append(list_widget)
        list_widget.model().rowsMoved.connect(lambda *args: self._update_mode_order(list_widget))

        card.content_layout.addWidget(list_widget)
        card_layout.addWidget(card)

    def _update_mode_order(self, list_widget):
        new_order = []
        for i in range(list_widget.count()):
            item = list_widget.item(i)
            new_order.append(item.data(Qt.UserRole))

        self.update_config("ModeOrder", ",".join(new_order))

    def _validate_mode_shortcuts(self):
        """Check for duplicate shortcuts among enabled modes."""
        self.validation_errors.clear()

        # Reset warnings and styles
        for key in MODE_SHORTCUT_KEYS:
            if key in self.shortcut_warning_labels:
                self.shortcut_warning_labels[key].hide()
            if key in self.shortcut_labels:
                self.shortcut_labels[key].setStyleSheet("")

        # Only check enabled modes
        enabled_shortcuts = {}
        for shortcut_key, visibility_key in MODE_SHORTCUT_TO_VISIBILITY.items():
            default_visibility = "True"
            vis_meta = (
                self.all_metadata.get(visibility_key) if hasattr(self, "all_metadata") else None
            )
            if vis_meta:
                default_visibility = vis_meta[3]

            is_enabled = (
                str(self.current_values.get(visibility_key, default_visibility)).lower() == "true"
            )
            if is_enabled:
                default_shortcut = ""
                shortcut_meta = (
                    self.all_metadata.get(shortcut_key) if hasattr(self, "all_metadata") else None
                )
                if shortcut_meta:
                    default_shortcut = shortcut_meta[3]
                val = self.current_values.get(shortcut_key, default_shortcut)
                if val:
                    if val in enabled_shortcuts:
                        enabled_shortcuts[val].append(shortcut_key)
                    else:
                        enabled_shortcuts[val] = [shortcut_key]

        # Flag duplicates
        for shortcut, keys in enabled_shortcuts.items():
            if len(keys) > 1:
                error_msg = _("Duplicate shortcut '{}' used for multiple enabled modes.").format(
                    shortcut
                )
                self.validation_errors.append(error_msg)
                for key in keys:
                    if key in self.shortcut_warning_labels:
                        self.shortcut_warning_labels[key].setText(error_msg)
                        self.shortcut_warning_labels[key].show()
                    if key in self.shortcut_labels:
                        self.shortcut_labels[key].setStyleSheet(
                            "color: palette(highlight); font-weight: bold;"
                        )

    def load_data(self):
        """Standardized reload method (alias for load_config)."""
        self.load_config()

    def restore_defaults(self):
        """Resets current values to engine defaults."""
        self.blockSignals(True)
        try:
            config_data = self.dbus.get_config()
            if not config_data:
                return

            metadata_list = config_data.get("metadata", [])
            new_values = {}
            for group in metadata_list:
                for item in group[1]:
                    key, type_str, label, default, annotations = item
                    new_values[key] = default
            self.modified_values = new_values.copy()
            self.current_values = new_values
            self.load_config()
        finally:
            self.blockSignals(False)

    def has_validation_errors(self):
        self._validate_mode_shortcuts()
        return bool(self.validation_errors)

    def validation_message(self):
        return "\n".join(self.validation_errors)

    def save_data(self) -> bool:
        """Commits all staged changes to DBus."""
        if self.has_validation_errors():
            return False

        if not self.modified_values:
            return True

        config_data = self.dbus.get_config()
        if config_data:
            latest_values = config_data.get("values", {})
            latest_values.update(self.modified_values)
            if not self.dbus.set_config(latest_values):
                return False
            self.modified_values.clear()
            self.initial_values = self.current_values.copy()
            return True
        elif not self.dbus.iface:
            return False
        return True

    def update_config(self, key: str, new_value):
        """Updates internal state and notifies parent window of change."""
        self.modified_values[key] = new_value
        self.current_values[key] = new_value

        # Real-time validation if it's a shortcut
        if key in MODE_SHORTCUT_KEYS or key in MODE_SHORTCUT_TO_VISIBILITY.values():
            self._validate_mode_shortcuts()

        # Notify the parent window (LotusSettingsWindow) if it exists
        main_win = self.window()
        if hasattr(main_win, "on_changed"):
            main_win.on_changed()
