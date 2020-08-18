/*
 * Copyright (c) 2020 Valve Corporation
 * Copyright (c) 2020 LunarG, Inc.
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
 * - Richard S. Wright Jr.
 * - Christophe Riccio
 */

#pragma once

#include "layer.h"

#include <QString>
#include <QStringList>

#include <vector>

// json file preset_index must match the preset enum values
enum ValidationPreset {
    VALIDATION_PRESET_NONE = 0,
    VALIDATION_PRESET_USER_DEFINED = VALIDATION_PRESET_NONE,
    VALIDATION_PRESET_STANDARD = 1,
    VALIDATION_PRESET_GPU_ASSISTED = 2,
    VALIDATION_PRESET_SHADER_PRINTF = 3,
    VALIDATION_PRESET_REDUCED_OVERHEAD = 4,
    VALIDATION_PRESET_BEST_PRACTICES = 5,
    VALIDATION_PRESET_SYNCHRONIZATION = 6,

    VALIDATION_PRESET_FIRST = VALIDATION_PRESET_USER_DEFINED,
    VALIDATION_PRESET_LAST = VALIDATION_PRESET_SYNCHRONIZATION
};

enum { VALIDATION_PRESET_COUNT = VALIDATION_PRESET_LAST - VALIDATION_PRESET_FIRST + 1 };

class Configuration {
   public:
    Configuration();

    QString _name;                   // User readable display of the profile name (may contain spaces)
                                     // This is the same as the filename, but with the .json stripped off.
    QString _file;                   // Root file name without path (by convention, no spaces and .profile suffix)
    QString _description;            // A friendly description of what this profile does
    QByteArray _setting_tree_state;  // Recall editor tree state
    ValidationPreset _preset;        // Khronos layer presets. 0 = none or user defined

    // A configuration is nothing but a list of layers and their settings in truth
    std::vector<Layer> _overridden_layers;

    QStringList _excluded_layers;  // Just the names of blacklisted layers

    Layer &CreateOverriddenLayer(const Layer &layer);

    bool IsOverriddenLayerAvailable(const QString &layer_name, const QString &full_path) const;  // Find the layer if it exists
    Layer *FindOverriddenLayer(const QString &layer_name);  // Find the layer if it exists, only care about the name

    Configuration *DuplicateConfiguration();  // Copy a profile so we can mess with it

    void CollapseConfiguration();  // Remove unused layers and settings, set exclusion list

    bool IsValid() const;

   private:
};

int test_configuration();
