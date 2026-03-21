/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>

#include "types.h"
#include "fixture.h"

// Represents a hoist parsed from MVR
struct Support {
    std::string uuid;
    std::string name;
    std::string gdtfSpec;
    std::string gdtfMode;
    std::string function;
    float chainLength = 0.0f;
    std::string position;
    std::string positionName;
    std::string layer;

    float capacityKg = 0.0f;
    float weightKg = 0.0f;
    float loadKg = 0.0f;
    std::string motorName;
    std::string motorManufacturer;
    std::string motorModel;
    // Stable fixture UUID linked to this hoist motor metadata.
    std::string motorFixtureUuid;
    // Enables inheriting missing values from linked fixture/preset defaults.
    bool useMotorDefaults = true;
    // Stable ID for a dummy hoist profile from library/hoists/dummy_profiles.json.
    std::string dummyProfileId;
    // Legacy display label kept for backward compatibility with older files.
    std::string dummyPreset;
    // Defines whether values come from library defaults or user overrides.
    std::string hoistDataSource = "Inherited";
    std::string motorNameSource = "Inherited";
    std::string motorManufacturerSource = "Inherited";
    std::string motorModelSource = "Inherited";
    std::string capacitySource = "Inherited";
    std::string weightSource = "Inherited";
    std::string hoistFunctionSource = "Inherited";
    // Hoist function/category. Defaults to values returned by GetHoistFunctionOptions().
    std::string hoistFunction = "Lighting";

    Matrix transform;
};

inline const std::array<std::string, 5> &GetHoistFunctionOptions() {
    static const std::array<std::string, 5> options = {
        "Lighting", "Audio", "Video", "Scenic", "Extra"};
    return options;
}

inline std::string NormalizeHoistFunction(const std::string &rawValue) {
    auto trimmed = rawValue;
    auto trimSpaces = [](std::string &s) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    };
    trimSpaces(trimmed);

    if (trimmed.empty())
        return "Lighting";

    double parsed = 0.0;
    auto begin = trimmed.data();
    auto end = trimmed.data() + trimmed.size();
    auto result = std::from_chars(begin, end, parsed);
    if (result.ec == std::errc{} && result.ptr == end) {
        if (parsed == 0.0)
            return "Lighting";
    }

    auto lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto &opt : GetHoistFunctionOptions()) {
        auto optLower = opt;
        std::transform(optLower.begin(), optLower.end(), optLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == optLower)
            return opt;
    }

    return trimmed;
}

inline std::string NormalizeHoistDataSource(const std::string &rawValue) {
    auto trimmed = rawValue;
    auto trimSpaces = [](std::string &s) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    };
    trimSpaces(trimmed);
    if (trimmed.empty())
        return "Inherited";

    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (trimmed == "manual")
        return "Manual";
    if (trimmed == "inherited")
        return "Inherited";
    return "Inherited";
}

inline bool IsManualHoistDataSource(const std::string &source) {
    return NormalizeHoistDataSource(source) == "Manual";
}

inline std::string ResolveHoistFieldDataSource(const std::string &fieldSource,
                                               const std::string &globalSource) {
    const std::string normalizedField = NormalizeHoistDataSource(fieldSource);
    if (!fieldSource.empty())
        return normalizedField;
    return NormalizeHoistDataSource(globalSource);
}

struct HoistPresetDefaults {
    std::string motorName;
    std::string motorManufacturer;
    std::string motorModel;
    float capacityKg = 0.0f;
    float weightKg = 0.0f;
    std::string hoistFunction;
};

struct HoistFixtureDefaults {
    std::string motorName;
    std::string motorManufacturer;
    std::string motorModel;
    float capacityKg = 0.0f;
    float weightKg = 0.0f;
    std::string hoistFunction;
};

struct EffectiveSupportData {
    std::string motorName;
    std::string motorManufacturer;
    std::string motorModel;
    float capacityKg = 0.0f;
    float weightKg = 0.0f;
    std::string hoistFunction = "Lighting";
    std::string dataSource = "Inherited";
    std::string motorNameSource = "Inherited";
    std::string motorManufacturerSource = "Inherited";
    std::string motorModelSource = "Inherited";
    std::string capacitySource = "Inherited";
    std::string weightSource = "Inherited";
    std::string hoistFunctionSource = "Inherited";
};



