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
#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "gdtf_mutation_audit.h"
#include "gdtf_canonicalizer.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "matrixutils.h"
#include "runtime_storage.h"
#include "meshprimitives.h"
#include "consolepanel.h"

#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cmath>
#include <optional>
#include <string_view>
#include <tinyxml2.h>
#include <wx/wx.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>
#include <wx/filename.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

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
#include <iomanip>
#include <mutex>
#include <system_error>

namespace fs = std::filesystem;

namespace {
// Parses a trimmed float token and succeeds only when the whole token is numeric.
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

    errno = 0;
    std::string trimmedText(trimmed);
    char* endPtr = nullptr;
    const double parsed = std::strtod(trimmedText.c_str(), &endPtr);
    if (endPtr == trimmedText.c_str() + trimmedText.size() && errno != ERANGE) {
        out = static_cast<float>(parsed);
        return true;
    }
    return false;
}

// Parses a trimmed integer token and succeeds only when the whole token is numeric.
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
    runtime_storage::SceneResourceLeasePtr extractionLease;
    std::unique_ptr<tinyxml2::XMLDocument> doc;
    tinyxml2::XMLElement* fixtureType = nullptr;
    std::vector<std::string> modes;
    std::unordered_map<std::string, std::vector<GdtfChannelInfo>> modeChannels;
    std::unordered_map<std::string, int> modeChannelCounts;
    std::unordered_map<std::string, Mesh> meshCache;
    std::unordered_map<std::string, GdtfGeometryTree> geometryTreeCache;
    std::unordered_map<std::string, std::vector<GdtfObject>> flatObjectCache;
    std::string fixtureName;
    std::string fixtureManufacturer;
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

struct CachedGdtfHash
{
    uint64_t contentHash = 0;
};

using GdtfCacheMap = std::unordered_map<std::string, GdtfCacheEntry>;

// Returns the process-lifetime GDTF cache without registering resource-owning static destructors.
static GdtfCacheMap& GdtfCache()
{
    static auto* cache = new GdtfCacheMap();
    return *cache;
}

static std::unordered_map<std::string, fs::file_time_type> g_failedGdtfCache;
static std::unordered_map<std::string, size_t> g_gdtfFailedAttempts;
static std::unordered_map<std::string, std::string> g_gdtfFailureReasons;
static std::unordered_map<std::string, CachedGdtfHash> g_gdtfHashCache;
static std::recursive_mutex g_gdtfCacheMutex;
static bool g_gdtfCacheShutdown = false;

struct MissingModelLog
{
    size_t count = 0;
    std::string file;
    std::string baseDir;
};

static bool ExtractZip(const std::string& zipPath, const std::string& destDir);

// Returns a lowercase copy of the provided string for case-insensitive comparisons.
static std::string ToLower(const std::string& s)
{
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return std::tolower(c); });
    return t;
}

// Checks whether a path ends with the requested extension using case-insensitive matching.
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


// Builds a stable cache key for a generated primitive mesh.
static std::string BuildPrimitiveMeshCacheKey(const GdtfModelInfo& modelInfo)
{
    std::ostringstream oss;
    oss << "primitive|" << ToLower(modelInfo.primitiveType) << '|'
        << std::setprecision(9) << modelInfo.length << '|'
        << modelInfo.width << '|' << modelInfo.height;
    return oss.str();
}

// Returns a primitive mesh from the per-archive cache or builds it once.
static bool GetCachedPrimitiveMesh(const GdtfModelInfo& modelInfo,
                                   std::unordered_map<std::string, Mesh>& meshCache,
                                   Mesh& mesh)
{
    GdtfModelInfo adjusted = modelInfo;
    constexpr float kDefaultMeters = 0.1f;
    if (adjusted.length <= 0.0f)
        adjusted.length = kDefaultMeters;
    if (adjusted.width <= 0.0f)
        adjusted.width = kDefaultMeters;
    if (adjusted.height <= 0.0f)
        adjusted.height = kDefaultMeters;

    const std::string key = BuildPrimitiveMeshCacheKey(adjusted);
    auto it = meshCache.find(key);
    if (it != meshCache.end()) {
        mesh = it->second;
        return true;
    }

    Mesh generated;
    if (!BuildPrimitiveMesh(adjusted.primitiveType, generated))
        return false;
    ApplyModelDimensions(generated, adjusted);
    auto inserted = meshCache.emplace(key, std::move(generated)).first;
    mesh = inserted->second;
    return true;
}

// Builds a box primitive scaled from model dimensions using visible defaults for missing size components.
static bool BuildDimensionFallbackMesh(const GdtfModelInfo& modelInfo, Mesh& mesh)
{
    GdtfModelInfo adjusted = modelInfo;
    constexpr float kDefaultMeters = 0.1f;
    if (adjusted.length <= 0.0f)
        adjusted.length = kDefaultMeters;
    if (adjusted.width <= 0.0f)
        adjusted.width = kDefaultMeters;
    if (adjusted.height <= 0.0f)
        adjusted.height = kDefaultMeters;

    if (!BuildPrimitiveMesh("Cube", mesh))
        return false;
    ApplyModelDimensions(mesh, adjusted);
    return true;
}

// Locates a model file using spec-compliant preferred folders and cached recursive fallback lookups.
static std::string FindModelFile(const std::string& baseDir,
                                 const std::string& fileName)
{
    const fs::path modelsDir = PathUtils::PathFromUtf8(baseDir) / "models";
    if (!fs::exists(modelsDir))
        return {};

    const fs::path requested = PathUtils::PathFromUtf8(fileName);
    const std::string requestedName = requested.filename().string();
    const bool hasExtension = requested.has_extension();

    auto tryCandidate = [&](const fs::path& candidate) -> std::string {
        if (fs::exists(candidate) && fs::is_regular_file(candidate))
            return candidate.string();
        return {};
    };

    if (hasExtension) {
        const fs::path direct = modelsDir / requested;
        if (std::string resolved = tryCandidate(direct); !resolved.empty())
            return resolved;
    } else {
        const std::vector<std::pair<std::string, std::string>> preferredFolders = {
            {"gltf_low", ".glb"},
            {"gltf", ".glb"},
            {"gltf_high", ".glb"},
            {"glb", ".glb"},
            {"3ds_low", ".3ds"},
            {"3ds", ".3ds"},
            {"3ds_high", ".3ds"},
        };
        for (const auto& [folder, extension] : preferredFolders) {
            const fs::path candidate = modelsDir / folder / (requestedName + extension);
            if (std::string resolved = tryCandidate(candidate); !resolved.empty())
                return resolved;
        }
    }

    static std::unordered_map<std::string, std::vector<fs::path>> modelFileIndexCache;
    auto indexIt = modelFileIndexCache.find(modelsDir.string());
    if (indexIt == modelFileIndexCache.end()) {
        std::vector<fs::path> indexedPaths;
        for (const auto& entry : fs::recursive_directory_iterator(modelsDir)) {
            if (entry.is_regular_file())
                indexedPaths.push_back(entry.path());
        }
        indexIt = modelFileIndexCache.emplace(modelsDir.string(), std::move(indexedPaths)).first;
    }

    for (const fs::path& path : indexIt->second) {
        if (hasExtension) {
            if (ToLower(path.filename().string()) == ToLower(requestedName))
                return path.string();
            continue;
        }
        if (ToLower(path.stem().string()) != ToLower(requestedName))
            continue;
        if (HasExtension(path, ".glb") || HasExtension(path, ".3ds"))
            return path.string();
    }

    return {};
}

