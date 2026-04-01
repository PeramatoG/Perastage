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
#include "gdtfloader.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "matrixutils.h"
#include "meshprimitives.h"
#include "consolepanel.h"

#include <cctype>
#include <charconv>
#include <string_view>
#include <tinyxml2.h>
#include <wx/wx.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>
#include <wx/filename.h>

#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <random>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <memory>
#include <cfloat>
#include <cstdint>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <mutex>

namespace fs = std::filesystem;

namespace {
bool TryParseFloat(const std::string& text, float& out)
{
    if (text.empty())
        return false;

    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    if (first == text.end())
        return false;
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    std::string_view trimmed(&(*first), static_cast<size_t>(last - first));

    float value = 0.0f;
    auto begin = trimmed.data();
    auto end = trimmed.data() + trimmed.size();
    auto result = std::from_chars(begin, end, value);
    if (result.ec == std::errc{} && result.ptr == end) {
        out = value;
        return true;
    }
    return false;
}

bool TryParseInt(std::string_view text, int& out)
{
    if (text.empty())
        return false;

    const auto first = std::find_if_not(text.begin(), text.end(),
                                        [](unsigned char c) { return std::isspace(c); });
    if (first == text.end())
        return false;
    const auto last = std::find_if_not(text.rbegin(), text.rend(),
                                       [](unsigned char c) { return std::isspace(c); }).base();
    std::string_view trimmed(&(*first), static_cast<size_t>(last - first));

    int value = 0;
    auto begin = trimmed.data();
    auto end = trimmed.data() + trimmed.size();
    auto result = std::from_chars(begin, end, value);
    if (result.ec == std::errc{} && result.ptr == end) {
        out = value;
        return true;
    }
    return false;
}

bool IsBlank(std::string_view text)
{
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
    });
}

bool ContainsTokenInsensitive(const std::string& text, std::string_view token)
{
    if (text.empty() || token.empty())
        return false;

    std::string lowerText = text;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string lowerToken(token);
    std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowerText.find(lowerToken) != std::string::npos;
}

bool IsBeamLikeNodeName(const std::string& nodeName)
{
    if (nodeName.empty())
        return false;

    std::string lower = nodeName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "beam" || lower.find("beam") != std::string::npos;
}

bool IsLikelyLensGeometry(tinyxml2::XMLElement* node,
                          const std::unordered_map<std::string, GdtfModelInfo>& models,
                          const char* modelName,
                          bool parentIsLens)
{
    if (!node)
        return false;

    if (parentIsLens)
        return true;

    if (IsBeamLikeNodeName(node->Name()))
        return true;

    if (const char* geometryName = node->Attribute("Name")) {
        if (ContainsTokenInsensitive(geometryName, "lens") ||
            ContainsTokenInsensitive(geometryName, "optic") ||
            ContainsTokenInsensitive(geometryName, "glass")) {
            return true;
        }
    }

    if (!modelName)
        return false;

    std::string model(modelName);
    if (ContainsTokenInsensitive(model, "lens") ||
        ContainsTokenInsensitive(model, "optic") ||
        ContainsTokenInsensitive(model, "glass")) {
        return true;
    }

    auto modelIt = models.find(model);
    if (modelIt == models.end())
        return false;

    const std::string& modelFile = modelIt->second.file;
    return ContainsTokenInsensitive(modelFile, "lens") ||
           ContainsTokenInsensitive(modelFile, "optic") ||
           ContainsTokenInsensitive(modelFile, "glass");
}
} // namespace

struct GdtfCacheEntry
{
    fs::file_time_type timestamp;
    std::string extractedDir;
    std::unique_ptr<tinyxml2::XMLDocument> doc;
    tinyxml2::XMLElement* fixtureType = nullptr;
    std::vector<std::string> modes;
    std::unordered_map<std::string, std::vector<GdtfChannelInfo>> modeChannels;
    std::unordered_map<std::string, int> modeChannelCounts;
    std::unordered_map<std::string, Mesh> meshCache;
    std::string fixtureName;
    bool propertiesParsed = false;
    float weightKg = 0.0f;
    float powerW = 0.0f;
    bool modelColorParsed = false;
    std::string modelColor;
    std::unordered_set<std::string> missingModelsLogged;
    std::unordered_set<std::string> failedModelLoads;
    std::unordered_set<std::string> emptyModelFileLogged;
    size_t emptyGeometryLogCount = 0;
};

static std::unordered_map<std::string, GdtfCacheEntry> g_gdtfCache;
static std::unordered_map<std::string, fs::file_time_type> g_failedGdtfCache;
static std::unordered_map<std::string, size_t> g_gdtfFailedAttempts;
static std::unordered_map<std::string, std::string> g_gdtfFailureReasons;
static std::recursive_mutex g_gdtfCacheMutex;

struct MissingModelLog
{
    size_t count = 0;
    std::string file;
    std::string baseDir;
};

static bool ExtractZip(const std::string& zipPath, const std::string& destDir);

static std::string ToLower(const std::string& s)
{
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return std::tolower(c); });
    return t;
}

static bool HasExtension(const fs::path& p, const std::string& ext)
{
    return ToLower(p.extension().string()) == ToLower(ext);
}

static bool IsPrimitiveTypeDefined(const std::string& primitiveType)
{
    if (primitiveType.empty())
        return false;
    return ToLower(primitiveType) != "undefined";
}

static void ApplyModelDimensions(Mesh& mesh, const GdtfModelInfo& modelInfo)
{
    if (mesh.vertices.empty())
        return;

    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
    for (size_t vi = 0; vi + 2 < mesh.vertices.size(); vi += 3) {
        float x = mesh.vertices[vi];
        float y = mesh.vertices[vi + 1];
        float z = mesh.vertices[vi + 2];
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
        minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
    }

    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    float targetX = modelInfo.length * 1000.0f; // meters -> mm
    float targetY = modelInfo.width  * 1000.0f;
    float targetZ = modelInfo.height * 1000.0f;
    float sx = (targetX > 0.0f && sizeX > 0.0f) ? targetX / sizeX : 1.0f;
    float sy = (targetY > 0.0f && sizeY > 0.0f) ? targetY / sizeY : 1.0f;
    float sz = (targetZ > 0.0f && sizeZ > 0.0f) ? targetZ / sizeZ : 1.0f;
    if (sx != 1.0f || sy != 1.0f || sz != 1.0f) {
        for (size_t vi = 0; vi + 2 < mesh.vertices.size(); vi += 3) {
            mesh.vertices[vi]     *= sx;
            mesh.vertices[vi + 1] *= sy;
            mesh.vertices[vi + 2] *= sz;
        }
    }
}

