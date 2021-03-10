/*
 * Copyright (c) 2020-2021 Valve Corporation
 * Copyright (c) 2020-2021 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors:
 * - Richard S. Wright Jr. <richard@lunarg.com>
 * - Christophe Riccio <christophe@lunarg.com>
 */

#include "configurator.h"
#include "settings_tree.h"

#include "widget_setting_int.h"
#include "widget_setting_int_range.h"
#include "widget_setting_bool.h"
#include "widget_setting_enum.h"
#include "widget_setting_string.h"
#include "widget_setting_flags.h"
#include "widget_setting_filesystem.h"
#include "widget_setting_list.h"
#include "widget_setting_search.h"

#include "../vkconfig_core/version.h"
#include "../vkconfig_core/platform.h"
#include "../vkconfig_core/util.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QApplication>

#include <cassert>

SettingsTreeManager::SettingsTreeManager()
    : _settings_tree(nullptr),
      _validation_log_file_widget(nullptr),
      _validation_debug_action(nullptr),
      _validation_areas(nullptr) {}

void SettingsTreeManager::CreateGUI(QTreeWidget *build_tree) {
    assert(build_tree);

    // Do this first to make absolutely sure if these is an old configuration still active it's state gets saved.
    CleanupGUI();

    Configurator &configurator = Configurator::Get();

    _settings_tree = build_tree;
    Configuration *configuration = configurator.configurations.GetActiveConfiguration();
    assert(configuration != nullptr);

    _settings_tree->blockSignals(true);
    _settings_tree->clear();

    QFont font_layer = _settings_tree->font();
    font_layer.setBold(true);

    QFont font_section = _settings_tree->font();
    font_section.setItalic(true);

    if (!configuration->HasOverride()) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, "No overridden or excluded layer");
        item->setFont(0, font_section);
        _settings_tree->addTopLevelItem(item);
    } else {
        const std::size_t overridden_layer_count = CountOverriddenLayers(configuration->parameters);

        if (overridden_layer_count > 1) {
            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, "Vulkan Applications");
            item->setTextAlignment(0, Qt::AlignCenter);
            item->setFont(0, font_section);
            item->setDisabled(true);
            _settings_tree->addTopLevelItem(item);
        }

        // There will be one top level item for each layer
        for (std::size_t i = 0, n = configuration->parameters.size(); i < n; ++i) {
            Parameter &parameter = configuration->parameters[i];
            if (!IsPlatformSupported(parameter.platform_flags)) continue;

            if (parameter.state != LAYER_STATE_OVERRIDDEN) continue;

            const std::vector<Layer> &available_layers = configurator.layers.available_layers;
            const Layer *layer = FindByKey(available_layers, parameter.key.c_str());

            std::string layer_text = parameter.key;
            if (layer == nullptr) {
                layer_text += " (Missing)";
            } else if (layer->status != STATUS_STABLE) {
                layer_text += std::string(" (") + GetToken(layer->status) + ")";
            }

            QTreeWidgetItem *layer_item = new QTreeWidgetItem();
            layer_item->setText(0, layer_text.c_str());
            layer_item->setFont(0, font_layer);
            // layer_item->setExpanded(!parameter.collapsed);
            if (layer != nullptr) layer_item->setToolTip(0, layer->description.c_str());
            _settings_tree->addTopLevelItem(layer_item);

            if (layer == nullptr) continue;

            // Handle the case were we get off easy. No settings.
            if (parameter.settings.Empty()) {
                QTreeWidgetItem *layer_child_item = new QTreeWidgetItem();
                layer_child_item->setText(0, "No User Settings");
                // layer_child_item->setExpanded(true);
                layer_item->addChild(layer_child_item);
                continue;
            }

            if (!layer->presets.empty()) {
                QTreeWidgetItem *presets_item = new QTreeWidgetItem();
                // presets_item->setExpanded(true);
                WidgetPreset *presets_combobox = new WidgetPreset(presets_item, *layer, parameter);
                connect(presets_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(OnPresetChanged(int)));
                layer_item->addChild(presets_item);
                _settings_tree->setItemWidget(presets_item, 0, presets_combobox);
                _presets_comboboxes.push_back(presets_combobox);
            }

            if (parameter.key == "VK_LAYER_KHRONOS_validation") {
                BuildValidationTree(layer_item, parameter);
            } else {
                BuildGenericTree(layer_item, parameter);
            }
        }

        if (overridden_layer_count > 1) {
            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, "Vulkan Drivers");
            item->setTextAlignment(0, Qt::AlignCenter);
            item->setFont(0, font_section);
            item->setDisabled(true);
            _settings_tree->addTopLevelItem(item);
        }

        const std::size_t excluded_layer_count =
            CountExcludedLayers(configuration->parameters, configurator.layers.available_layers);

        if (excluded_layer_count > 0) {
            // The last item is just the excluded layers
            QTreeWidgetItem *excluded_layers = new QTreeWidgetItem();
            excluded_layers->setText(0, "Excluded Layers:");
            excluded_layers->setFont(0, font_section);
            excluded_layers->setExpanded(true);
            _settings_tree->addTopLevelItem(excluded_layers);

            for (std::size_t i = 0, n = configuration->parameters.size(); i < n; ++i) {
                Parameter &parameter = configuration->parameters[i];
                if (!IsPlatformSupported(parameter.platform_flags)) continue;

                if (parameter.state != LAYER_STATE_EXCLUDED) continue;

                const Layer *layer = FindByKey(configurator.layers.available_layers, parameter.key.c_str());
                if (layer == nullptr) continue;  // Do not display missing excluded layers

                QTreeWidgetItem *layer_item = new QTreeWidgetItem();
                layer_item->setText(0, parameter.key.c_str());
                layer_item->setFont(0, font_layer);
                layer_item->setToolTip(0, layer->description.c_str());
                excluded_layers->addChild(layer_item);
            }
        }
    }

    //_settings_tree->expandAll();
    _settings_tree->resizeColumnToContents(0);
    SetTreeState(configuration->setting_tree_state, 0, _settings_tree->invisibleRootItem());
    _settings_tree->blockSignals(false);
}