// Creates a unique temporary extraction directory without throwing.

struct TempExtraction
{
    // Extracts a GDTF archive into a temporary directory for legacy loading.
    explicit TempExtraction(const std::string& zipPath)
    {
        workspace = runtime_storage::TemporaryWorkspace("gdtf-load");
        if (!workspace.IsValid())
            return;
        dir = workspace.Path().string();
        extracted = ExtractZip(zipPath, dir);
    }

    // Removes temporary extraction data without throwing during cleanup.
    ~TempExtraction()
    {
        workspace.Cleanup();
    }

    // Reports whether archive extraction completed successfully.
    bool IsValid() const { return extracted; }

    // Releases ownership of the extraction directory to the caller.
    runtime_storage::SceneResourceLeasePtr ReleaseLease()
    {
        dir.clear();
        return workspace.TransferToSceneLease();
    }

    // Returns the temporary extraction directory path.
    const std::string& Path() const { return dir; }

private:
    std::string dir;
    runtime_storage::TemporaryWorkspace workspace;
    bool extracted = false;
};

// Extracts a ZIP archive into a destination directory while skipping unsafe paths.
static bool ExtractZip(const std::string& zipPath, const std::string& destDir)
{
    const fs::path sourcePath = PathUtils::PathFromUtf8(zipPath);
    const fs::path destinationRoot = PathUtils::PathFromUtf8(destDir);
    const gdtf::ArchiveReadResult result =
        gdtf::ExtractGdtfArchive(sourcePath, destinationRoot);
    if (ConsolePanel::Instance()) {
        for (const gdtf::ArchiveDiagnostic& diagnostic : result.diagnostics) {
            if (diagnostic.code == gdtf::ArchiveDiagnosticCode::None)
                continue;
            wxString msg = wxString::Format(
                "GDTF: %s%s",
                wxString::FromUTF8(diagnostic.message),
                diagnostic.entryPath.empty()
                    ? wxString()
                    : wxString::Format(" (%s)",
                                       wxString::FromUTF8(diagnostic.entryPath)));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
    }
    const bool hasFatalDiagnostic =
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                    [](const gdtf::ArchiveDiagnostic& diagnostic) {
                        switch (diagnostic.code) {
                        case gdtf::ArchiveDiagnosticCode::None:
                        case gdtf::ArchiveDiagnosticCode::NonCanonicalDescriptionXml:
                        case gdtf::ArchiveDiagnosticCode::Utf8FlagMissing:
                        case gdtf::ArchiveDiagnosticCode::Utf8FallbackUsed:
                        case gdtf::ArchiveDiagnosticCode::LegacyFilenameEncodingUsed:
                            return false;
                        default:
                            return true;
                        }
                    });
    return !result.entries.empty() && !hasFatalDiagnostic;
}