static std::string FindModelFile(const std::string& baseDir,
                                 const std::string& fileName)
{
    fs::path modelsDir = fs::path(baseDir) / "models";
    if (!fs::exists(modelsDir))
        return {};

    fs::path namePath = fileName;
    std::string stem = namePath.stem().string();
    std::string ext = ToLower(namePath.extension().string());

    auto tryExt = [&](const std::string& e) -> std::string {
        fs::path d = modelsDir / e.substr(1) / (stem + e);
        if(fs::exists(d))
            return d.string();
        return {};
    };

    if(!ext.empty()) {
        std::string res = tryExt(ext);
        if(!res.empty()) return res;
    } else {
        std::string res = tryExt(".3ds");
        if(!res.empty()) return res;
        res = tryExt(".glb");
        if(!res.empty()) return res;
    }

    for (auto& p : fs::recursive_directory_iterator(modelsDir)) {
        if (!p.is_regular_file())
            continue;
        if (p.path().stem() != stem)
            continue;
        if(ext.empty()) {
            if(HasExtension(p.path(), ".3ds") || HasExtension(p.path(), ".glb"))
                return p.path().string();
        } else if(HasExtension(p.path(), ext)) {
            return p.path().string();
        }
    }
    return {};
}

static std::string CreateTempDir()
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string folderName = "GDTF_" + std::to_string(now);
    fs::path base = fs::temp_directory_path();
    fs::path full = base / folderName;
    fs::create_directory(full);
    return full.string();
}

struct TempExtraction
{
    explicit TempExtraction(const std::string& zipPath)
    {
        dir = CreateTempDir();
        extracted = ExtractZip(zipPath, dir);
    }

    ~TempExtraction()
    {
        if (!dir.empty()) {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    }

    bool IsValid() const { return extracted; }

    std::string Release()
    {
        std::string out = std::move(dir);
        dir.clear();
        return out;
    }

    const std::string& Path() const { return dir; }

private:
    std::string dir;
    bool extracted = false;
};

static bool ExtractZip(const std::string& zipPath, const std::string& destDir)
{
    if (!fs::exists(zipPath)) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("GDTF: cannot open %s", wxString::FromUTF8(zipPath));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
        return false;
    }
    wxLogNull logNo;
    wxFileInputStream input(zipPath);
    if (!input.IsOk()) {
        if (ConsolePanel::Instance()) {
            wxString msg = wxString::Format("GDTF: cannot open %s", wxString::FromUTF8(zipPath));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
        return false;
    }
    wxZipInputStream zipStream(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zipStream.GetNextEntry())), entry) {
        std::string filename = entry->GetName().ToStdString();
        std::string fullPath = destDir + "/" + filename;
        
        if (entry->IsDir()) {
            wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            continue;
        }
        wxFileName::Mkdir(wxFileName(fullPath).GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        std::ofstream output(fullPath, std::ios::binary);
        if (!output.is_open()) {
            if (ConsolePanel::Instance()) {
                wxString msg = wxString::Format("GDTF: cannot create %s", wxString::FromUTF8(fullPath));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
            return false;
        }
        char buffer[4096];
        while (true) {
            zipStream.Read(buffer, sizeof(buffer));
            size_t bytes = zipStream.LastRead();
            if (bytes == 0)
                break;
            output.write(buffer, bytes);
        }
        output.close();
    }
    return true;
}

static tinyxml2::XMLElement* GetFixtureType(tinyxml2::XMLDocument& doc)
{
    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft)
        ft = ft->FirstChildElement("FixtureType");
    else
        ft = doc.FirstChildElement("FixtureType");
    return ft;
}

static std::string GetFixtureNameFromXml(tinyxml2::XMLElement* ft)
{
    if (!ft)
        return {};
    if (const char* nameAttr = ft->Attribute("Name"))
        return nameAttr;
    if (const char* shortName = ft->Attribute("ShortName"))
        return shortName;
    if (const char* longName = ft->Attribute("LongName"))
        return longName;
    return {};
}

