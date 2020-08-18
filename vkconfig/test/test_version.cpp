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

#include "../version.h"

#include <string>

static int test_version_string() {
    const std::string version_a("1.1.130");
    const std::string version_b("1.2.145");

    int error = 0;

    error += Version(version_a.c_str()).str() == version_a ? 0 : 1;
    error += Version(version_b.c_str()).str() == version_b ? 0 : 1;

    return error;
}

static int test_version_compare() {
    const std::string version_a("1.1.130");
    const std::string version_b("1.2.135");
    const std::string version_c("1.2.145");

    int error = 0;

    error += version_a == version_a ? 0 : 1;
    error += version_b == version_b ? 0 : 1;
    error += version_c == version_c ? 0 : 1;

    error += version_a != version_b ? 0 : 1;
    error += version_b != version_c ? 0 : 1;
    error += version_a != version_c ? 0 : 1;

    error += version_a < version_b ? 0 : 1;
    error += version_b < version_c ? 0 : 1;
    error += version_a < version_c ? 0 : 1;

    error += version_a <= version_a ? 0 : 1;
    error += version_b <= version_b ? 0 : 1;
    error += version_c <= version_c ? 0 : 1;

    error += version_a <= version_b ? 0 : 1;
    error += version_b <= version_c ? 0 : 1;
    error += version_a <= version_c ? 0 : 1;

    error += version_b > version_a ? 0 : 1;
    error += version_c > version_b ? 0 : 1;
    error += version_c > version_a ? 0 : 1;

    error += version_a >= version_a ? 0 : 1;
    error += version_b >= version_b ? 0 : 1;
    error += version_c >= version_c ? 0 : 1;

    error += version_b >= version_a ? 0 : 1;
    error += version_c >= version_b ? 0 : 1;
    error += version_c >= version_a ? 0 : 1;

    return error;
}

int test_version() {
    int error = 0;

    error += test_version_string();
    error += test_version_compare();

    return error;
}

