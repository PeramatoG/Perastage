#include "dmx/gdtf_gobo_catalog.h"

#include "asset_cache.h"
#include "dmx/gdtf_physical_ranges.h"
#include "dmx/gdtf_xml_reader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <cmath>
#include <cstdlib>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace peraviz::dmx {

namespace {

FixtureGoboRangeBehavior parse_gobo_range_behavior(const std::string &channel_set_name) {
    const std::string lower_name = lower_ascii(channel_set_name);
    if (lower_name.find("shake") != std::string::npos) {
        return FixtureGoboRangeBehavior::kShake;
    }
    if (lower_name.find("posrotate") != std::string::npos ||
        lower_name.find("wheelspin") != std::string::npos ||
        lower_name.find("spin") != std::string::npos ||
        lower_name.find("rotation") != std::string::npos ||
        lower_name.find("rotate") != std::string::npos) {
        return FixtureGoboRangeBehavior::kRotation;
    }
    if (lower_name.find("index") != std::string::npos ||
        lower_name.find("pos") != std::string::npos) {
        return FixtureGoboRangeBehavior::kIndex;
    }
    return FixtureGoboRangeBehavior::kFixed;
}

bool is_select_shake_function(const std::string &function_name,
                              const std::string &function_attribute) {
    const std::string lowered_function_name = lower_ascii(function_name);
    if (lowered_function_name.find("selectshake") != std::string::npos) {
        return true;
    }
    const std::string lowered_function_attribute = lower_ascii(function_attribute);
    return lowered_function_attribute.find("selectshake") != std::string::npos;
}

int parse_positive_int(const char *raw) {
    if (!raw) {
        return -1;
    }

    std::string value = trim_ascii(raw);
    if (value.empty()) {
        return -1;
    }

    size_t length = 0;
    while (length < value.size() && std::isdigit(static_cast<unsigned char>(value[length])) != 0) {
        ++length;
    }
    if (length == 0) {
        return -1;
    }

    const long parsed = std::strtol(value.substr(0, length).c_str(), nullptr, 10);
    if (parsed <= 0L) {
        return -1;
    }
    return static_cast<int>(parsed);
}

std::string find_archive_media_by_stem(const std::string &gdtf_path,
                                       const std::string &media_reference) {
    const std::filesystem::path ref_path = std::filesystem::u8path(trim_ascii(media_reference));
    const std::string wanted_stem = lower_ascii(ref_path.stem().u8string());
    if (wanted_stem.empty()) {
        return {};
    }

    wxFileInputStream input(wxString::FromUTF8(gdtf_path.c_str()));
    if (!input.IsOk()) {
        return {};
    }

    std::string best_match;
    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        std::string entry_name = entry->GetName().ToUTF8().data();
        std::replace(entry_name.begin(), entry_name.end(), '\\', '/');
        const std::string entry_lower = lower_ascii(entry_name);
        const std::filesystem::path entry_path = std::filesystem::u8path(entry_name);
        const std::string entry_ext = lower_ascii(entry_path.extension().u8string());
        if (entry_ext != ".png" && entry_ext != ".jpg" && entry_ext != ".jpeg") {
            continue;
        }
        const std::string entry_stem = lower_ascii(entry_path.stem().u8string());
        if (entry_stem != wanted_stem) {
            continue;
        }

        const bool in_wheels_folder = entry_lower.find("wheels/") != std::string::npos;
        if (in_wheels_folder) {
            return entry_name;
        }
        if (best_match.empty()) {
            best_match = entry_name;
        }
    }

    return best_match;
}

std::string ensure_gobo_media_extracted(const std::string &gdtf_path,
                                        peraviz::ZipAssetCache &cache,
                                        const std::string &media_reference) {
    if (media_reference.empty()) {
        return {};
    }

    const std::string normalized = trim_ascii(media_reference);
    std::vector<std::string> candidates;
    candidates.push_back(normalized);
    candidates.push_back("wheels/" + normalized);
    candidates.push_back("wheels/images/" + normalized);

    const std::string lower = lower_ascii(normalized);
    const bool has_ext = lower.find('.') != std::string::npos;
    if (!has_ext) {
        candidates.push_back("wheels/" + normalized + ".png");
        candidates.push_back("wheels/" + normalized + ".jpg");
        candidates.push_back("wheels/" + normalized + ".jpeg");
        candidates.push_back("wheels/images/" + normalized + ".png");
        candidates.push_back("wheels/images/" + normalized + ".jpg");
        candidates.push_back("wheels/images/" + normalized + ".jpeg");
    }

    for (const std::string &candidate : candidates) {
        const std::string extracted = cache.ensure_extracted(candidate);
        if (!extracted.empty()) {
            return extracted;
        }
    }

    const std::string archive_match = find_archive_media_by_stem(gdtf_path, normalized);
    if (!archive_match.empty()) {
        return cache.ensure_extracted(archive_match);
    }

    return {};
}