static void ParseModes(tinyxml2::XMLElement* ft,
                      std::vector<std::string>& modes,
                      std::unordered_map<std::string, std::vector<GdtfChannelInfo>>& modeChannels,
                      std::unordered_map<std::string, int>& modeChannelCounts)
{
    modes.clear();
    modeChannels.clear();
    modeChannelCounts.clear();
    if (!ft)
        return;
    tinyxml2::XMLElement* modesNode = ft->FirstChildElement("DMXModes");
    if (!modesNode)
        return;

    for (tinyxml2::XMLElement* m = modesNode->FirstChildElement("DMXMode");
         m; m = m->NextSiblingElement("DMXMode"))
    {
        const char* name = m->Attribute("Name");
        if (!name)
            continue;
        modes.push_back(name);

        std::vector<GdtfChannelInfo> channelsVec;
        int count = 0;
        if (tinyxml2::XMLElement* channels = m->FirstChildElement("DMXChannels")) {
            auto trim = [](std::string& value) {
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
            };
            auto parseOffsets = [&](const char* rawOffset) {
                std::vector<int> parsedOffsets;
                if (!rawOffset)
                    return parsedOffsets;
                std::string offStr = rawOffset;
                trim(offStr);
                if (offStr.empty() || offStr == "None")
                    return parsedOffsets;
                std::stringstream ss(offStr);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    trim(token);
                    if (token.empty() || token == "None")
                        continue;
                    int parsed = 0;
                    if (TryParseInt(token, parsed)) {
                        parsedOffsets.push_back(parsed);
                    } else if (ConsolePanel::Instance()) {
                        wxString msg = wxString::Format(
                            "GDTF: invalid DMX channel offset '%s'",
                            wxString::FromUTF8(token));
                        ConsolePanel::Instance()->AppendMessage(msg);
                    }
                }
                return parsedOffsets;
            };
            auto extractFunctionName = [](tinyxml2::XMLElement* dmxChannel) {
                if (!dmxChannel)
                    return std::string{};
                for (tinyxml2::XMLElement* lc = dmxChannel->FirstChildElement("LogicalChannel");
                     lc; lc = lc->NextSiblingElement("LogicalChannel")) {
                    for (tinyxml2::XMLElement* cf = lc->FirstChildElement("ChannelFunction");
                         cf; cf = cf->NextSiblingElement("ChannelFunction")) {
                        const char* attr = cf->Attribute("Attribute");
                        if (!attr)
                            attr = cf->Attribute("attribute");
                        if (attr && *attr)
                            return std::string(attr);
                    }
                    const char* attr = lc->Attribute("Attribute");
                    if (!attr)
                        attr = lc->Attribute("attribute");
                    if (attr && *attr)
                        return std::string(attr);
                }
                return std::string{};
            };
            auto parseDmxEndpointValue = [&](const char* rawValue) {
                if (!rawValue || !*rawValue)
                    return std::string{};
                std::string value = rawValue;
                trim(value);
                if (value.empty())
                    return std::string{};
                const size_t separatorPos = value.find_first_of("/.");
                if (separatorPos != std::string::npos)
                    value = value.substr(0, separatorPos);
                int parsed = 0;
                if (!TryParseInt(value, parsed))
                    return std::string{};
                return std::to_string(parsed);
            };
            auto parseChannelSetsSummary =
                [&](tinyxml2::XMLElement* channelFunction) {
                    std::vector<std::string> channelSets;
                    if (!channelFunction)
                        return channelSets;
                    for (tinyxml2::XMLElement* channelSet = channelFunction->FirstChildElement("ChannelSet");
                         channelSet;
                         channelSet = channelSet->NextSiblingElement("ChannelSet")) {
                        std::string label;
                        if (const char* name = channelSet->Attribute("Name"))
                            label = name;
                        if (label.empty()) {
                            if (const char* wheelSlot = channelSet->Attribute("WheelSlotIndex")) {
                                label = "WheelSlot ";
                                label += wheelSlot;
                            }
                        }
                        const std::string dmxFrom =
                            parseDmxEndpointValue(channelSet->Attribute("DMXFrom"));
                        if (!dmxFrom.empty()) {
                            if (!label.empty())
                                label += "@";
                            label += dmxFrom;
                        }
                        if (label.empty())
                            label = "unnamed";
                        channelSets.push_back(std::move(label));
                    }
                    return channelSets;
                };
            auto tryResolveModeMasterReference =
                [&](const std::unordered_map<std::string, std::string>& modeMasterNameByReference,
                    const std::string& reference) {
                    if (reference.empty())
                        return std::string{};
                    auto it = modeMasterNameByReference.find(reference);
                    if (it == modeMasterNameByReference.end())
                        return reference;
                    return it->second;
                };
            auto buildFunctionSummary =
                [&](tinyxml2::XMLElement* dmxChannel) {
                    if (!dmxChannel)
                        return std::string{};

                    std::unordered_map<std::string, std::string> modeMasterNameByReference;
                    for (tinyxml2::XMLElement* lc = dmxChannel->FirstChildElement("LogicalChannel");
                         lc; lc = lc->NextSiblingElement("LogicalChannel")) {
                        for (tinyxml2::XMLElement* cf = lc->FirstChildElement("ChannelFunction");
                             cf; cf = cf->NextSiblingElement("ChannelFunction")) {
                            const char* name = cf->Attribute("Name");
                            const char* attr = cf->Attribute("Attribute");
                            if (!attr)
                                attr = cf->Attribute("attribute");
                            if (name && *name && attr && *attr)
                                modeMasterNameByReference[name] = attr;
                            if (attr && *attr)
                                modeMasterNameByReference[attr] = attr;
                        }
                    }

                    std::vector<std::string> summaries;
                    for (tinyxml2::XMLElement* lc = dmxChannel->FirstChildElement("LogicalChannel");
                         lc; lc = lc->NextSiblingElement("LogicalChannel")) {
                        std::string logicalAttribute;
                        if (const char* attr = lc->Attribute("Attribute"))
                            logicalAttribute = attr;
                        if (logicalAttribute.empty()) {
                            if (const char* attr = lc->Attribute("attribute"))
                                logicalAttribute = attr;
                        }

                        bool logicalChannelHadFunction = false;
                        for (tinyxml2::XMLElement* cf = lc->FirstChildElement("ChannelFunction");
                             cf; cf = cf->NextSiblingElement("ChannelFunction")) {
                            logicalChannelHadFunction = true;
                            std::string attribute = logicalAttribute;
                            if (const char* attr = cf->Attribute("Attribute"))
                                attribute = attr;
                            if (attribute.empty()) {
                                if (const char* attr = cf->Attribute("attribute"))
                                    attribute = attr;
                            }
                            std::string summary = attribute;
                            if (summary.empty()) {
                                if (const char* name = cf->Attribute("Name"))
                                    summary = name;
                            }
                            if (summary.empty())
                                summary = "Function";

                            const std::vector<std::string> channelSets =
                                parseChannelSetsSummary(cf);
                            if (!channelSets.empty()) {
                                summary += " [sets: ";
                                for (size_t index = 0; index < channelSets.size(); ++index) {
                                    if (index > 0)
                                        summary += ", ";
                                    summary += channelSets[index];
                                }
                                summary += "]";
                            }

                            const char* modeMasterRaw = cf->Attribute("ModeMaster");
                            if (modeMasterRaw && *modeMasterRaw) {
                                const std::string modeMaster =
                                    tryResolveModeMasterReference(modeMasterNameByReference,
                                                                  modeMasterRaw);
                                std::string range;
                                const std::string modeFrom =
                                    parseDmxEndpointValue(cf->Attribute("ModeFrom"));
                                const std::string modeTo =
                                    parseDmxEndpointValue(cf->Attribute("ModeTo"));
                                if (!modeFrom.empty() || !modeTo.empty()) {
                                    range = " ";
                                    range += modeFrom.empty() ? "?" : modeFrom;
                                    range += "-";
                                    range += modeTo.empty() ? "?" : modeTo;
                                }
                                summary += " [mode master: ";
                                summary += modeMaster.empty() ? std::string(modeMasterRaw) : modeMaster;
                                summary += range;
                                summary += "]";
                            }

                            summaries.push_back(std::move(summary));
                        }

                        if (!logicalChannelHadFunction && !logicalAttribute.empty())
                            summaries.push_back(std::move(logicalAttribute));
                    }

                    if (summaries.empty())
                        return extractFunctionName(dmxChannel);

                    std::string merged;
                    for (size_t index = 0; index < summaries.size(); ++index) {
                        if (index > 0)
                            merged += " | ";
                        merged += summaries[index];
                    }
                    return merged;
                };
            auto suffixForByte = [](size_t byteIndex) {
                if (byteIndex == 1)
                    return std::string(" (fine)");
                if (byteIndex == 2)
                    return std::string(" (ultra fine)");
                return wxString::Format(" (byte %zu)", byteIndex + 1).ToStdString();
            };

            for (tinyxml2::XMLElement* c = channels->FirstChildElement("DMXChannel");
                 c; c = c->NextSiblingElement("DMXChannel"))
            {
                const std::vector<int> offsets = parseOffsets(c->Attribute("Offset"));
                const std::string baseFunction = buildFunctionSummary(c);

                if (!offsets.empty()) {
                    for (size_t i = 0; i < offsets.size(); ++i) {
                        GdtfChannelInfo info;
                        info.channel = offsets[i];
                        info.function = baseFunction;
                        if (!info.function.empty() && i > 0)
                            info.function += suffixForByte(i);
                        channelsVec.push_back(std::move(info));
                        ++count;
                    }
                    continue;
                }

                GdtfChannelInfo info;
                info.channel = static_cast<int>(channelsVec.size()) + 1;
                info.function = baseFunction;
                channelsVec.push_back(std::move(info));
            }
        }

        modeChannels.emplace(name, std::move(channelsVec));
        modeChannelCounts[name] = count;
    }
}