inline HoistFixtureDefaults BuildHoistFixtureDefaults(const Fixture &fixture) {
    HoistFixtureDefaults defaults;
    defaults.motorName = fixture.typeName;
    defaults.motorManufacturer = "";
    defaults.motorModel = fixture.gdtfMode;
    defaults.capacityKg = 0.0f;
    defaults.weightKg = fixture.weightKg;
    defaults.hoistFunction = NormalizeHoistFunction(fixture.function);
    return defaults;
}

inline bool HasManualNumeric(float value) { return value > 0.0f; }

inline bool HasManualText(const std::string &value) { return !value.empty(); }

inline EffectiveSupportData ResolveEffectiveSupportData(
    const Support &support,
    const std::optional<HoistPresetDefaults> &presetDefaults = std::nullopt,
    const std::optional<HoistFixtureDefaults> &fixtureDefaults = std::nullopt) {
    EffectiveSupportData resolved;
    const std::string normalizedSource = NormalizeHoistDataSource(support.hoistDataSource);
    resolved.dataSource = normalizedSource;
    resolved.motorNameSource =
        ResolveHoistFieldDataSource(support.motorNameSource, normalizedSource);
    resolved.motorManufacturerSource = ResolveHoistFieldDataSource(
        support.motorManufacturerSource, normalizedSource);
    resolved.motorModelSource =
        ResolveHoistFieldDataSource(support.motorModelSource, normalizedSource);
    resolved.capacitySource =
        ResolveHoistFieldDataSource(support.capacitySource, normalizedSource);
    resolved.weightSource =
        ResolveHoistFieldDataSource(support.weightSource, normalizedSource);
    resolved.hoistFunctionSource = ResolveHoistFieldDataSource(
        support.hoistFunctionSource, normalizedSource);

    resolved.motorName = support.motorName;
    resolved.motorManufacturer = support.motorManufacturer;
    resolved.motorModel = support.motorModel;
    resolved.capacityKg = support.capacityKg;
    resolved.weightKg = support.weightKg;
    resolved.hoistFunction = NormalizeHoistFunction(
        support.hoistFunction.empty() ? support.function : support.hoistFunction);

    if (!support.useMotorDefaults)
        return resolved;

    auto applyDefaults = [&](const auto &defaults, const char *sourceLabel) {
        if (!IsManualHoistDataSource(resolved.motorNameSource) &&
            !HasManualText(resolved.motorName) && HasManualText(defaults.motorName)) {
            resolved.motorName = defaults.motorName;
            resolved.motorNameSource = sourceLabel;
        }
        if (!IsManualHoistDataSource(resolved.motorManufacturerSource) &&
            !HasManualText(resolved.motorManufacturer) &&
            HasManualText(defaults.motorManufacturer)) {
            resolved.motorManufacturer = defaults.motorManufacturer;
            resolved.motorManufacturerSource = sourceLabel;
        }
        if (!IsManualHoistDataSource(resolved.motorModelSource) &&
            !HasManualText(resolved.motorModel) && HasManualText(defaults.motorModel)) {
            resolved.motorModel = defaults.motorModel;
            resolved.motorModelSource = sourceLabel;
        }
        if (!IsManualHoistDataSource(resolved.capacitySource) &&
            !HasManualNumeric(resolved.capacityKg) &&
            HasManualNumeric(defaults.capacityKg)) {
            resolved.capacityKg = defaults.capacityKg;
            resolved.capacitySource = sourceLabel;
        }
        if (!IsManualHoistDataSource(resolved.weightSource) &&
            !HasManualNumeric(resolved.weightKg) && HasManualNumeric(defaults.weightKg)) {
            resolved.weightKg = defaults.weightKg;
            resolved.weightSource = sourceLabel;
        }
        if (!IsManualHoistDataSource(resolved.hoistFunctionSource) &&
            NormalizeHoistFunction(resolved.hoistFunction) == "Lighting" &&
            HasManualText(defaults.hoistFunction)) {
            resolved.hoistFunction = NormalizeHoistFunction(defaults.hoistFunction);
            resolved.hoistFunctionSource = sourceLabel;
        }
        if (resolved.motorNameSource == sourceLabel ||
            resolved.motorManufacturerSource == sourceLabel ||
            resolved.motorModelSource == sourceLabel ||
            resolved.capacitySource == sourceLabel ||
            resolved.weightSource == sourceLabel ||
            resolved.hoistFunctionSource == sourceLabel) {
            resolved.dataSource = sourceLabel;
        }
    };

    if (fixtureDefaults.has_value())
        applyDefaults(*fixtureDefaults, "Fixture");

    if (presetDefaults.has_value())
        applyDefaults(*presetDefaults, "Inherited");

    return resolved;
}