void SettingsTreeManager::CleanupGUI() {
    if (_settings_tree == nullptr)  // Was not initialized
        return;

    Configurator &configurator = Configurator::Get();

    Configuration *configuration = configurator.configurations.GetActiveConfiguration();
    if (configuration == nullptr) return;

    if (_validation_areas) {
        delete _validation_areas;
        _validation_areas = nullptr;
    }

    _presets_comboboxes.clear();

    _settings_tree->clear();
    _settings_tree = nullptr;
    _validation_debug_action = nullptr;
    _validation_log_file_widget = nullptr;
}

void SettingsTreeManager::BuildValidationTree(QTreeWidgetItem *parent, Parameter &parameter) {
    Configurator &configurator = Configurator::Get();
    std::vector<Layer> &available_layers = configurator.layers.available_layers;
    Layer *validation_layer = FindByKey(available_layers, "VK_LAYER_KHRONOS_validation");
    assert(validation_layer != nullptr);

    SettingMetaSet &layer_settings = validation_layer->settings;
    SettingDataSet &param_settings = parameter.settings;

    QTreeWidgetItem *validation_areas_item = new QTreeWidgetItem();
    validation_areas_item->setText(0, "Validation Areas");
    // validation_areas_item->setExpanded(true);
    parent->addChild(validation_areas_item);

    // This just finds the enables and disables
    _validation_areas = new SettingsValidationAreas(_settings_tree, validation_areas_item, validation_layer->_api_version,
                                                    layer_settings, param_settings);

    const SettingMetaFlags *debug_action_meta = layer_settings.Get<SettingMetaFlags>("debug_action");
    assert(debug_action_meta != nullptr);
    assert(debug_action_meta->type == SETTING_FLAGS);
    SettingDataFlags *debug_action_data = param_settings.Get<SettingDataFlags>("debug_action");
    assert(debug_action_data != nullptr);
    assert(debug_action_data->type == SETTING_FLAGS);

    const SettingMetaFileSave *log_file_meta = layer_settings.Get<SettingMetaFileSave>("log_filename");
    assert(log_file_meta != nullptr);
    assert(log_file_meta->type == SETTING_SAVE_FILE);
    SettingDataFileSave *log_file_data = param_settings.Get<SettingDataFileSave>("log_filename");
    assert(log_file_data != nullptr);
    assert(log_file_data->type == SETTING_SAVE_FILE);

    // The debug action set of settings has it's own branch
    QTreeWidgetItem *debug_action_branch = new QTreeWidgetItem();
    debug_action_branch->setText(0, debug_action_meta->label.c_str());
    debug_action_branch->setExpanded(true);
    parent->addChild(debug_action_branch);

    // Each debug action has it's own checkbox
    for (std::size_t i = 0, n = debug_action_meta->enum_values.size(); i < n; ++i) {
        if (!IsPlatformSupported(debug_action_meta->enum_values[i].platform_flags)) continue;

        QTreeWidgetItem *child = new QTreeWidgetItem();
        WidgetSettingFlag *widget =
            new WidgetSettingFlag(*debug_action_meta, *debug_action_data, debug_action_meta->enum_values[i].key);
        debug_action_branch->addChild(child);
        _settings_tree->setItemWidget(child, 0, widget);
        widget->setFont(_settings_tree->font());
        connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));

        // The log message action also has a child; the log file selection setting/widget
        // Note, this is usually last, but I'll check for it any way in case other new items are added
        if (debug_action_meta->enum_values[i].key == "VK_DBG_LAYER_ACTION_LOG_MSG") {  // log action?
            _validation_debug_action = widget;
            _validation_log_file_widget = new WidgetSettingFilesystem(_settings_tree, child, *log_file_meta, *log_file_data);
            connect(_validation_log_file_widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            connect(_validation_debug_action, SIGNAL(stateChanged(int)), this, SLOT(OnDebugLogMessageChanged(int)));

            // Capture initial state, which reflects enabled/disabled
            _validation_log_file_widget->setDisabled(!_validation_debug_action->isChecked());
        }
    }

    const SettingMetaFlags *meta_report_flags = layer_settings.Get<SettingMetaFlags>("report_flags");
    if (meta_report_flags != nullptr) {
        const SettingMetaFlags &setting_meta = *meta_report_flags;
        SettingDataFlags &setting_data = param_settings.Create<SettingDataFlags>(setting_meta.key, setting_meta.type);

        QTreeWidgetItem *sub_category = new QTreeWidgetItem;
        sub_category->setText(0, setting_meta.label.c_str());
        sub_category->setToolTip(0, setting_meta.description.c_str());
        parent->addChild(sub_category);

        for (std::size_t i = 0, n = setting_meta.enum_values.size(); i < n; ++i) {
            QTreeWidgetItem *child = new QTreeWidgetItem();
            WidgetSettingFlag *widget = new WidgetSettingFlag(setting_meta, setting_data, setting_meta.enum_values[i].key.c_str());
            sub_category->addChild(child);
            _settings_tree->setItemWidget(child, 0, widget);
            widget->setFont(_settings_tree->font());
            connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
        }
    }

    const SettingMetaInt *meta_duplicate_message_limit = layer_settings.Get<SettingMetaInt>("duplicate_message_limit");
    if (meta_duplicate_message_limit != nullptr) {
        const SettingMetaInt &setting_meta = *meta_duplicate_message_limit;
        SettingDataInt &setting_data = param_settings.Create<SettingDataInt>(setting_meta.key, setting_meta.type);

        WidgetSettingInt *widget = new WidgetSettingInt(_settings_tree, parent, setting_meta, setting_data);
        connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
    }

    const SettingMetaList *meta_message_id_list = layer_settings.Get<SettingMetaList>("message_id_filter");
    if (meta_message_id_list != nullptr) {
        const SettingMetaList &setting_meta = *meta_message_id_list;
        SettingDataList &setting_data = param_settings.Create<SettingDataList>(setting_meta.key, setting_meta.type);

        QTreeWidgetItem *mute_message_item = new QTreeWidgetItem;
        mute_message_item->setText(0, setting_meta.label.c_str());
        mute_message_item->setToolTip(0, setting_meta.description.c_str());
        mute_message_item->setExpanded(true);
        parent->addChild(mute_message_item);

        WidgetSettingSearch *widget_search = new WidgetSettingSearch(setting_meta.list, setting_data.value);

        QTreeWidgetItem *next_line = new QTreeWidgetItem();
        next_line->setSizeHint(0, QSize(0, 24));
        mute_message_item->addChild(next_line);
        _settings_tree->setItemWidget(next_line, 0, widget_search);

        QTreeWidgetItem *list_item = new QTreeWidgetItem();
        mute_message_item->addChild(list_item);
        list_item->setSizeHint(0, QSize(0, 200));
        WidgetSettingList *widget_list = new WidgetSettingList(setting_meta, setting_data);
        _settings_tree->setItemWidget(list_item, 0, widget_list);

        connect(widget_search, SIGNAL(itemSelected(const QString &)), widget_list, SLOT(addItem(const QString &)));
        connect(widget_search, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
        connect(widget_list, SIGNAL(itemRemoved(const QString &)), widget_search, SLOT(addToSearchList(const QString &)));
        connect(widget_list, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()), Qt::QueuedConnection);
    }

    connect(_validation_areas, SIGNAL(settingChanged()), this, SLOT(OnSettingChanged()));
}