// Finds the extracted description.xml using the tolerant read priority.
static std::string FindExtractedDescriptionPath(const std::string& extractedDir)
{
    std::error_code ec;
    const fs::path root = PathUtils::PathFromUtf8(extractedDir);
    const fs::path canonical = root / "description.xml";
    if (fs::is_regular_file(canonical, ec) && !ec)
        return canonical.string();
    ec.clear();

    std::vector<fs::path> rootCaseInsensitive;
    std::vector<fs::path> nested;
    if (!fs::exists(root, ec) || ec)
        return {};

    for (fs::recursive_directory_iterator it(root, fs::directory_options::none, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        std::string name = it->path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (name != "description.xml")
            continue;
        const bool rootEntry = it->path().parent_path() == root;
        if (rootEntry)
            rootCaseInsensitive.push_back(it->path());
        else
            nested.push_back(it->path());
    }
    if (rootCaseInsensitive.size() == 1)
        return rootCaseInsensitive.front().string();
    if (rootCaseInsensitive.empty() && nested.size() == 1)
        return nested.front().string();
    return {};
}

// Finds the FixtureType element without creating or normalizing XML nodes.
static tinyxml2::XMLElement* GetFixtureType(tinyxml2::XMLDocument& doc)
{
    tinyxml2::XMLElement* root = doc.FirstChildElement("GDTF");
    return root ? root->FirstChildElement("FixtureType") : nullptr;
}

static bool ParseXmlWithEscapedControlFallback(const std::string& filePath,
                                               tinyxml2::XMLDocument& doc)
{
    if (doc.LoadFile(filePath.c_str()) == tinyxml2::XML_SUCCESS)
        return true;

    std::ifstream input(filePath, std::ios::binary);
    if (!input.is_open())
        return false;

    std::string raw((std::istreambuf_iterator<char>(input)),
                    std::istreambuf_iterator<char>());
    if (raw.empty())
        return false;

    if (raw.find("\\n") == std::string::npos &&
        raw.find("\\r") == std::string::npos &&
        raw.find("\\t") == std::string::npos) {
        return false;
    }

    std::string sanitized;
    sanitized.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            const char escaped = raw[i + 1];
            if (escaped == 'n') {
                sanitized.push_back('\n');
                ++i;
                continue;
            }
            if (escaped == 'r') {
                sanitized.push_back('\r');
                ++i;
                continue;
            }
            if (escaped == 't') {
                sanitized.push_back('\t');
                ++i;
                continue;
            }
        }
        sanitized.push_back(raw[i]);
    }

    doc.Clear();
    return doc.Parse(sanitized.c_str(), sanitized.size()) == tinyxml2::XML_SUCCESS;
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
    tinyxml2::XMLElement* geometriesNode = ft->FirstChildElement("Geometries");

    auto parseIntAttribute = [](tinyxml2::XMLElement* element,
                                const char* attributeName,
                                int fallback) {
        if (!element || !attributeName)
            return fallback;
        int parsed = fallback;
        const char* raw = element->Attribute(attributeName);
        if (!raw)
            return fallback;
        if (!TryParseInt(raw, parsed))
            return fallback;
        return parsed;
    };
    auto collectGeometryReferenceOffsets =
        [&](const std::string& modeRootGeometryName) {
            std::unordered_map<std::string, std::vector<int>> offsetsByGeometry;
            if (!geometriesNode || modeRootGeometryName.empty())
                return offsetsByGeometry;

            auto findGeometryByName =
                [&](auto&& self, tinyxml2::XMLElement* node,
                    const std::string& wantedName) -> tinyxml2::XMLElement* {
                    if (!node)
                        return nullptr;
                    const char* name = node->Attribute("Name");
                    if (name && wantedName == name)
                        return node;
                    for (tinyxml2::XMLElement* child = node->FirstChildElement();
                         child; child = child->NextSiblingElement()) {
                        if (tinyxml2::XMLElement* found = self(self, child, wantedName))
                            return found;
                    }
                    return nullptr;
                };

            tinyxml2::XMLElement* modeRootGeometry = nullptr;
            for (tinyxml2::XMLElement* geometry = geometriesNode->FirstChildElement();
                 geometry && !modeRootGeometry;
                 geometry = geometry->NextSiblingElement()) {
                modeRootGeometry = findGeometryByName(findGeometryByName, geometry,
                                                      modeRootGeometryName);
            }
            if (!modeRootGeometry)
                return offsetsByGeometry;

            auto walkGeometry =
                [&](auto&& self, tinyxml2::XMLElement* node) -> void {
                    if (!node)
                        return;

                    if (std::string(node->Name()) == "GeometryReference") {
                        const char* referencedGeometry = node->Attribute("Geometry");
                        if (referencedGeometry && *referencedGeometry) {
                            std::vector<int>& shifts =
                                offsetsByGeometry[referencedGeometry];
                            bool hadBreak = false;
                            for (tinyxml2::XMLElement* br = node->FirstChildElement("Break");
                                 br; br = br->NextSiblingElement("Break")) {
                                hadBreak = true;
                                int dmxOffset = parseIntAttribute(br, "DMXOffset", 1);
                                shifts.push_back(std::max(0, dmxOffset - 1));
                            }
                            if (!hadBreak)
                                shifts.push_back(0);
                        }
                    }

                    for (tinyxml2::XMLElement* child = node->FirstChildElement();
                         child; child = child->NextSiblingElement()) {
                        self(self, child);
                    }
                };

            walkGeometry(walkGeometry, modeRootGeometry);
            return offsetsByGeometry;
        };

    for (tinyxml2::XMLElement* m = modesNode->FirstChildElement("DMXMode");
         m; m = m->NextSiblingElement("DMXMode"))
    {
        const char* name = m->Attribute("Name");
        if (!name)
            continue;
        modes.push_back(name);
        const std::string modeRootGeometryName =
            m->Attribute("Geometry") ? m->Attribute("Geometry") : "";
        const std::unordered_map<std::string, std::vector<int>> geometryOffsetShifts =
            collectGeometryReferenceOffsets(modeRootGeometryName);

        std::vector<GdtfChannelInfo> channelsVec;
        int footprint = 0;
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
                if (offStr.empty())
                    return parsedOffsets;
                auto toLower = [](std::string value) {
                    std::transform(value.begin(), value.end(), value.begin(),
                                   [](unsigned char ch) {
                                       return static_cast<char>(std::tolower(ch));
                                   });
                    return value;
                };
                const std::string offStrLower = toLower(offStr);
                if (offStrLower == "none")
                    return parsedOffsets;
                for (char& ch : offStr) {
                    if (ch == ';' || ch == '|' || ch == '/')
                        ch = ',';
                }
                std::stringstream ss(offStr);
                std::string group;
                while (std::getline(ss, group, ',')) {
                    trim(group);
                    if (group.empty() || toLower(group) == "none")
                        continue;

                    std::stringstream groupStream(group);
                    std::string token;
                    while (groupStream >> token) {
                        trim(token);
                        if (token.empty() || toLower(token) == "none")
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
                }
                return parsedOffsets;
            };
            auto extractFunctionName = [](tinyxml2::XMLElement* dmxChannel) {
                if (!dmxChannel)
                    return std::string{};
                const char* channelAttr = dmxChannel->Attribute("Attribute");
                if (!channelAttr)
                    channelAttr = dmxChannel->Attribute("attribute");
                if (channelAttr && *channelAttr)
                    return std::string(channelAttr);
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
            std::unordered_map<std::string, std::string> modeMasterNameByReference;
            for (tinyxml2::XMLElement* modeChannel = channels->FirstChildElement("DMXChannel");
                 modeChannel; modeChannel = modeChannel->NextSiblingElement("DMXChannel")) {
                for (tinyxml2::XMLElement* lc = modeChannel->FirstChildElement("LogicalChannel");
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
            }
            auto buildFunctionSummary =
                [&](tinyxml2::XMLElement* dmxChannel) {
                    if (!dmxChannel)
                        return std::string{};

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
                const std::string geometryName =
                    c->Attribute("Geometry") ? c->Attribute("Geometry") : "";

                auto emitChannelByte = [&](int channelNumber, size_t byteIndex) {
                    if (channelNumber <= 0)
                        return;
                    GdtfChannelInfo info;
                    info.channel = channelNumber;
                    info.function = baseFunction;
                    if (!info.function.empty() && byteIndex > 0)
                        info.function += suffixForByte(byteIndex);
                    channelsVec.push_back(std::move(info));
                    if (channelNumber > footprint)
                        footprint = channelNumber;
                };

                if (!offsets.empty()) {
                    bool expandedWithGeometryReference = false;
                    auto geometryShiftIt = geometryOffsetShifts.find(geometryName);
                    if (!geometryName.empty() &&
                        geometryShiftIt != geometryOffsetShifts.end() &&
                        !geometryShiftIt->second.empty()) {
                        expandedWithGeometryReference = true;
                        for (int shift : geometryShiftIt->second) {
                            for (size_t i = 0; i < offsets.size(); ++i)
                                emitChannelByte(offsets[i] + shift, i);
                        }
                    }

                    if (!expandedWithGeometryReference) {
                        for (size_t i = 0; i < offsets.size(); ++i)
                            emitChannelByte(offsets[i], i);
                    }
                    continue;
                }

                GdtfChannelInfo info;
                info.channel = 0;
                info.function = baseFunction;
                info.isVirtual = true;
                channelsVec.push_back(std::move(info));
            }
        }

        std::stable_sort(channelsVec.begin(), channelsVec.end(),
                         [](const GdtfChannelInfo& lhs, const GdtfChannelInfo& rhs) {
                             if (lhs.isVirtual != rhs.isVirtual)
                                 return !lhs.isVirtual;
                             return lhs.channel < rhs.channel;
                         });

        modeChannels.emplace(name, std::move(channelsVec));
        modeChannelCounts[name] = footprint;
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

// Converts a filesystem timestamp to a portable nanosecond string for cache keys.
static std::string FileTimestampToCacheKeyString(const fs::file_time_type& timestamp)
{
    const auto timestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        timestamp.time_since_epoch());
    const auto timestampValue = static_cast<std::int64_t>(timestampNs.count());
    return std::to_string(timestampValue);
}

// Builds the metadata key used to reuse hashes for unchanged GDTF archive files.
static std::string BuildGdtfHashCacheKey(const fs::path& absPath,
                                         const fs::file_time_type& timestamp,
                                         uintmax_t fileSize)
{
    std::ostringstream oss;
    oss << PathUtils::PathToUtf8(absPath) << '|'
        << FileTimestampToCacheKeyString(timestamp) << '|'
        << fileSize;
    return oss.str();
}

// Calculates the stable content hash for a GDTF archive file.
static bool ComputeGdtfContentHash(const fs::path& absPath, uint64_t& hash)
{
    std::ifstream file(absPath, std::ios::binary);
    if (!file.is_open())
        return false;

    hash = 14695981039346656037ull;
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
    return true;
}

// Formats the stable in-process cache key used by parsed and failed GDTF caches.
static std::string BuildGdtfStableCacheKey(const fs::path& absPath, uint64_t hash)
{
    std::ostringstream oss;
    oss << PathUtils::PathToUtf8(absPath.filename()) << '|'
        << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

// Returns a parsed GDTF cache entry, reusing content hashes when file metadata is unchanged.
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
    if (g_gdtfCacheShutdown) {
        setReason("GDTF cache is shut down");
        return nullptr;
    }

    if (gdtfPath.empty())
    {
        setReason("Empty GDTF path");
        return nullptr;
    }

    std::error_code ec;
    fs::path absPath = fs::absolute(PathUtils::PathFromUtf8(gdtfPath), ec);
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

    auto fileSize = fs::file_size(absPath, ec);
    if (ec)
    {
        setReason("Cannot read GDTF file size");
        return nullptr;
    }

    uint64_t hash = 0;
    const std::string hashCacheKey = BuildGdtfHashCacheKey(absPath, timestamp, fileSize);
    auto hashIt = g_gdtfHashCache.find(hashCacheKey);
    if (hashIt != g_gdtfHashCache.end()) {
        hash = hashIt->second.contentHash;
    } else {
        if (!ComputeGdtfContentHash(absPath, hash)) {
            setReason("Cannot open GDTF file for hashing");
            return nullptr;
        }
        // This metadata-keyed hash cache avoids rereading unchanged archives while retaining content-hash invalidation when file metadata changes.
        g_gdtfHashCache[hashCacheKey] = CachedGdtfHash{hash};
    }

    std::string stableKey = BuildGdtfStableCacheKey(absPath, hash);
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

    auto& cache = GdtfCache();
    auto it = cache.find(stableKey);
    if (it != cache.end()) {
        if (it->second.doc && it->second.fixtureType) {
            if (fromCache)
                *fromCache = true;
            return &it->second;
        }

        cache.erase(it);
    }

    GdtfCacheEntry entry;
    entry.timestamp = timestamp;
    TempExtraction extraction(PathUtils::PathToUtf8(absPath));
    if (!extraction.IsValid()) {
        g_failedGdtfCache[stableKey] = timestamp;
        setReason("Unable to extract GDTF archive (corrupted or unreadable file)");
        g_gdtfFailureReasons[stableKey] = failureReason ? *failureReason : "unknown error";
        return nullptr;
    }
    entry.extractedDir = extraction.Path();
    entry.extractionLease = extraction.ReleaseLease();

    entry.doc = std::make_unique<tinyxml2::XMLDocument>();
    std::string descPath = FindExtractedDescriptionPath(entry.extractedDir);
    if (descPath.empty()) {
        entry.extractionLease.reset();
        g_failedGdtfCache[stableKey] = timestamp;
        setReason("Missing or ambiguous description.xml inside GDTF file");
        g_gdtfFailureReasons[stableKey] = failureReason ? *failureReason : "unknown error";
        return nullptr;
    }
    if (!ParseXmlWithEscapedControlFallback(descPath, *entry.doc)) {
        entry.extractionLease.reset();
        g_failedGdtfCache[stableKey] = timestamp;
        setReason("Missing or invalid description.xml inside GDTF file");
        g_gdtfFailureReasons[stableKey] = failureReason ? *failureReason : "unknown error";
        return nullptr;
    }

    entry.fixtureType = GetFixtureType(*entry.doc);
    if (!entry.fixtureType) {
        entry.extractionLease.reset();
        g_failedGdtfCache[stableKey] = timestamp;
        setReason("GDTF description.xml is missing a <FixtureType> element");
        g_gdtfFailureReasons[stableKey] = failureReason ? *failureReason : "unknown error";
        return nullptr;
    }

    entry.fixtureName = GetFixtureNameFromXml(entry.fixtureType);
    if (const char* manufacturer = entry.fixtureType->Attribute("Manufacturer"))
        entry.fixtureManufacturer = manufacturer;
    ParseModes(entry.fixtureType, entry.modes, entry.modeChannels, entry.modeChannelCounts);
    ParseProperties(entry.fixtureType, entry.weightKg, entry.powerW);
    entry.propertiesParsed = true;
    entry.modelColor = ParseModelColor(entry.fixtureType);
    entry.modelColorParsed = true;

    auto res = cache.emplace(stableKey, std::move(entry));
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
            case GdtfNodeType::Magnet:
                return std::string("Magnet_") + stableToken;
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
    else if (nodeType == "Magnet")
        parsedNodeType = GdtfNodeType::Magnet;

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
                            bool tried3ds = false;
                            bool triedGlb = false;
                            const fs::path loadedPath = PathUtils::PathFromUtf8(path);
                            std::string fallbackGlbPath;

                            if (HasExtension(loadedPath, ".3ds")) {
                                tried3ds = true;
                                std::string loadError3ds;
                                loaded = Load3DS(path, mesh, false, &loadError3ds);
                                if (!loaded) {
                                    fallbackGlbPath = FindModelFile(baseDir, loadedPath.stem().string());
                                    if (!fallbackGlbPath.empty() && HasExtension(PathUtils::PathFromUtf8(fallbackGlbPath), ".glb")) {
                                        triedGlb = true;
                                        std::string loadErrorGlbFallback;
                                        loaded = LoadGLB(fallbackGlbPath, mesh, &loadErrorGlbFallback);
                                        if (loaded)
                                            path = fallbackGlbPath;
                                    }
                                }
                            } else if (HasExtension(loadedPath, ".glb")) {
                                triedGlb = true;
                                std::string loadErrorGlb;
                                loaded = LoadGLB(path, mesh, &loadErrorGlb);
                            }

                            if (loaded) {
                                ApplyModelDimensions(mesh, modelInfo);
                                mit = meshCache.emplace(path, std::move(mesh)).first;
                            } else {
                                bool shouldLog = true;
                                if (failedModelLoads)
                                    shouldLog = failedModelLoads->insert(path).second;

                                if (shouldLog && ConsolePanel::Instance()) {
                                    wxString msg;
                                    if (tried3ds && triedGlb) {
                                        msg = wxString::Format(
                                            "GDTF: failed to load model %s and fallback %s (unsupported or empty geometry)",
                                            wxString::FromUTF8(path),
                                            wxString::FromUTF8(fallbackGlbPath));
                                    } else {
                                        msg = wxString::Format("GDTF: failed to load model %s (unsupported or empty geometry)", wxString::FromUTF8(path));
                                    }
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
                if (GetCachedPrimitiveMesh(modelInfo, meshCache, mesh))
                    haveMesh = true;
            }

            if (!haveMesh && (modelInfo.length > 0.0f || modelInfo.width > 0.0f || modelInfo.height > 0.0f)) {
                if (BuildDimensionFallbackMesh(modelInfo, mesh)) {
                    haveMesh = true;
                    if (ConsolePanel::Instance())
                        ConsolePanel::Instance()->AppendMessage(wxString::Format("GDTF: using dimension box fallback for model %s", wxString::FromUTF8(modelInfo.file)));
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

// Collects GeometryReference targets from a geometry subtree.
static void CollectReferencedGeometryNames(tinyxml2::XMLElement* node,
                                           std::unordered_set<std::string>& referencedNames)
{
    if (!node)
        return;
    if (std::string_view(node->Name()) == "GeometryReference") {
        if (const char* geometry = node->Attribute("Geometry"); geometry && !IsBlank(geometry))
            referencedNames.insert(geometry);
    }
    for (tinyxml2::XMLElement* child = node->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        CollectReferencedGeometryNames(child, referencedNames);
    }
}

// Resolves render roots from the selected DMX mode and standards-based fallbacks.
static std::vector<tinyxml2::XMLElement*> ResolveGeometryRoots(
    tinyxml2::XMLElement* fixtureType,
    tinyxml2::XMLElement* geometries,
    const std::unordered_map<std::string, tinyxml2::XMLElement*>& geometryMap,
    const std::string& modeName)
{
    auto resolveModeRoot = [&](tinyxml2::XMLElement* mode) -> tinyxml2::XMLElement* {
        if (!mode)
            return nullptr;
        const char* geometry = mode->Attribute("Geometry");
        if (!geometry || IsBlank(geometry))
            return nullptr;
        auto geometryIt = geometryMap.find(geometry);
        return geometryIt != geometryMap.end() ? geometryIt->second : nullptr;
    };

    tinyxml2::XMLElement* dmxModes = fixtureType->FirstChildElement("DMXModes");
    if (dmxModes && !modeName.empty()) {
        for (tinyxml2::XMLElement* mode = dmxModes->FirstChildElement("DMXMode"); mode;
             mode = mode->NextSiblingElement("DMXMode")) {
            const char* name = mode->Attribute("Name");
            if (name && modeName == name) {
                if (tinyxml2::XMLElement* root = resolveModeRoot(mode))
                    return {root};
                break;
            }
        }
    }

    if (dmxModes) {
        for (tinyxml2::XMLElement* mode = dmxModes->FirstChildElement("DMXMode"); mode;
             mode = mode->NextSiblingElement("DMXMode")) {
            if (tinyxml2::XMLElement* root = resolveModeRoot(mode))
                return {root};
        }
    }

    std::unordered_set<std::string> referencedNames;
    for (tinyxml2::XMLElement* geometry = geometries->FirstChildElement(); geometry;
         geometry = geometry->NextSiblingElement()) {
        CollectReferencedGeometryNames(geometry, referencedNames);
    }

    std::vector<tinyxml2::XMLElement*> roots;
    for (tinyxml2::XMLElement* geometry = geometries->FirstChildElement(); geometry;
         geometry = geometry->NextSiblingElement()) {
        const char* name = geometry->Attribute("Name");
        if (!name || referencedNames.find(name) == referencedNames.end())
            roots.push_back(geometry);
    }
    if (!roots.empty())
        return roots;

    for (tinyxml2::XMLElement* geometry = geometries->FirstChildElement(); geometry;
         geometry = geometry->NextSiblingElement()) {
        roots.push_back(geometry);
    }
    return roots;
}

// Builds the complete top-level geometry lookup used by GeometryReference nodes.
static std::unordered_map<std::string, tinyxml2::XMLElement*> BuildGeometryMap(
    tinyxml2::XMLElement* geometries)
{
    std::unordered_map<std::string, tinyxml2::XMLElement*> geometryMap;
    for (tinyxml2::XMLElement* geometry = geometries->FirstChildElement(); geometry;
         geometry = geometry->NextSiblingElement()) {
        if (const char* name = geometry->Attribute("Name"); name && !IsBlank(name))
            geometryMap[name] = geometry;
    }
    return geometryMap;
}


// Parses model definitions from the fixture XML into renderable model metadata.
static std::unordered_map<std::string, GdtfModelInfo> BuildModelInfoMap(GdtfCacheEntry& entry)
{
    std::unordered_map<std::string, GdtfModelInfo> models;
    tinyxml2::XMLElement* ft = entry.fixtureType;
    if (!ft)
        return models;
    if (tinyxml2::XMLElement* modelList = ft->FirstChildElement("Models")) {
        for (tinyxml2::XMLElement* m = modelList->FirstChildElement("Model");
             m; m = m->NextSiblingElement("Model")) {
            const char* name = m->Attribute("Name");
            const char* file = m->Attribute("File");
            const char* primitiveType = m->Attribute("PrimitiveType");
            const bool hasName = name && !IsBlank(name);
            const bool hasFile = file && !IsBlank(file);
            const std::string primitive = primitiveType ? primitiveType : "";
            const bool hasPrimitive = IsPrimitiveTypeDefined(primitive);
            if (!hasName)
                continue;

            GdtfModelInfo info;
            info.name = name;
            if (hasFile)
                info.file = file;
            info.primitiveType = primitive;
            m->QueryFloatAttribute("Length", &info.length);
            m->QueryFloatAttribute("Width", &info.width);
            m->QueryFloatAttribute("Height", &info.height);
            const bool hasDimensions = info.length > 0.0f || info.width > 0.0f ||
                                       info.height > 0.0f;
            if (hasName && !hasFile && !hasPrimitive && !hasDimensions) {
                if (ConsolePanel::Instance() && entry.emptyModelFileLogged.insert(name).second) {
                    wxString msg = wxString::Format(
                        "GDTF: Model %s has no file, primitive, or positive dimensions",
                        wxString::FromUTF8(name));
                    ConsolePanel::Instance()->AppendMessage(msg);
                }
                continue;
            }
            models[name] = info;
        }
    }
    return models;
}

// Builds or reuses the canonical geometry tree for one cached GDTF and DMX mode.
static bool BuildGdtfGeometryTreeFromCache(GdtfCacheEntry& entry,
                                           const std::string& modeName,
                                           GdtfGeometryTree& outTree)
{
    const std::string modeKey = modeName.empty() ? std::string("<default>") : modeName;
    auto cached = entry.geometryTreeCache.find(modeKey);
    if (cached != entry.geometryTreeCache.end()) {
        outTree = cached->second;
        return !outTree.nodes.empty();
    }

    GdtfGeometryTree built;
    const auto models = BuildModelInfoMap(entry);
    if (tinyxml2::XMLElement* geoms = entry.fixtureType->FirstChildElement("Geometries")) {
        const auto geomMap = BuildGeometryMap(geoms);
        const auto roots = ResolveGeometryRoots(entry.fixtureType, geoms, geomMap, modeName);
        for (tinyxml2::XMLElement* g : roots) {
            ParseGeometry(g, MatrixUtils::Identity(), models, entry.extractedDir, geomMap,
                          entry.meshCache, built, &entry.missingModelsLogged,
                          &entry.failedModelLoads, -1);
        }
    }

    outTree = built;
    entry.geometryTreeCache[modeKey] = std::move(built);
    return !outTree.nodes.empty();
}

// Derives flat render objects from the canonical geometry tree for one DMX mode.
static bool BuildGdtfFlatObjectsFromCache(GdtfCacheEntry& entry,
                                          const std::string& modeName,
                                          std::vector<GdtfObject>& outObjects)
{
    const std::string modeKey = modeName.empty() ? std::string("<default>") : modeName;
    auto cached = entry.flatObjectCache.find(modeKey);
    if (cached != entry.flatObjectCache.end()) {
        outObjects = cached->second;
        return !outObjects.empty();
    }

    GdtfGeometryTree tree;
    if (!BuildGdtfGeometryTreeFromCache(entry, modeName, tree))
        return false;

    std::vector<GdtfObject> flat;
    for (const GdtfNode3D& node : tree.nodes) {
        if (node.hasMesh)
            flat.push_back({node.mesh, node.worldTransform, node.isLens});
    }
    outObjects = flat;
    entry.flatObjectCache[modeKey] = std::move(flat);
    return !outObjects.empty();
}

// Releases cached GDTF documents and extraction leases during controlled application shutdown.
void ShutdownGdtfCache() noexcept
{
    try {
        std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);
        g_gdtfCacheShutdown = true;
        GdtfCache().clear();
        g_failedGdtfCache.clear();
        g_gdtfFailedAttempts.clear();
        g_gdtfFailureReasons.clear();
        g_gdtfHashCache.clear();
    } catch (...) {
    }
}

// Loads the default GDTF geometry hierarchy using the first usable DMX mode.
bool LoadGdtfGeometryTree(const std::string& gdtfPath,
                          GdtfGeometryTree& outTree,
                          std::string* outError)
{
    return LoadGdtfGeometryTree(gdtfPath, outTree, std::string(), outError);
}

// Loads the GDTF geometry hierarchy for the requested DMX mode.
bool LoadGdtfGeometryTree(const std::string& gdtfPath,
                          GdtfGeometryTree& outTree,
                          const std::string& modeName,
                          std::string* outError)
{
    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

    outTree.nodes.clear();
    outTree.axisNodeIndices.clear();
    outTree.emitterNodeIndices.clear();
    if (outError)
        outError->clear();

    bool cachedFailure = false;
    bool fromCache = false;
    std::string failureReason;
    std::string cacheKey;
    GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath, &cachedFailure, &fromCache,
                                          &failureReason, &cacheKey);
    if (!entry || !entry->fixtureType) {
        if (outError)
            *outError = failureReason.empty() ? "unknown error" : failureReason;
        return false;
    }

    if (!BuildGdtfGeometryTreeFromCache(*entry, modeName, outTree)) {
        if (outError)
            *outError = "No geometry with models found";
        return false;
    }
    return true;
}

// Loads the default GDTF models using the first usable DMX mode.
bool LoadGdtf(const std::string& gdtfPath,
              std::vector<GdtfObject>& outObjects,
              std::string* outError)
{
    return LoadGdtf(gdtfPath, outObjects, std::string(), outError);
}

// Loads the GDTF models for the requested DMX mode.
bool LoadGdtf(const std::string& gdtfPath,
              std::vector<GdtfObject>& outObjects,
              const std::string& modeName,
              std::string* outError)
{
    try {
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

    if (!BuildGdtfFlatObjectsFromCache(*entry, modeName, outObjects)) {
        constexpr const char* kEmptyGeometryReason = "No geometry with models found";
        size_t count = ++entry->emptyGeometryLogCount;

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

    if (ConsolePanel::Instance() && !fromCache) {
        wxString msg = wxString::Format("GDTF: loaded %zu objects from %s",
                                       outObjects.size(),
                                       wxString::FromUTF8(gdtfPath));
        ConsolePanel::Instance()->AppendMessage(msg);
    }

    return true;
    } catch (const std::exception& error) {
        outObjects.clear();
        if (outError)
            *outError = error.what();
        return false;
    } catch (...) {
        outObjects.clear();
        if (outError)
            *outError = "Unknown error while loading GDTF.";
        return false;
    }
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
    try {
        std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

        std::vector<std::string> result;
        if (gdtfPath.empty())
            return result;

        GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
        if (!entry)
            return result;

        return entry->modes;
    } catch (...) {
        return {};
    }
}


// Returns the cached extraction directory for a GDTF file when available.
std::string GetCachedGdtfExtractionDirectory(const std::string& gdtfPath)
{
    try {
        std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);
        GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
        if (!entry)
            return {};
        return entry->extractedDir;
    } catch (...) {
        return {};
    }
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
    try {
        std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

        if (gdtfPath.empty())
            return {};

        GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
        if (!entry)
            return {};

        return entry->fixtureName;
    } catch (...) {
        return {};
    }
}

// Returns the manufacturer declared by the fixture type in a GDTF file.
std::string GetGdtfFixtureManufacturer(const std::string& gdtfPath)
{
    try {
        std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);
        if (gdtfPath.empty())
            return {};
        GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
        return entry ? entry->fixtureManufacturer : std::string{};
    } catch (...) {
        return {};
    }
}

bool GetGdtfProperties(const std::string& gdtfPath,
                       float& outWeightKg,
                       float& outPowerW)
{
    outWeightKg = 0.0f;
    outPowerW = 0.0f;
    try {
        std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

        if (gdtfPath.empty())
            return false;

        GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
        if (!entry)
            return false;

        outWeightKg = entry->weightKg;
        outPowerW = entry->powerW;
        return entry->propertiesParsed;
    } catch (...) {
        outWeightKg = 0.0f;
        outPowerW = 0.0f;
        return false;
    }
}

std::string GetGdtfModelColor(const std::string& gdtfPath)
{
    try {
        std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);

        if (gdtfPath.empty())
            return {};

        GdtfCacheEntry* entry = GetCachedGdtf(gdtfPath);
        if (!entry)
            return {};

        return entry->modelColor;
    } catch (...) {
        return {};
    }
}

namespace {

struct ArchivePublicationResult {
    bool success = false;
    std::string stage;
    std::string diagnostic;
    fs::path tempPath;
};

// Reports whether the archive-relative path is portable and safe to publish.
bool IsSafeArchiveRelativePath(const fs::path& path)
{
    const std::string value = path.generic_string();
    if (value.empty() || value.front() == '/' || value.find('\\') != std::string::npos)
        return false;
    std::stringstream stream(value);
    std::string component;
    while (std::getline(stream, component, '/')) {
        if (component.empty() || component == "." || component == "..")
            return false;
    }
    return true;
}

// Creates a unique temporary archive path next to the target archive.
fs::path MakeUniqueSiblingArchivePath(const fs::path& targetPath)
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::random_device device;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const fs::path candidate = targetPath.parent_path() /
            (targetPath.filename().string() + ".tmp." + std::to_string(now) + "." +
             std::to_string(device()) + "." + std::to_string(attempt));
        std::error_code ec;
        if (!fs::exists(candidate, ec))
            return candidate;
    }
    return targetPath.parent_path() / (targetPath.filename().string() + ".tmp");
}