int normalize_gobo_slot_index(int slot_index,
                              const std::unordered_set<int> &known_slots,
                              const std::vector<int> &known_slot_order) {
    if (slot_index <= 0) {
        return -1;
    }
    if (known_slots.find(slot_index) != known_slots.end()) {
        return slot_index;
    }

    if (known_slot_order.empty()) {
        return -1;
    }

    const int wrapped_index = (slot_index - 1) % static_cast<int>(known_slot_order.size());
    const int wrapped_slot = known_slot_order[wrapped_index];
    if (known_slots.find(wrapped_slot) != known_slots.end()) {
        return wrapped_slot;
    }
    return -1;
}

bool parse_float_attr_ci(tinyxml2::XMLElement *node,
                         const char *name_a,
                         const char *name_b,
                         float &out_value) {
    if (!node) {
        return false;
    }
    return node->QueryFloatAttribute(name_a, &out_value) == tinyxml2::XML_SUCCESS ||
           node->QueryFloatAttribute(name_b, &out_value) == tinyxml2::XML_SUCCESS;
}

tinyxml2::XMLElement *find_document_root_element(tinyxml2::XMLElement *node) {
    if (!node) {
        return nullptr;
    }
    tinyxml2::XMLNode *root = node;
    while (root->Parent() != nullptr) {
        root = root->Parent();
    }
    tinyxml2::XMLDocument *document = root->ToDocument();
    return document ? document->RootElement() : nullptr;
}

bool parse_amplitude_from_attribute_definition(tinyxml2::XMLElement *attribute_definition,
                                               float &out_amplitude_from_degrees,
                                               float &out_amplitude_to_degrees) {
    if (!attribute_definition) {
        return false;
    }

    for (tinyxml2::XMLElement *child = attribute_definition->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        if (lower_ascii(child->Name()) != "subphysicalunit") {
            continue;
        }
        if (lower_ascii(read_attr_ci(child, "Type", "type")) != "amplitude") {
            continue;
        }

        float raw_from = 0.0F;
        float raw_to = 0.0F;
        if (!parse_float_attr_ci(child, "PhysicalFrom", "physicalfrom", raw_from) ||
            !parse_float_attr_ci(child, "PhysicalTo", "physicalto", raw_to)) {
            continue;
        }

        const std::string physical_unit =
            lower_ascii(read_attr_ci(child, "PhysicalUnit", "physicalunit"));
        if (physical_unit == "percent") {
            out_amplitude_from_degrees = std::max(0.0F, raw_from) * 0.01F * 360.0F;
            out_amplitude_to_degrees = std::max(0.0F, raw_to) * 0.01F * 360.0F;
        } else {
            out_amplitude_from_degrees = std::max(0.0F, raw_from);
            out_amplitude_to_degrees = std::max(0.0F, raw_to);
        }
        return true;
    }

    return false;
}

} // namespace

GoboWheelCatalog build_gobo_wheel_catalog(const std::string &gdtf_path, tinyxml2::XMLElement *root) {
    GoboWheelCatalog out;
    if (!root) {
        return out;
    }

    peraviz::ZipAssetCache cache(gdtf_path);
    const std::vector<tinyxml2::XMLElement *> wheels = collect_elements_by_name(root, "wheel");
    for (tinyxml2::XMLElement *wheel : wheels) {
        const std::string wheel_name = lower_ascii(read_attr_ci(wheel, "Name", "name"));
        if (wheel_name.empty()) {
            continue;
        }

        int implicit_index = 1;
        for (tinyxml2::XMLElement *slot_node = wheel->FirstChildElement(); slot_node;
             slot_node = slot_node->NextSiblingElement()) {
            const std::string slot_tag = lower_ascii(slot_node->Name());
            if (slot_tag != "slot" && slot_tag != "wheelslot") {
                continue;
            }

            int slot_index = parse_positive_int(slot_node->Attribute("WheelSlotIndex"));
            if (slot_index <= 0) {
                slot_index = parse_positive_int(slot_node->Attribute("wheelslotindex"));
            }
            if (slot_index <= 0) {
                slot_index = implicit_index;
            }
            implicit_index = std::max(implicit_index, slot_index + 1);

            GoboWheelDefinition &wheel_definition = out[wheel_name];
            if (wheel_definition.declared_slots.insert(slot_index).second) {
                wheel_definition.declared_slot_order.push_back(slot_index);
            }

            const std::string media_file = read_attr_ci(slot_node, "MediaFileName", "mediafilename");
            if (media_file.empty()) {
                continue;
            }

            const std::string extracted = ensure_gobo_media_extracted(gdtf_path, cache, media_file);
            if (!extracted.empty()) {
                wheel_definition.slot_images[slot_index] = extracted;
            }
        }
    }

    return out;
}