static void ParseProperties(tinyxml2::XMLElement* ft, float& weightKg, float& powerW)
{
    auto parseAttributeFloat = [](tinyxml2::XMLElement* element,
                                  std::initializer_list<const char*> names,
                                  float& outValue) -> bool {
        if (!element)
            return false;
        for (const char* name : names) {
            if (!name)
                continue;
            if (const char* raw = element->Attribute(name)) {
                float parsed = 0.0f;
                if (TryParseFloat(raw, parsed)) {
                    outValue = parsed;
                    return true;
                }
            }
        }
        return false;
    };

    auto collectGeometryPower = [&](auto&& self,
                                    tinyxml2::XMLElement* node,
                                    float& totalPowerW) -> void {
        if (!node)
            return;

        for (tinyxml2::XMLElement* child = node->FirstChildElement();
             child; child = child->NextSiblingElement())
        {
            const char* elementName = child->Name();
            if (elementName && std::string(elementName) == "WiringObject") {
                const char* componentType = child->Attribute("ComponentType");
                const bool isConsumer = componentType &&
                    std::string(componentType) == "Consumer";
                if (isConsumer) {
                    float payloadW = 0.0f;
                    if (parseAttributeFloat(child,
                                            {"ElectricalPayLoad", "ElectricalPayload"},
                                            payloadW) &&
                        payloadW > 0.0f) {
                        totalPowerW += payloadW;
                    }
                }
            }

            self(self, child, totalPowerW);
        }
    };

    weightKg = 0.0f;
    powerW = 0.0f;
    if (!ft)
        return;
    tinyxml2::XMLElement* phys = ft->FirstChildElement("PhysicalDescriptions");
    if (!phys)
        return;
    tinyxml2::XMLElement* props = phys->FirstChildElement("Properties");
    if (!props)
        return;

    if (tinyxml2::XMLElement* w = props->FirstChildElement("Weight")) {
        parseAttributeFloat(w, {"Value", "value"}, weightKg);
    }

    for (tinyxml2::XMLElement* pc = props->FirstChildElement("PowerConsumption");
         pc; pc = pc->NextSiblingElement("PowerConsumption")) {
        float parsed = 0.0f;
        if (parseAttributeFloat(pc, {"Value", "PowerConsumption", "value"}, parsed) &&
            parsed > 0.0f) {
            powerW += parsed;
        }
    }

    if (powerW > 0.0f)
        return;

    tinyxml2::XMLElement* geometries = ft->FirstChildElement("Geometries");
    if (!geometries)
        return;

    float wiringPowerW = 0.0f;
    collectGeometryPower(collectGeometryPower, geometries, wiringPowerW);
    if (wiringPowerW > 0.0f)
        powerW = wiringPowerW;
}

static std::string ParseModelColor(tinyxml2::XMLElement* ft)
{
    if (!ft)
        return {};

    tinyxml2::XMLElement* models = ft->FirstChildElement("Models");
    if (!models)
        return {};
    tinyxml2::XMLElement* model = models->FirstChildElement("Model");
    if (!model)
        return {};
    const char* attr = model->Attribute("Color");
    if (!attr)
        return {};

    std::string t = attr;
    std::replace(t.begin(), t.end(), ',', ' ');
    std::stringstream ss(t);
    double x = 0.0, y = 0.0, Yv = 0.0;
    if (!(ss >> x >> y >> Yv) || y <= 0.0)
        return {};
    double X = x * (Yv / y);
    double Z = (1.0 - x - y) * (Yv / y);
    double r = 3.2406 * X - 1.5372 * Yv - 0.4986 * Z;
    double g = -0.9689 * X + 1.8758 * Yv + 0.0415 * Z;
    double b = 0.0557 * X - 0.2040 * Yv + 1.0570 * Z;
    auto gamma = [](double c) {
        c = std::max(0.0, c);
        return c <= 0.0031308 ? 12.92 * c
                               : 1.055 * std::pow(c, 1.0/2.4) - 0.055;
    };
    r = gamma(r);
    g = gamma(g);
    b = gamma(b);
    r = std::clamp(r, 0.0, 1.0);
    g = std::clamp(g, 0.0, 1.0);
    b = std::clamp(b, 0.0, 1.0);
    int R = static_cast<int>(std::round(r * 255.0));
    int G = static_cast<int>(std::round(g * 255.0));
    int B = static_cast<int>(std::round(b * 255.0));
    std::ostringstream os;
    os << '#' << std::uppercase << std::hex << std::setfill('0')
       << std::setw(2) << R << std::setw(2) << G << std::setw(2) << B;
    return os.str();
}