void SettingsTreeManager::OnDebugLogMessageChanged(int index) {
    (void)index;
    const bool enabled = _validation_debug_action->isChecked();
    _settings_tree->blockSignals(true);
    _validation_log_file_widget->Enable(enabled);
    _settings_tree->blockSignals(false);
    OnSettingChanged();
}

void SettingsTreeManager::BuildGenericTree(QTreeWidgetItem *parent, Parameter &parameter) {
    std::vector<Layer> &available_layers = Configurator::Get().layers.available_layers;

    const SettingMetaSet &setting_metas = FindByKey(available_layers, parameter.key.c_str())->settings;
    SettingDataSet &setting_datas = parameter.settings;

    for (std::size_t setting_index = 0, n = setting_metas.Size(); setting_index < n; ++setting_index) {
        const SettingMeta &setting_meta = setting_metas[setting_index];

        if (!IsPlatformSupported(setting_meta.platform_flags)) continue;

        switch (setting_meta.type) {
            case SETTING_BOOL:
            case SETTING_BOOL_NUMERIC_DEPRECATED: {
                const SettingMetaBool &meta = static_cast<const SettingMetaBool &>(setting_meta);
                SettingDataBool *data = setting_datas.Get<SettingDataBool>(setting_meta.key.c_str());
                assert(data != nullptr);

                WidgetSettingBool *widget = new WidgetSettingBool(_settings_tree, parent, meta, *data);
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            } break;

            case SETTING_INT: {
                const SettingMetaInt &meta = static_cast<const SettingMetaInt &>(setting_meta);
                SettingDataInt *data = setting_datas.Get<SettingDataInt>(setting_meta.key.c_str());
                assert(data != nullptr);

                WidgetSettingInt *widget = new WidgetSettingInt(_settings_tree, parent, meta, *data);
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            } break;

            case SETTING_SAVE_FILE:
            case SETTING_LOAD_FILE:
            case SETTING_SAVE_FOLDER: {
                const SettingMetaFilesystem &meta = static_cast<const SettingMetaFilesystem &>(setting_meta);
                SettingDataString *data = setting_datas.Get<SettingDataString>(setting_meta.key.c_str());
                assert(data != nullptr);

                WidgetSettingFilesystem *widget = new WidgetSettingFilesystem(_settings_tree, parent, meta, *data);
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            } break;

            case SETTING_ENUM: {
                const SettingMetaEnum &meta = static_cast<const SettingMetaEnum &>(setting_meta);
                SettingDataEnum *data = setting_datas.Get<SettingDataEnum>(setting_meta.key.c_str());
                assert(data != nullptr);

                WidgetSettingEnum *enum_widget = new WidgetSettingEnum(_settings_tree, parent, meta, *data);
                connect(enum_widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            } break;

            case SETTING_FLAGS: {
                QTreeWidgetItem *setting_item = new QTreeWidgetItem();
                parent->addChild(setting_item);

                const SettingMetaFlags &meta = static_cast<const SettingMetaFlags &>(setting_meta);
                SettingDataFlags *data = setting_datas.Get<SettingDataFlags>(setting_meta.key.c_str());
                assert(data != nullptr);

                setting_item->setText(0, setting_meta.label.c_str());
                setting_item->setToolTip(0, setting_meta.description.c_str());

                for (std::size_t i = 0, n = meta.enum_values.size(); i < n; ++i) {
                    WidgetSettingFlag *widget = new WidgetSettingFlag(meta, *data, meta.enum_values[i].key.c_str());
                    QTreeWidgetItem *place_holder = new QTreeWidgetItem();
                    setting_item->addChild(place_holder);
                    _settings_tree->setItemWidget(place_holder, 0, widget);
                    widget->setFont(_settings_tree->font());
                    connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
                }
            } break;

            case SETTING_INT_RANGES: {
                const SettingMetaIntRanges &meta = static_cast<const SettingMetaIntRanges &>(setting_meta);
                SettingDataIntRanges *data = setting_datas.Get<SettingDataIntRanges>(setting_meta.key.c_str());
                assert(data != nullptr);

                WidgetSettingIntRanges *widget = new WidgetSettingIntRanges(_settings_tree, parent, meta, *data);
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            } break;

            case SETTING_STRING: {
                const SettingMetaString &meta = static_cast<const SettingMetaString &>(setting_meta);
                SettingDataString *data = setting_datas.Get<SettingDataString>(setting_meta.key.c_str());
                assert(data != nullptr);

                WidgetSettingString *widget = new WidgetSettingString(_settings_tree, parent, meta, *data);
                connect(widget, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
            } break;

            case SETTING_LIST: {
                const SettingMetaList &meta = static_cast<const SettingMetaList &>(setting_meta);
                SettingDataList *data = setting_datas.Get<SettingDataList>(setting_meta.key.c_str());
                assert(data != nullptr);

                QTreeWidgetItem *mute_message_item = new QTreeWidgetItem;
                mute_message_item->setText(0, setting_meta.label.c_str());
                mute_message_item->setToolTip(0, setting_meta.description.c_str());
                parent->addChild(mute_message_item);

                WidgetSettingSearch *widget_search = new WidgetSettingSearch(meta.list, data->value);

                QTreeWidgetItem *next_line = new QTreeWidgetItem();
                next_line->setSizeHint(0, QSize(0, 28));
                mute_message_item->addChild(next_line);
                _settings_tree->setItemWidget(next_line, 0, widget_search);

                QTreeWidgetItem *list_item = new QTreeWidgetItem();
                mute_message_item->addChild(list_item);
                list_item->setSizeHint(0, QSize(0, 200));

                WidgetSettingList *widget_list = new WidgetSettingList(meta, *data);
                _settings_tree->setItemWidget(list_item, 0, widget_list);

                connect(widget_search, SIGNAL(itemSelected(const QString &)), widget_list, SLOT(addItem(const QString &)));
                connect(widget_search, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()));
                connect(widget_list, SIGNAL(itemRemoved(const QString &)), widget_search, SLOT(addToSearchList(const QString &)));
                connect(widget_list, SIGNAL(itemChanged()), this, SLOT(OnSettingChanged()), Qt::QueuedConnection);
            } break;

            default: {
                assert(0);  // Unknown setting
            } break;
        }
    }
}

void SettingsTreeManager::OnPresetChanged(int combox_preset_index) {
    (void)combox_preset_index;

    CreateGUI(_settings_tree);

    Configurator &configurator = Configurator::Get();
    configurator.environment.Notify(NOTIFICATION_RESTART);
    configurator.configurations.RefreshConfiguration(configurator.layers.available_layers);
}

// The setting has been edited
void SettingsTreeManager::OnSettingChanged() {
    for (std::size_t i = 0, n = _presets_comboboxes.size(); i < n; ++i) {
        _presets_comboboxes[i]->UpdateCurrentIndex();
    }

    Configurator &configurator = Configurator::Get();
    configurator.environment.Notify(NOTIFICATION_RESTART);
    configurator.configurations.RefreshConfiguration(configurator.layers.available_layers);
}

void SettingsTreeManager::GetTreeState(QByteArray &byte_array, QTreeWidgetItem *top_item) {
    byte_array.push_back(top_item->isExpanded() ? '1' : '0');
    for (int i = 0, n = top_item->childCount(); i < n; ++i) {
        GetTreeState(byte_array, top_item->child(i));
    }
}

int SettingsTreeManager::SetTreeState(QByteArray &byte_array, int index, QTreeWidgetItem *top_item) {
    // We very well could run out, on initial run, expand everything
    if (index > byte_array.length())
        top_item->setExpanded(true);
    else {
        top_item->setExpanded(byte_array[index++] == '1');
    }
    // Walk the children
    if (top_item->childCount() != 0) {
        for (int i = 0, n = top_item->childCount(); i < n; ++i) {
            index = SetTreeState(byte_array, index, top_item->child(i));
        }
    }
    return index;
}