bool resolve_shake_attribute_amplitude_degrees(tinyxml2::XMLElement *channel_function,
                                               float &out_amplitude_from_degrees,
                                               float &out_amplitude_to_degrees) {
    out_amplitude_from_degrees = 0.0F;
    out_amplitude_to_degrees = 0.0F;
    if (!channel_function) {
        return false;
    }

    const std::string attribute_name = read_attr_ci(channel_function, "Attribute", "attribute");
    if (attribute_name.empty()) {
        return false;
    }

    tinyxml2::XMLElement *root = find_document_root_element(channel_function);
    if (!root) {
        return false;
    }

    for (tinyxml2::XMLElement *attribute_definition : collect_elements_by_name(root, "attribute")) {
        if (!attribute_definition) {
            continue;
        }
        if (read_attr_ci(attribute_definition, "Name", "name") != attribute_name) {
            continue;
        }
        return parse_amplitude_from_attribute_definition(attribute_definition,
                                                         out_amplitude_from_degrees,
                                                         out_amplitude_to_degrees);
    }

    return false;
}

void consume_gobo_channel_sets(tinyxml2::XMLElement *channel_function,
                               const GoboWheelCatalog &wheel_catalog,
                               FixtureGoboWheelOffset &out_wheel,
                               int function_dmx_from,
                               int function_dmx_to,
                               int mode_window_from,
                               int mode_window_to) {
    if (!channel_function) {
        return;
    }

    const std::string wheel_name = lower_ascii(read_attr_ci(channel_function, "Wheel", "wheel"));
    if (wheel_name.empty()) {
        return;
    }

    auto wheel_it = wheel_catalog.find(wheel_name);
    if (wheel_it == wheel_catalog.end()) {
        return;
    }

    out_wheel.wheel_name = wheel_name;
    const std::string function_name = read_attr_ci(channel_function, "Name", "name");
    const std::string function_attribute = read_attr_ci(channel_function, "Attribute", "attribute");
    const bool function_forces_select_shake =
        is_select_shake_function(function_name, function_attribute);
    const FixtureGoboRangeBehavior function_behavior_hint =
        parse_gobo_range_behavior(function_name);

    const std::vector<int> &known_slot_order = wheel_it->second.declared_slot_order;
    std::unordered_set<int> known_slots = wheel_it->second.declared_slots;
    for (const auto &[slot_index, image_path] : wheel_it->second.slot_images) {
        if (slot_index <= 0 || image_path.empty()) {
            continue;
        }
        out_wheel.slots.push_back({slot_index, image_path});
    }

    struct ParsedGoboSet {
        int dmx_from = 0;
        int dmx_to = -1;
        int slot_index = -1;
        FixtureGoboRangeBehavior behavior = FixtureGoboRangeBehavior::kFixed;
        std::string name;
        bool has_physical = false;
        float physical_from = 0.0F;
        float physical_to = 0.0F;
    };
    std::vector<ParsedGoboSet> parsed_sets;

    const int clamped_function_from = std::clamp(function_dmx_from, 0, 255);
    const int clamped_function_to = std::clamp(function_dmx_to, clamped_function_from, 255);
    const int clamped_mode_from = std::clamp(mode_window_from, 0, 255);
    const int clamped_mode_to = std::clamp(mode_window_to, clamped_mode_from, 255);

    float function_physical_from = 0.0F;
    float function_physical_to = 0.0F;
    const bool has_function_physical_from =
        channel_function->QueryFloatAttribute("PhysicalFrom", &function_physical_from) == tinyxml2::XML_SUCCESS ||
        channel_function->QueryFloatAttribute("physicalfrom", &function_physical_from) == tinyxml2::XML_SUCCESS;
    const bool has_function_physical_to =
        channel_function->QueryFloatAttribute("PhysicalTo", &function_physical_to) == tinyxml2::XML_SUCCESS ||
        channel_function->QueryFloatAttribute("physicalto", &function_physical_to) == tinyxml2::XML_SUCCESS;
    const bool has_function_physical = has_function_physical_from && has_function_physical_to;
    float shake_attribute_amplitude_from_degrees = 0.0F;
    float shake_attribute_amplitude_to_degrees = 0.0F;
    const bool has_shake_attribute_amplitude = resolve_shake_attribute_amplitude_degrees(
        channel_function,
        shake_attribute_amplitude_from_degrees,
        shake_attribute_amplitude_to_degrees);

    const auto resolve_channel_set_dmx =
        [clamped_function_from, clamped_function_to](int raw_value) -> int {
        if (raw_value >= clamped_function_from && raw_value <= 255) {
            return std::clamp(raw_value, clamped_function_from, clamped_function_to);
        }
        const int relative_value = clamped_function_from + std::max(0, raw_value);
        return std::clamp(relative_value, clamped_function_from, clamped_function_to);
    };

    int last_slot_index = -1;
    if (!out_wheel.ranges.empty()) {
        last_slot_index = out_wheel.ranges.back().slot_index;
    }
    for (tinyxml2::XMLElement *channel_set = channel_function->FirstChildElement(); channel_set;
         channel_set = channel_set->NextSiblingElement()) {
        if (lower_ascii(channel_set->Name()) != "channelset") {
            continue;
        }

        int slot_index = parse_positive_int(channel_set->Attribute("WheelSlotIndex"));
        if (slot_index <= 0) {
            slot_index = parse_positive_int(channel_set->Attribute("wheelslotindex"));
        }
        if (slot_index <= 0) {
            slot_index = last_slot_index;
        }
        slot_index = normalize_gobo_slot_index(slot_index, known_slots, known_slot_order);
        if (slot_index <= 0) {
            continue;
        }
        last_slot_index = slot_index;

        int raw_dmx_from = parse_dmx_value_8bit(channel_set->Attribute("DMXFrom"));
        if (raw_dmx_from < 0) {
            raw_dmx_from = parse_dmx_value_8bit(channel_set->Attribute("dmxfrom"));
        }
        if (raw_dmx_from < 0) {
            raw_dmx_from = 0;
        }
        const int dmx_from = resolve_channel_set_dmx(raw_dmx_from);

        int raw_dmx_to = parse_dmx_value_8bit(channel_set->Attribute("DMXTo"));
        if (raw_dmx_to < 0) {
            raw_dmx_to = parse_dmx_value_8bit(channel_set->Attribute("dmxto"));
        }
        const int dmx_to = raw_dmx_to < 0 ? -1 : resolve_channel_set_dmx(raw_dmx_to);

        const std::string channel_set_name = read_attr_ci(channel_set, "Name", "name");
        FixtureGoboRangeBehavior behavior =
            parse_gobo_range_behavior(channel_set_name);
        if (function_forces_select_shake) {
            behavior = FixtureGoboRangeBehavior::kShake;
        } else if (behavior == FixtureGoboRangeBehavior::kFixed &&
                   function_behavior_hint != FixtureGoboRangeBehavior::kFixed) {
            behavior = function_behavior_hint;
        }
        float physical_from = 0.0F;
        float physical_to = 0.0F;
        const bool has_physical_from = channel_set->QueryFloatAttribute("PhysicalFrom", &physical_from) == tinyxml2::XML_SUCCESS ||
                                       channel_set->QueryFloatAttribute("physicalfrom", &physical_from) == tinyxml2::XML_SUCCESS;
        const bool has_physical_to = channel_set->QueryFloatAttribute("PhysicalTo", &physical_to) == tinyxml2::XML_SUCCESS ||
                                     channel_set->QueryFloatAttribute("physicalto", &physical_to) == tinyxml2::XML_SUCCESS;
        const bool has_physical = has_physical_from && has_physical_to;
        parsed_sets.push_back({dmx_from, dmx_to, slot_index, behavior, channel_set_name, has_physical, physical_from, physical_to});
    }

    if (parsed_sets.empty()) {
        return;
    }

    std::stable_sort(parsed_sets.begin(), parsed_sets.end(),
                     [](const ParsedGoboSet &a, const ParsedGoboSet &b) {
                         return a.dmx_from < b.dmx_from;
                     });

    for (size_t i = 0; i < parsed_sets.size(); ++i) {
        ParsedGoboSet &row = parsed_sets[i];
        if (row.dmx_to < 0) {
            if (i + 1 < parsed_sets.size()) {
                row.dmx_to = std::max(row.dmx_from, parsed_sets[i + 1].dmx_from - 1);
            } else {
                row.dmx_to = clamped_function_to;
            }
        }
        row.dmx_from = std::clamp(row.dmx_from, clamped_function_from, clamped_function_to);
        row.dmx_to = std::clamp(row.dmx_to, clamped_function_from, clamped_function_to);
        if (row.dmx_to < row.dmx_from) {
            std::swap(row.dmx_from, row.dmx_to);
        }

        out_wheel.ranges.push_back({row.dmx_from, row.dmx_to, clamped_mode_from, clamped_mode_to, row.slot_index, row.behavior});

        if (row.behavior == FixtureGoboRangeBehavior::kIndex) {
            out_wheel.supports_index = true;
        }

        if (row.behavior == FixtureGoboRangeBehavior::kRotation ||
            row.behavior == FixtureGoboRangeBehavior::kShake) {
            float rotation_physical_from = row.physical_from;
            float rotation_physical_to = row.physical_to;
            bool has_rotation_physical = row.has_physical;

            const std::string lower_name = lower_ascii(row.name);
            const bool is_named_stop = lower_name.find("no rotation") != std::string::npos ||
                                       lower_name.find("stop") != std::string::npos;
            if (!has_rotation_physical && is_named_stop) {
                rotation_physical_from = 0.0F;
                rotation_physical_to = 0.0F;
                has_rotation_physical = true;
            } else if (!has_rotation_physical && has_function_physical) {
                const float fn_range =
                    static_cast<float>(clamped_function_to - clamped_function_from);
                if (fn_range > 0.0F) {
                    const float t_from = std::clamp(
                        static_cast<float>(row.dmx_from - clamped_function_from) / fn_range,
                        0.0F, 1.0F);
                    const float t_to = std::clamp(
                        static_cast<float>(row.dmx_to - clamped_function_from) / fn_range,
                        0.0F, 1.0F);
                    rotation_physical_from = function_physical_from +
                        t_from * (function_physical_to - function_physical_from);
                    rotation_physical_to = function_physical_from +
                        t_to * (function_physical_to - function_physical_from);
                } else {
                    rotation_physical_from = function_physical_from;
                    rotation_physical_to = function_physical_to;
                }
                has_rotation_physical = true;
            }

            if (!has_rotation_physical &&
                infer_rotation_physical_from_name(row.name,
                                                  rotation_physical_from,
                                                  rotation_physical_to)) {
                has_rotation_physical = true;
            }

            if (has_rotation_physical) {
                if (row.behavior == FixtureGoboRangeBehavior::kShake) {
                    out_wheel.supports_shake = true;
                    FixtureGoboShakeRange shake_range;
                    shake_range.dmx_from = row.dmx_from;
                    shake_range.dmx_to = row.dmx_to;
                    shake_range.mode_from_8bit = clamped_mode_from;
                    shake_range.mode_to_8bit = clamped_mode_to;
                    shake_range.physical_from = rotation_physical_from;
                    shake_range.physical_to = rotation_physical_to;
                    shake_range.has_explicit_amplitude = has_shake_attribute_amplitude;
                    if (has_shake_attribute_amplitude) {
                        shake_range.amplitude_from_degrees = shake_attribute_amplitude_from_degrees;
                        shake_range.amplitude_to_degrees = shake_attribute_amplitude_to_degrees;
                    }
                    shake_range.slot_index = row.slot_index;
                    shake_range.control_type = FixtureGoboShakeControlType::kSameChannelSelect;
                    out_wheel.shake_ranges.push_back(shake_range);
                } else {
                    out_wheel.supports_rotation = true;
                    out_wheel.supports_spin_rotation = true;
                    FixtureGoboRotationRange rotation_range;
                    rotation_range.dmx_from = row.dmx_from;
                    rotation_range.dmx_to = row.dmx_to;
                    rotation_range.mode_from_8bit = clamped_mode_from;
                    rotation_range.mode_to_8bit = clamped_mode_to;
                    rotation_range.physical_from = rotation_physical_from;
                    rotation_range.physical_to = rotation_physical_to;
                    rotation_range.is_stop_range = std::fabs(rotation_physical_from) < 0.0001F && std::fabs(rotation_physical_to) < 0.0001F;
                    rotation_range.is_rotation_channel_range = false;
                    out_wheel.rotation_ranges.push_back(rotation_range);
                }
            }
        }
    }
}