static GdtfCacheEntry* GetCachedGdtf(const std::string& gdtfPath,
                                     bool* cachedFailure = nullptr,
                                     bool* fromCache = nullptr,
                                     std::string* failureReason = nullptr,
                                     std::string* outStableKey = nullptr)
{
    auto setReason = [&](const std::string& reason) {
        if (failureReason)
            *failureReason = reason;
    };

    if (cachedFailure)
        *cachedFailure = false;
    if (fromCache)
        *fromCache = false;
    if (gdtfPath.empty())
    {
        setReason("Empty GDTF path");
        return nullptr;
    }

    std::error_code ec;
    fs::path absPath = fs::absolute(gdtfPath, ec);
    if (ec)
    {
        setReason("Cannot resolve absolute GDTF path");
        return nullptr;
    }

    auto timestamp = fs::last_write_time(absPath, ec);
    if (ec)
    {
        setReason("Cannot read GDTF file metadata");
        return nullptr;
    }

    std::ifstream file(absPath, std::ios::binary);
    if (!file.is_open())
    {
        setReason("Cannot open GDTF file for hashing");
        return nullptr;
    }
    uint64_t hash = 14695981039346656037ull;
    const uint64_t prime = 1099511628211ull;
    char buffer[4096];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        std::streamsize read = file.gcount();
        for (std::streamsize i = 0; i < read; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= prime;
        }
    }
    std::ostringstream oss;
    oss << absPath.filename().string() << '|'
        << std::hex << std::setw(16) << std::setfill('0') << hash;
    std::string stableKey = oss.str();
    if (outStableKey)
        *outStableKey = stableKey;

    auto failedIt = g_failedGdtfCache.find(stableKey);
    if (failedIt != g_failedGdtfCache.end()) {
        if (cachedFailure)
            *cachedFailure = true;
        auto reasonIt = g_gdtfFailureReasons.find(stableKey);
        if (reasonIt != g_gdtfFailureReasons.end())
            setReason(reasonIt->second);
        return nullptr;
    }

    auto it = g_gdtfCache.find(stableKey);
    if (it != g_gdtfCache.end()) {
        if (it->second.doc && it->second.fixtureType) {
            if (fromCache)
                *fromCache = true;
            return &it->second;
        }

        fs::remove_all(it->second.extractedDir, ec);
        g_gdtfCache.erase(it);
    }

    GdtfCacheEntry entry;
    entry.timestamp = timestamp;
    TempExtraction extraction(absPath.string());
    if (!extraction.IsValid()) {
        g_failedGdtfCache[stableKey] = timestamp;
        setReason("Unable to extract GDTF archive (corrupted or unreadable file)");
        g_gdtfFailureReasons[stableKey] = failureReason ? *failureReason : "unknown error";
        return nullptr;
    }
    entry.extractedDir = extraction.Release();

    entry.doc = std::make_unique<tinyxml2::XMLDocument>();
    std::string descPath = entry.extractedDir + "/description.xml";
    if (entry.doc->LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS) {
        fs::remove_all(entry.extractedDir, ec);
        g_failedGdtfCache[stableKey] = timestamp;
        setReason("Missing or invalid description.xml inside GDTF file");
        g_gdtfFailureReasons[stableKey] = failureReason ? *failureReason : "unknown error";
        return nullptr;
    }

    entry.fixtureType = GetFixtureType(*entry.doc);
    if (!entry.fixtureType) {
        fs::remove_all(entry.extractedDir, ec);
        g_failedGdtfCache[stableKey] = timestamp;
        setReason("GDTF description.xml is missing a <FixtureType> element");
        g_gdtfFailureReasons[stableKey] = failureReason ? *failureReason : "unknown error";
        return nullptr;
    }

    entry.fixtureName = GetFixtureNameFromXml(entry.fixtureType);
    ParseModes(entry.fixtureType, entry.modes, entry.modeChannels, entry.modeChannelCounts);
    ParseProperties(entry.fixtureType, entry.weightKg, entry.powerW);
    entry.propertiesParsed = true;
    entry.modelColor = ParseModelColor(entry.fixtureType);
    entry.modelColorParsed = true;

    auto res = g_gdtfCache.emplace(stableKey, std::move(entry));
    g_failedGdtfCache.erase(stableKey);
    g_gdtfFailureReasons.erase(stableKey);
    return res.second ? &res.first->second : nullptr;
}

static void ParseGeometry(tinyxml2::XMLElement* node,
                          const Matrix& parent,
                          const std::unordered_map<std::string, GdtfModelInfo>& models,
                          const std::string& baseDir,
                          const std::unordered_map<std::string, tinyxml2::XMLElement*>& geomMap,
                          std::unordered_map<std::string, Mesh>& meshCache,
                          GdtfGeometryTree& outTree,
                          std::unordered_set<std::string>* missingModels,
                          std::unordered_set<std::string>* failedModelLoads,
                          int parentNodeIndex,
                          const char* overrideModel = nullptr,
                          bool parentIsLens = false)
{
    static const std::unordered_set<std::string> kGeometryNodeTypes = {
        "Geometry", "Axis", "Beam", "MediaServerLayer", "MediaServerCamera",
        "MediaServerMaster", "Display", "GeometryReference", "Laser",
        "WiringObject", "Inventory", "Structure", "Support", "Magnet"
    };

    auto makeStableNodeName = [&](GdtfNodeType nodeType, tinyxml2::XMLElement* currentNode) {
        const char* nameAttr = currentNode ? currentNode->Attribute("Name") : nullptr;
        std::string stableToken = (nameAttr && !IsBlank(nameAttr))
            ? std::string(nameAttr)
            : std::to_string(outTree.nodes.size());
        switch (nodeType) {
            case GdtfNodeType::Axis:
                return std::string("Axis_") + stableToken;
            case GdtfNodeType::Emitter:
                return std::string("Emitter_") + stableToken;
            case GdtfNodeType::Geometry:
            default:
                return std::string("Geometry_") + stableToken;
        }
    };

    Matrix local = MatrixUtils::Identity();
    if (const char* pos = node->Attribute("Position"))
        MatrixUtils::ParseMatrix(pos, local);

    Matrix transform = MatrixUtils::Multiply(parent, local);

    std::string nodeType = node->Name();
    if (nodeType == "GeometryReference") {
        const char* refName = node->Attribute("Geometry");
        if (refName) {
            auto it = geomMap.find(refName);
            if (it != geomMap.end()) {
                const char* m = node->Attribute("Model");
                ParseGeometry(it->second, transform, models, baseDir, geomMap, meshCache,
                              outTree, missingModels, failedModelLoads, parentNodeIndex,
                              m ? m : overrideModel, parentIsLens);
            }
        }
        return;
    }

    const char* modelName = overrideModel ? overrideModel : node->Attribute("Model");
    bool isLensGeometry = IsLikelyLensGeometry(node, models, modelName, parentIsLens);
    GdtfNodeType parsedNodeType = GdtfNodeType::Geometry;
    if (nodeType == "Axis")
        parsedNodeType = GdtfNodeType::Axis;
    else if (nodeType == "Beam" || isLensGeometry)
        parsedNodeType = GdtfNodeType::Emitter;

    const int currentNodeIndex = static_cast<int>(outTree.nodes.size());
    GdtfNode3D node3d;
    node3d.stableName = makeStableNodeName(parsedNodeType, node);
    node3d.type = parsedNodeType;
    node3d.parentIndex = parentNodeIndex;
    node3d.localTransform = local;
    node3d.worldTransform = transform;
    node3d.isLens = isLensGeometry;

    if (modelName) {
        auto it = models.find(modelName);
        if (it != models.end()) {
            const GdtfModelInfo& modelInfo = it->second;
            Mesh mesh;
            bool haveMesh = false;

            if (!modelInfo.file.empty()) {
                std::string path = FindModelFile(baseDir, modelInfo.file);
                if (!path.empty()) {
                    auto mit = meshCache.find(path);
                    if (mit == meshCache.end()) {
                        bool alreadyFailed = failedModelLoads && failedModelLoads->find(path) != failedModelLoads->end();
                        if (!alreadyFailed) {
                            bool loaded = false;
                            if (HasExtension(path, ".3ds"))
                                loaded = Load3DS(path, mesh);
                            else if (HasExtension(path, ".glb"))
                                loaded = LoadGLB(path, mesh);

                            if (loaded) {
                                ApplyModelDimensions(mesh, modelInfo);
                                mit = meshCache.emplace(path, std::move(mesh)).first;
                            } else {
                                bool shouldLog = true;
                                if (failedModelLoads)
                                    shouldLog = failedModelLoads->insert(path).second;

                                if (shouldLog && ConsolePanel::Instance()) {
                                    wxString msg = wxString::Format("GDTF: failed to load model %s", wxString::FromUTF8(path));
                                    ConsolePanel::Instance()->AppendMessage(msg);
                                }
                            }
                        }
                    }
                    if (mit != meshCache.end()) {
                        mesh = mit->second;
                        haveMesh = true;
                    }
                } else if (ConsolePanel::Instance()) {
                    std::string key = baseDir + "|" + modelInfo.file;
                    if (!missingModels || missingModels->insert(key).second) {
                        wxString msg = wxString::Format(
                            "GDTF: missing model file %s in %s",
                            wxString::FromUTF8(modelInfo.file),
                            wxString::FromUTF8(baseDir));
                        ConsolePanel::Instance()->AppendMessage(msg);
                    }
                }
            }

            if (!haveMesh && IsPrimitiveTypeDefined(modelInfo.primitiveType)) {
                if (BuildPrimitiveMesh(modelInfo.primitiveType, mesh)) {
                    ApplyModelDimensions(mesh, modelInfo);
                    haveMesh = true;
                }
            }

            if (haveMesh) {
                node3d.mesh = mesh;
                node3d.hasMesh = true;
            }
        }
    }

    outTree.nodes.push_back(std::move(node3d));
    if (parsedNodeType == GdtfNodeType::Axis)
        outTree.axisNodeIndices.push_back(currentNodeIndex);
    else if (parsedNodeType == GdtfNodeType::Emitter)
        outTree.emitterNodeIndices.push_back(currentNodeIndex);

    for (tinyxml2::XMLElement* child = node->FirstChildElement(); child; child = child->NextSiblingElement()) {
        std::string n = child->Name();
        if (kGeometryNodeTypes.find(n) != kGeometryNodeTypes.end() || n.rfind("Filter",0)==0) {
            ParseGeometry(child, transform, models, baseDir, geomMap, meshCache, outTree,
                          missingModels, failedModelLoads, currentNodeIndex, nullptr,
                          isLensGeometry);
        }
    }
}

