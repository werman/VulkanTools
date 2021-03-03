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

#pragma once

#include "widget_setting.h"

#include <QResizeEvent>
#include <QLineEdit>
#include <QPushButton>

class WidgetSettingFilesystem : public WidgetSetting {
    Q_OBJECT

   public:
    explicit WidgetSettingFilesystem(QTreeWidget *tree, QTreeWidgetItem *parent, const SettingMetaFilesystem &setting_meta,
                                     SettingDataString &setting_data);

   public Q_SLOTS:
    void OnButtonClicked();
    void OnTextEdited(const QString &value);
    void OnItemExpanded(QTreeWidgetItem *expanded_item);
    void OnItemCollapsed(QTreeWidgetItem *expanded_item);

   Q_SIGNALS:
    void itemChanged();

   private:
    WidgetSettingFilesystem(const WidgetSettingFilesystem &) = delete;
    WidgetSettingFilesystem &operator=(const WidgetSettingFilesystem &) = delete;

    virtual void resizeEvent(QResizeEvent *event) override;

    const SettingMetaFilesystem &setting_meta;
    SettingDataString &setting_data;
    QPushButton *button;
    QLineEdit *title_field;
    QLineEdit *child_field;
    QTreeWidgetItem *child_item;
};