void dedupe_and_sort_gobo_wheel(FixtureGoboWheelOffset &wheel) {
    std::sort(wheel.slots.begin(), wheel.slots.end(),
              [](const FixtureGoboSlot &a,
                 const FixtureGoboSlot &b) {
                  if (a.slot_index != b.slot_index) {
                      return a.slot_index < b.slot_index;
                  }
                  return a.image_path < b.image_path;
              });
    wheel.slots.erase(
        std::unique(wheel.slots.begin(), wheel.slots.end(),
                    [](const FixtureGoboSlot &a,
                       const FixtureGoboSlot &b) {
                        return a.slot_index == b.slot_index && a.image_path == b.image_path;
                    }),
        wheel.slots.end());

    std::stable_sort(wheel.ranges.begin(), wheel.ranges.end(),
                     [](const FixtureGoboRange &a,
                        const FixtureGoboRange &b) {
                         if (a.dmx_from != b.dmx_from) {
                             return a.dmx_from < b.dmx_from;
                         }
                         return a.dmx_to < b.dmx_to;
                     });
    wheel.ranges.erase(
        std::unique(wheel.ranges.begin(), wheel.ranges.end(),
                    [](const FixtureGoboRange &a,
                       const FixtureGoboRange &b) {
                        return a.dmx_from == b.dmx_from && a.dmx_to == b.dmx_to &&
                               a.mode_from_8bit == b.mode_from_8bit &&
                               a.mode_to_8bit == b.mode_to_8bit &&
                               a.slot_index == b.slot_index && a.behavior == b.behavior;
                    }),
        wheel.ranges.end());

    std::stable_sort(wheel.rotation_ranges.begin(), wheel.rotation_ranges.end(),
                     [](const FixtureGoboRotationRange &a,
                        const FixtureGoboRotationRange &b) {
                         if (a.dmx_from != b.dmx_from) {
                             return a.dmx_from < b.dmx_from;
                         }
                         return a.dmx_to < b.dmx_to;
                     });
    wheel.rotation_ranges.erase(
        std::unique(wheel.rotation_ranges.begin(), wheel.rotation_ranges.end(),
                    [](const FixtureGoboRotationRange &a,
                       const FixtureGoboRotationRange &b) {
                        return a.dmx_from == b.dmx_from && a.dmx_to == b.dmx_to &&
                               a.mode_from_8bit == b.mode_from_8bit &&
                               a.mode_to_8bit == b.mode_to_8bit &&
                               a.physical_from == b.physical_from &&
                               a.physical_to == b.physical_to &&
                               a.is_rotation_channel_range == b.is_rotation_channel_range;
                    }),
        wheel.rotation_ranges.end());

    std::stable_sort(wheel.shake_ranges.begin(), wheel.shake_ranges.end(),
                     [](const FixtureGoboShakeRange &a,
                        const FixtureGoboShakeRange &b) {
                         if (a.control_type != b.control_type) {
                             return a.control_type == FixtureGoboShakeControlType::kDedicatedShakeChannel;
                         }
                         if (a.dmx_from != b.dmx_from) {
                             return a.dmx_from < b.dmx_from;
                         }
                         return a.dmx_to < b.dmx_to;
                     });
    wheel.shake_ranges.erase(
        std::unique(wheel.shake_ranges.begin(), wheel.shake_ranges.end(),
                    [](const FixtureGoboShakeRange &a,
                       const FixtureGoboShakeRange &b) {
                        return a.dmx_from == b.dmx_from && a.dmx_to == b.dmx_to &&
                               a.mode_from_8bit == b.mode_from_8bit &&
                               a.mode_to_8bit == b.mode_to_8bit &&
                               a.physical_from == b.physical_from &&
                               a.physical_to == b.physical_to &&
                               a.has_explicit_amplitude == b.has_explicit_amplitude &&
                               a.amplitude_from_degrees == b.amplitude_from_degrees &&
                               a.amplitude_to_degrees == b.amplitude_to_degrees &&
                               a.slot_index == b.slot_index &&
                               a.control_type == b.control_type;
                    }),
        wheel.shake_ranges.end());
}

} // namespace peraviz::dmx