bool LoadGdtfGeometryTree(const std::string& gdtfPath,
                          GdtfGeometryTree& outTree,
                          std::string* outError)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    outTree.nodes.clear();
    outTree.axisNodeIndices.clear();
    outTree.emitterNodeIndices.clear();

    std::vector<GdtfObject> outObjects;
    const bool ok = LoadGdtf(gdtfPath, outObjects, outError);
    if (!ok)
        return false;

    bool cachedFailure = false;
    bool fromCache = false;
    std::string failureReason;
    std::string cacheKey;
    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath, &cachedFailure, &fromCache,
                                          &failureReason, &cacheKey);
    if (!entry || !entry->fixtureType) {
        if (outError && outError->empty())
            *outError = failureReason.empty() ? "unknown error" : failureReason;
        return false;
    }

    tinyxml2::XMLElement* ft = entry->fixtureType;
    std::unordered_map<std::string, GdtfModelInfo> models;
    if (tinyxml2::XMLElement* modelList = ft->FirstChildElement("Models")) {
        for (tinyxml2::XMLElement* m = modelList->FirstChildElement("Model");
             m; m = m->NextSiblingElement("Model")) {
            const char* name = m->Attribute("Name");
            const char* file = m->Attribute("File");
            const char* primitiveType = m->Attribute("PrimitiveType");
            bool hasName = name && !IsBlank(name);
            bool hasFile = file && !IsBlank(file);
            std::string primitive = primitiveType ? primitiveType : "";
            bool hasPrimitive = IsPrimitiveTypeDefined(primitive);
            if (!hasName || (!hasFile && !hasPrimitive))
                continue;

            GdtfModelInfo info;
            if (hasFile)
                info.file = file;
            info.primitiveType = primitive;
            m->QueryFloatAttribute("Length", &info.length);
            m->QueryFloatAttribute("Width", &info.width);
            m->QueryFloatAttribute("Height", &info.height);
            models[name] = info;
        }
    }

    std::unordered_map<std::string, Mesh>& meshCache = entry->meshCache;
    std::unordered_set<std::string>* missingModels = &entry->missingModelsLogged;
    std::unordered_set<std::string>* failedModelLoads = &entry->failedModelLoads;

    if (tinyxml2::XMLElement* geoms = ft->FirstChildElement("Geometries")) {
        std::unordered_map<std::string, tinyxml2::XMLElement*> geomMap;
        for (tinyxml2::XMLElement* g = geoms->FirstChildElement(); g;
             g = g->NextSiblingElement()) {
            if (const char* n = g->Attribute("Name"))
                geomMap[n] = g;
        }
        for (tinyxml2::XMLElement* g = geoms->FirstChildElement(); g;
             g = g->NextSiblingElement()) {
            ParseGeometry(g, MatrixUtils::Identity(), models, entry->extractedDir,
                          geomMap, meshCache, outTree, missingModels, failedModelLoads,
                          -1, nullptr, false);
        }
    }

    return !outTree.nodes.empty();
}