// Removes a temporary archive path without throwing during cleanup.
void RemoveTemporaryArchive(const fs::path& path)
{
    if (path.empty())
        return;
    std::error_code ec;
    fs::remove(path, ec);
}

// Invokes an optional publication hook before a named stage.
bool AllowPublicationStage(const GdtfDocumentMutationPublicationHooks* hooks,
                           const std::string& stage,
                           ArchivePublicationResult& result)
{
    if (!hooks || !hooks->beforeStage)
        return true;
    std::string error;
    if (hooks->beforeStage(stage, error))
        return true;
    result.stage = stage;
    result.diagnostic = error.empty() ? "publication stage was rejected by test hook" : error;
    return false;
}

// Writes a directory into a temporary GDTF archive with checked ZIP operations.
ArchivePublicationResult WriteDirectoryArchive(const std::string& srcDir,
                                               const fs::path& targetPath,
                                               const GdtfDocumentMutationPublicationHooks* hooks)
{
    ArchivePublicationResult result;
    result.tempPath = MakeUniqueSiblingArchivePath(targetPath);

    std::vector<fs::directory_entry> files;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(srcDir, ec), end; it != end; it.increment(ec)) {
        if (ec) {
            result.stage = "EnumerateArchiveEntries";
            result.diagnostic = ec.message();
            return result;
        }
        if (it->is_regular_file(ec) && !ec)
            files.push_back(*it);
    }
    std::sort(files.begin(), files.end(), [](const fs::directory_entry& left,
                                             const fs::directory_entry& right) {
        return left.path().generic_string() < right.path().generic_string();
    });

    if (!AllowPublicationStage(hooks, "BeforeOpenTemporaryArchive", result))
        return result;
    wxFileOutputStream output(result.tempPath.string());
    if (!output.IsOk()) {
        result.stage = "OpenTemporaryArchive";
        result.diagnostic = "could not open temporary GDTF archive for writing";
        return result;
    }

    wxZipOutputStream zip(output);
    for (const auto& file : files) {
        const fs::path rel = fs::relative(file.path(), srcDir, ec);
        if (ec || !IsSafeArchiveRelativePath(rel)) {
            result.stage = "ValidateArchiveEntryPath";
            result.diagnostic = ec ? ec.message() : ("unsafe archive entry path: " + rel.generic_string());
            return result;
        }
        if (!AllowPublicationStage(hooks, "BeforePutNextEntry", result)) {
            return result;
        }
        auto* entry = new wxZipEntry(rel.generic_string());
        entry->SetMethod(wxZIP_METHOD_DEFLATE);
        if (!zip.PutNextEntry(entry)) {
            result.stage = "PutNextEntry";
            result.diagnostic = "could not create archive entry: " + rel.generic_string();
            return result;
        }
        std::ifstream input(file.path(), std::ios::binary);
        if (!input.is_open()) {
            result.stage = "OpenArchiveInput";
            result.diagnostic = "could not open archive input: " + file.path().string();
            return result;
        }
        char buffer[4096];
        while (input.good()) {
            input.read(buffer, sizeof(buffer));
            const std::streamsize count = input.gcount();
            if (count <= 0)
                continue;
            if (!AllowPublicationStage(hooks, "BeforeWriteEntryBytes", result)) {
                return result;
            }
            zip.Write(buffer, count);
            if (zip.LastWrite() != static_cast<size_t>(count)) {
                result.stage = "WriteEntryBytes";
                result.diagnostic = "could not write archive entry bytes: " + rel.generic_string();
                return result;
            }
        }
        if (!input.eof()) {
            result.stage = "ReadArchiveInput";
            result.diagnostic = "could not read archive input: " + file.path().string();
            return result;
        }
        if (!zip.CloseEntry()) {
            result.stage = "CloseArchiveEntry";
            result.diagnostic = "could not close archive entry: " + rel.generic_string();
            return result;
        }
    }
    if (!AllowPublicationStage(hooks, "BeforeCloseTemporaryArchive", result)) {
        return result;
    }
    const bool zipClosed = zip.Close();
    const bool outputClosed = output.Close();
    if (!zipClosed || !outputClosed) {
        result.stage = "CloseTemporaryArchive";
        result.diagnostic = "could not close temporary GDTF archive";
        return result;
    }
    result.success = true;
    return result;
}

} // namespace

