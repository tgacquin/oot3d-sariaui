// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/randomizer/data.h"
#include <atomic>
#include <vector>
class QSettings;

namespace OOT3D::Randomizer::Presets {
struct Entry {
    std::string name,description;
    Config config;
    bool built_in=false;
};
// Seed presets are separate from authored presentation and current save rules.
const std::vector<Entry>& BuiltIns();
Config Apply(const Entry& preset,const Config& current);
std::vector<Entry> List();
std::string SaveNew(std::string name,const Config& config);
std::string Update(const std::string& name,const Config& config);
std::string Remove(const std::string& name);
void Load(QSettings& settings);
void Save(QSettings& settings);
std::string StorageError();
inline std::atomic<bool> dirty{false};
} // namespace OOT3D::Randomizer::Presets