bool LoadGdtf(const std::string& gdtfPath,
              std::vector<GdtfObject>& outObjects,
              std::string* outError)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    outObjects.clear();
    if (outError)
        outError->clear();
    bool cachedFailure = false;
    bool fromCache = false;
    std::string failureReason;
    std::string cacheKey;

    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath, &cachedFailure, &fromCache, &failureReason, &cacheKey);

    if (!fromCache && !cachedFailure && ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Loading GDTF %s", wxString::FromUTF8(gdtfPath));
        ConsolePanel::Instance()->AppendMessage(msg);
    }
    if (!entry || !entry->fixtureType) {
        size_t failureCount = ++g_gdtfFailedAttempts[cacheKey.empty() ? gdtfPath : cacheKey];
        if (outError)
            *outError = failureReason.empty() ? "unknown error" : failureReason;
        else if (ConsolePanel::Instance()) {
            if (cachedFailure) {
                if (failureCount == 2) {
                    wxString msg = wxString::Format(
                        "Failed to load GDTF %s: %s (repeated %zu times, suppressing further messages)",
                        wxString::FromUTF8(gdtfPath),
                        wxString::FromUTF8(failureReason.empty() ? "unknown error" : failureReason),
                        failureCount);
                    ConsolePanel::Instance()->AppendMessage(msg);
                }
            } else {
                wxString msg = wxString::Format("GDTF: failed to load %s: %s",
                                               wxString::FromUTF8(gdtfPath),
                                               wxString::FromUTF8(failureReason.empty() ? "unknown error" : failureReason));
                ConsolePanel::Instance()->AppendMessage(msg);
            }
        }
        return false;
    }

    g_gdtfFailedAttempts.erase(cacheKey.empty() ? gdtfPath : cacheKey);

    tinyxml2::XMLElement* ft = entry->fixtureType;

    std::unordered_map<std::string, GdtfModelInfo> models;
    if (tinyxml2::XMLElement* modelList = ft->FirstChildElement("Models")) {
        for (tinyxml2::XMLElement* m = modelList->FirstChildElement("Model"); m; m = m->NextSiblingElement("Model")) {
            const char* name = m->Attribute("Name");
            const char* file = m->Attribute("File");
            const char* primitiveType = m->Attribute("PrimitiveType");
            bool hasName = name && !IsBlank(name);
            bool hasFile = file && !IsBlank(file);
            std::string primitive = primitiveType ? primitiveType : "";
            bool hasPrimitive = IsPrimitiveTypeDefined(primitive);
            if (hasName && !hasFile && !hasPrimitive) {
                if (ConsolePanel::Instance() && entry->emptyModelFileLogged.insert(name).second) {
                    wxString msg = wxString::Format(
                        "GDTF: Model %s has empty File and undefined PrimitiveType",
                        wxString::FromUTF8(name));
                    ConsolePanel::Instance()->AppendMessage(msg);
                }
                continue;
            }
            if (hasName) {
                GdtfModelInfo info;
                if (hasFile)
                    info.file = file;
                info.primitiveType = primitive;
                m->QueryFloatAttribute("Length", &info.length);
                m->QueryFloatAttribute("Width", &info.width);
                m->QueryFloatAttribute("Height", &info.height);
                models[name] = info;
            }
        }
    }

    std::unordered_map<std::string, Mesh>& meshCache = entry->meshCache;
    std::unordered_set<std::string>* missingModels = &entry->missingModelsLogged;
    std::unordered_set<std::string>* failedModelLoads = &entry->failedModelLoads;
    GdtfGeometryTree geometryTree;
    if (tinyxml2::XMLElement* geoms = ft->FirstChildElement("Geometries")) {
        std::unordered_map<std::string, tinyxml2::XMLElement*> geomMap;
        for (tinyxml2::XMLElement* g = geoms->FirstChildElement(); g; g = g->NextSiblingElement()) {
            if (const char* n = g->Attribute("Name"))
                geomMap[n] = g;
        }
        for (tinyxml2::XMLElement* g = geoms->FirstChildElement(); g; g = g->NextSiblingElement()) {
            ParseGeometry(g, MatrixUtils::Identity(), models, entry->extractedDir, geomMap,
                          meshCache, geometryTree, missingModels, failedModelLoads, -1);
        }
    }

    for (const GdtfNode3D& node : geometryTree.nodes) {
        if (node.hasMesh)
            outObjects.push_back({node.mesh, node.worldTransform, node.isLens});
    }

    if (ConsolePanel::Instance() && !fromCache) {
        wxString msg = wxString::Format("GDTF: loaded %zu objects from %s",
                                       outObjects.size(),
                                       wxString::FromUTF8(gdtfPath));
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    if (outObjects.empty()) {
        constexpr const char* kEmptyGeometryReason = "No geometry with models found";
        size_t count = ++entry->emptyGeometryLogCount;
        std::string extractionDir = entry->extractedDir;
        auto timestamp = entry->timestamp;

        if (!cacheKey.empty()) {
            g_failedGdtfCache[cacheKey] = timestamp;
            g_gdtfFailureReasons[cacheKey] = kEmptyGeometryReason;
            g_gdtfCache.erase(cacheKey);
            std::error_code ec;
            fs::remove_all(extractionDir, ec);
        }

        if (outError)
            *outError = kEmptyGeometryReason;
        else if (ConsolePanel::Instance()) {
            if (count == 1) {
                wxString msg = wxString::Format(
                    "GDTF: loaded %s but no geometry with models was found",
                    wxString::FromUTF8(gdtfPath));
                ConsolePanel::Instance()->AppendMessage(msg);
            } else if (count == 2) {
                wxString msg = wxString::Format(
                    "GDTF: loaded %s but no geometry with models was found (repeated %zu times, suppressing further messages)",
                    wxString::FromUTF8(gdtfPath),
                    count);
                ConsolePanel::Instance()->AppendMessage(msg);
            }
        }
        return false;
    }

    entry->emptyGeometryLogCount = 0;
    return true;
}

int GetGdtfModeChannelCount(const std::string& gdtfPath,
                            const std::string& modeName)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    if (gdtfPath.empty() || modeName.empty())
        return -1;

    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
    if (!entry)
        return -1;

    auto countIt = entry->modeChannelCounts.find(modeName);
    if (countIt != entry->modeChannelCounts.end())
        return countIt->second;

    auto it = entry->modeChannels.find(modeName);
    if (it == entry->modeChannels.end())
        return -1;

    return static_cast<int>(it->second.size());
}

std::vector<std::string> GetGdtfModes(const std::string& gdtfPath)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    std::vector<std::string> result;
    if (gdtfPath.empty())
        return result;

    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
    if (!entry)
        return result;

    return entry->modes;
}

std::vector<GdtfChannelInfo> GetGdtfModeChannels(
    const std::string& gdtfPath,
    const std::string& modeName)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    std::vector<GdtfChannelInfo> result;
    if (gdtfPath.empty() || modeName.empty())
        return result;

    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
    if (!entry)
        return result;

    auto it = entry->modeChannels.find(modeName);
    if (it != entry->modeChannels.end())
        result = it->second;
    return result;
}

std::string GetGdtfFixtureName(const std::string& gdtfPath)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    if (gdtfPath.empty())
        return {};

    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
    if (!entry)
        return {};

    return entry->fixtureName;
}

bool GetGdtfProperties(const std::string& gdtfPath,
                       float& outWeightKg,
                       float& outPowerW)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    outWeightKg = 0.0f;
    outPowerW = 0.0f;
    if (gdtfPath.empty())
        return false;

    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
    if (!entry)
        return false;

    outWeightKg = entry->weightKg;
    outPowerW = entry->powerW;
    return entry->propertiesParsed;
}

std::string GetGdtfModelColor(const std::string& gdtfPath)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    if (gdtfPath.empty())
        return {};

    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
    if (!entry)
        return {};

    return entry->modelColor;
}

static bool ZipDir(const std::string& srcDir, const std::string& dstZip)
{
    wxFileOutputStream output(dstZip);
    if (!output.IsOk())
        return false;
    wxZipOutputStream zip(output);
    for (auto& p : fs::recursive_directory_iterator(srcDir)) {
        if (!p.is_regular_file())
            continue;
        fs::path rel = fs::relative(p.path(), srcDir);
        auto* e = new wxZipEntry(rel.generic_string());
        e->SetMethod(wxZIP_METHOD_DEFLATE);
        zip.PutNextEntry(e);
        std::ifstream in(p.path(), std::ios::binary);
        char buf[4096];
        while (in.good()) {
            in.read(buf, sizeof(buf));
            std::streamsize s = in.gcount();
            if (s > 0)
                zip.Write(buf, s);
        }
        zip.CloseEntry();
    }
    zip.Close();
    return true;
}