namespace {

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


// Reports whether requested GDTF physical-property mutations are finite.
static bool ValidateFinitePhysicalPropertyInputs(
    const GdtfDocumentMutationRequest& request, std::vector<std::string>& errors)
{
    bool valid = true;
    if (request.weightSet && !std::isfinite(request.weightKg)) {
        errors.push_back("GDTF mutation rejected non-finite Weight value");
        valid = false;
    }
    if (request.powerSet && !std::isfinite(request.powerW)) {
        errors.push_back("GDTF mutation rejected non-finite PowerConsumption value");
        valid = false;
    }
    return valid;
}

// Replaces the target archive with a completed sibling archive.
static bool ReplaceArchiveAtomically(const fs::path& tempPath, const fs::path& targetPath,
                                     std::string& error)
{
    std::error_code ec;
#ifdef _WIN32
    const std::wstring tempWide = tempPath.wstring();
    const std::wstring targetWide = targetPath.wstring();
    const BOOL moved = MoveFileExW(tempWide.c_str(), targetWide.c_str(),
                                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!moved)
        ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    fs::rename(tempPath, targetPath, ec);
#endif
    if (ec) {
        error = ec.message();
        fs::remove(tempPath, ec);
        return false;
    }
    return true;
}

// Mutates selected FixtureType document fields in one extracted archive transaction.
GdtfDocumentMutationResult MutateGdtfDocumentWithResult(
    const std::string& gdtfPath,
    const GdtfDocumentMutationRequest& request,
    const std::string& modifiedByProgram,
    const GdtfDocumentMutationPublicationHooks* publicationHooks)
{
    GdtfDocumentMutationResult result;
    result.publicationPath = gdtfPath;
    if (gdtfPath.empty()) {
        result.errors.push_back("GDTF path is empty");
        return result;
    }
    if (!ValidateFinitePhysicalPropertyInputs(request, result.errors))
        return result;

    TempExtraction extraction(gdtfPath);
    if (!extraction.IsValid()) {
        result.errors.push_back("Could not extract GDTF archive");
        return result;
    }

    const std::string descPath = extraction.Path() + "/description.xml";
    tinyxml2::XMLDocument doc;
    if (!ParseXmlWithEscapedControlFallback(descPath, doc)) {
        result.errors.push_back("Could not parse description.xml");
        return result;
    }

    tinyxml2::XMLElement* fixtureType = GdtfMutationAudit::EnsureFixtureType(doc);
    if (!fixtureType) {
        result.errors.push_back("description.xml is missing FixtureType");
        return result;
    }

    bool mutated = false;
    if (request.descriptionSet) {
        fixtureType->SetAttribute("Description", request.description.c_str());
        mutated = true;
    }
    if (request.weightSet || request.powerSet) {
        mutated = GdtfMutationAudit::ApplyPhysicalProperties(
                      fixtureType, doc,
                      request.weightSet ? std::optional<float>(request.weightKg) : std::nullopt,
                      request.powerSet ? std::optional<float>(request.powerW) : std::nullopt) ||
                  mutated;
    }
    if (!mutated) {
        result.success = true;
        return result;
    }

    GdtfMutationAudit::AppendRevision(
        fixtureType, doc,
        request.revisionText.empty()
            ? "Updated GDTF document fields from Perastage"
            : request.revisionText,
        modifiedByProgram);

    GdtfCanonicalizer::Options canonicalOptions;
    canonicalOptions.allowFixtureTypeIdRepair = true;
    canonicalOptions.stableIdSeed = gdtfPath;
    canonicalOptions.sourceLabel = gdtfPath;
    const GdtfCanonicalizer::Result canonicalResult =
        GdtfCanonicalizer::CanonicalizeDescription(doc, canonicalOptions);
    result.changed = canonicalResult.changed || mutated;
    result.warnings = canonicalResult.warnings;
    if (!canonicalResult.success) {
        result.errors = canonicalResult.errors;
        return result;
    }

    if (doc.SaveFile(descPath.c_str()) != tinyxml2::XML_SUCCESS) {
        result.errors.push_back("Could not save canonical description.xml");
        return result;
    }

    const fs::path targetPath(gdtfPath);
    ArchivePublicationResult publication = WriteDirectoryArchive(extraction.Path(), targetPath, publicationHooks);
    if (!publication.success) {
        result.errors.push_back("GDTF publication failed at " + publication.stage + ": " +
                                publication.diagnostic);
        RemoveTemporaryArchive(publication.tempPath);
        return result;
    }

    ArchivePublicationResult hookResult;
    if (!AllowPublicationStage(publicationHooks, "BeforeAtomicReplace", hookResult)) {
        result.errors.push_back("GDTF publication failed at " + hookResult.stage + ": " +
                                hookResult.diagnostic);
        RemoveTemporaryArchive(publication.tempPath);
        return result;
    }

    std::string replaceError;
    if (!ReplaceArchiveAtomically(publication.tempPath, targetPath, replaceError)) {
        result.errors.push_back("GDTF publication failed at AtomicReplace: " + replaceError);
        RemoveTemporaryArchive(publication.tempPath);
        return result;
    }
    result.atomicReplacementCompleted = true;

    std::lock_guard<std::recursive_mutex> lock(g_gdtfCacheMutex);
    GdtfCache().erase(gdtfPath);
    result.success = true;
    return result;
}

// Mutates selected FixtureType document fields through the compatibility boolean API.
bool MutateGdtfDocument(const std::string& gdtfPath,
                        const GdtfDocumentMutationRequest& request,
                        const std::string& modifiedByProgram)
{
    return MutateGdtfDocumentWithResult(gdtfPath, request, modifiedByProgram).success;
}

// Updates GDTF physical properties through the compatibility boolean API.
bool SetGdtfProperties(const std::string& gdtfPath,
                       float weightKg,
                       float powerW,
                       const std::string& modifiedByProgram)
{
    GdtfDocumentMutationRequest request;
    request.weightSet = true;
    request.weightKg = weightKg;
    request.powerSet = true;
    request.powerW = powerW;
    request.revisionText =
        "Updated fixture physical properties (Weight/PowerConsumption) from Perastage";
    return MutateGdtfDocument(gdtfPath, request, modifiedByProgram);
}