static std::string HexToCie(const std::string& hex)
{
    if (hex.size() != 7 || hex[0] != '#')
        return {};
    unsigned int rgb = 0;
    std::istringstream iss(hex.substr(1));
    iss >> std::hex >> rgb;
    unsigned int R = (rgb >> 16) & 0xFF;
    unsigned int G = (rgb >> 8) & 0xFF;
    unsigned int B = rgb & 0xFF;
    auto invGamma = [](double c) {
        return c <= 0.04045 ? c / 12.92
                            : std::pow((c + 0.055) / 1.055, 2.4);
    };
    double r = invGamma(R / 255.0);
    double g = invGamma(G / 255.0);
    double b = invGamma(B / 255.0);
    double X = 0.4124 * r + 0.3576 * g + 0.1805 * b;
    double Y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    double Z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
    double sum = X + Y + Z;
    double x = 0.0, y = 0.0;
    if (sum > 0.0) {
        x = X / sum;
        y = Y / sum;
    }
    std::ostringstream colStr;
    colStr << std::fixed << std::setprecision(6) << x << "," << y << "," << Y;
    return colStr.str();
}

bool SetGdtfModelColor(const std::string& gdtfPath,
                       const std::string& hexColor)
{
    if (gdtfPath.empty())
        return false;

    TempExtraction extraction(gdtfPath);
    if (!extraction.IsValid())
        return false;

    std::string descPath = extraction.Path() + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
        return false;

    tinyxml2::XMLElement* ft = doc.FirstChildElement("GDTF");
    if (ft)
        ft = ft->FirstChildElement("FixtureType");
    else
        ft = doc.FirstChildElement("FixtureType");
    if (!ft)
        return false;

    tinyxml2::XMLElement* models = ft->FirstChildElement("Models");
    if (!models)
        return false;

    std::string cie = HexToCie(hexColor);
    for (tinyxml2::XMLElement* m = models->FirstChildElement("Model"); m;
         m = m->NextSiblingElement("Model"))
        m->SetAttribute("Color", cie.c_str());

    doc.SaveFile(descPath.c_str());
    bool ok = ZipDir(extraction.Path(), gdtfPath);
    return ok;
}

namespace {

std::string BuildGdtfIsoTimestampUtc()
{
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now();
    const std::time_t nowTime = Clock::to_time_t(now);

    std::tm utcTm{};
#if defined(_WIN32)
    gmtime_s(&utcTm, &nowTime);
#else
    gmtime_r(&nowTime, &utcTm);
#endif

    std::ostringstream stamp;
    stamp << std::put_time(&utcTm, "%Y-%m-%dT%H:%M:%SZ");
    return stamp.str();
}

void AppendGdtfRevision(tinyxml2::XMLElement* fixtureType,
                        tinyxml2::XMLDocument& doc,
                        const std::string& text,
                        const std::string& modifiedByProgram)
{
    if (!fixtureType)
        return;

    tinyxml2::XMLElement* revisions = fixtureType->FirstChildElement("Revisions");
    if (!revisions) {
        revisions = doc.NewElement("Revisions");
        fixtureType->InsertEndChild(revisions);
    }

    tinyxml2::XMLElement* revision = doc.NewElement("Revision");
    revision->SetAttribute("Date", BuildGdtfIsoTimestampUtc().c_str());
    revision->SetAttribute("Text", text.c_str());
    revision->SetAttribute("ModifiedBy",
                           modifiedByProgram.empty() ? "Perastage" : modifiedByProgram.c_str());
    revision->SetAttribute("UserID", 0);
    revisions->InsertEndChild(revision);
    fixtureType->SetAttribute("Editor", "Perastage");
}

tinyxml2::XMLElement* EnsurePropertiesNode(tinyxml2::XMLElement* fixtureType,
                                           tinyxml2::XMLDocument& doc)
{
    if (!fixtureType)
        return nullptr;

    tinyxml2::XMLElement* physicalDescriptions =
        fixtureType->FirstChildElement("PhysicalDescriptions");
    if (!physicalDescriptions) {
        physicalDescriptions = doc.NewElement("PhysicalDescriptions");
        fixtureType->InsertEndChild(physicalDescriptions);
    }

    tinyxml2::XMLElement* properties =
        physicalDescriptions->FirstChildElement("Properties");
    if (!properties) {
        properties = doc.NewElement("Properties");
        physicalDescriptions->InsertEndChild(properties);
    }
    return properties;
}

} // namespace

bool SetGdtfProperties(const std::string& gdtfPath,
                       float weightKg,
                       float powerW,
                       const std::string& modifiedByProgram)
{
    if (gdtfPath.empty())
        return false;

    TempExtraction extraction(gdtfPath);
    if (!extraction.IsValid())
        return false;

    const std::string descPath = extraction.Path() + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(descPath.c_str()) != tinyxml2::XML_SUCCESS)
        return false;

    tinyxml2::XMLElement* fixtureType = doc.FirstChildElement("GDTF");
    if (fixtureType)
        fixtureType = fixtureType->FirstChildElement("FixtureType");
    else
        fixtureType = doc.FirstChildElement("FixtureType");
    if (!fixtureType)
        return false;

    tinyxml2::XMLElement* properties = EnsurePropertiesNode(fixtureType, doc);
    if (!properties)
        return false;

    tinyxml2::XMLElement* weightNode = properties->FirstChildElement("Weight");
    if (!weightNode) {
        weightNode = doc.NewElement("Weight");
        properties->InsertEndChild(weightNode);
    }
    weightNode->SetAttribute("Value", wxString::Format("%.3f", weightKg).ToStdString().c_str());

    tinyxml2::XMLElement* powerNode =
        properties->FirstChildElement("PowerConsumption");
    if (!powerNode) {
        powerNode = doc.NewElement("PowerConsumption");
        properties->InsertEndChild(powerNode);
    }
    powerNode->SetAttribute("Value", wxString::Format("%.3f", powerW).ToStdString().c_str());

    AppendGdtfRevision(
        fixtureType, doc,
        "Updated fixture physical properties (Weight/PowerConsumption) from Perastage",
        modifiedByProgram);

    doc.SaveFile(descPath.c_str());
    if (!ZipDir(extraction.Path(), gdtfPath))
        return false;

    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);
    g_gdtfCache.erase(gdtfPath);
    return true;
}
