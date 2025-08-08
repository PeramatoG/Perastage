#include "riderimporter.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <random>
#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#include <podofo/podofo.h>

#include "configmanager.h"
#include "fixture.h"
#include "truss.h"

using namespace PoDoFo;

namespace {
// Generate a random UUID4 string
std::string GenerateUuid() {
    static std::mt19937_64 rng{std::random_device{}()};
    static std::uniform_int_distribution<int> dist(0, 15);
    const char *v = "0123456789abcdef";
    int groups[] = {8,4,4,4,12};
    std::string out;
    for (int g=0; g<5; ++g) {
        if (g) out.push_back('-');
        for (int i=0;i<groups[g];++i)
            out.push_back(v[dist(rng)]);
    }
    return out;
}

std::string ReadTextFile(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs)
        return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

std::string ExtractPdfText(const std::string &path) {
    try {
        PdfMemDocument doc;
        doc.Load(path.c_str());
        std::string out;
#if PODOFO_VERSION >= PODOFO_MAKE_VERSION(0,10,0)
        auto &pages = doc.GetPages();
        for (unsigned i = 0; i < pages.GetCount(); ++i) {
            auto &page = pages.GetPageAt(i);
            PdfContentStreamReader reader(page);
            PdfContent content;
            while (reader.TryReadNext(content)) {
                if (content.GetType() != PdfContentType::Operator)
                    continue;
                const auto &keyword = content.GetKeyword();
                const auto &stack = content.GetStack();
                if (stack.GetSize() == 0)
                    continue;
                const PdfVariant &var = stack[0];
                if ((keyword == "Tj" || keyword == "'" || keyword == "\"") && var.IsString()) {
                    out += std::string(var.GetString().GetString());
                    out += '\n';
                } else if (keyword == "TJ" && var.IsArray()) {
                    const PdfArray &arr = var.GetArray();
                    for (const auto &el : arr) {
                        if (el.IsString())
                            out += std::string(el.GetString().GetString());
                    }
                    out += '\n';
                }
            }
            out += '\n';
        }
#else
        for (int i = 0; i < doc.GetPageCount(); ++i) {
            PdfPage *page = doc.GetPage(i);
            PdfContentsTokenizer tokenizer(page);
            EPdfContentsType type;
            const char *token = nullptr;
            PdfVariant var;
            std::string lastString;
            while (tokenizer.ReadNext(type, token, var)) {
                if (type == ePdfContentsType_Variant && var.IsString()) {
                    lastString = var.GetString().GetStringUtf8();
                } else if (type == ePdfContentsType_Variant && var.IsArray()) {
                    const PdfArray &arr = var.GetArray();
                    for (const auto &el : arr) {
                        if (el.IsString())
                            out += el.GetString().GetStringUtf8();
                    }
                } else if (type == ePdfContentsType_Keyword) {
                    if ((!strcmp(token, "Tj") || !strcmp(token, "'") || !strcmp(token, "\"") ) && !lastString.empty()) {
                        out += lastString;
                        out += '\n';
                        lastString.clear();
                    } else if (!strcmp(token, "TJ")) {
                        out += '\n';
                    }
                }
            }
            out += '\n';
        }
#endif
        if (!out.empty())
            return out;
    } catch (const PdfError &) {
        // fall through to pdftotext fallback
    }

    // Fallback to the external "pdftotext" command if PoDoFo failed
    std::string cmd = "pdftotext -layout \"" + path + "\" -";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return {};
    char buffer[256];
    std::string out;
    while (fgets(buffer, sizeof(buffer), pipe))
        out += buffer;
    pclose(pipe);
    return out;
}
} // namespace

bool RiderImporter::Import(const std::string &path) {
    std::string text;
    if (path.size() >= 4) {
        std::string ext = path.substr(path.size() - 4);
        for (auto &c : ext)
            c = static_cast<char>(std::tolower(c));
        if (ext == ".txt")
            text = ReadTextFile(path);
        else if (ext == ".pdf")
            text = ExtractPdfText(path);
    }
    if (text.empty())
        return false;

    ConfigManager &cfg = ConfigManager::Get();
    auto &scene = cfg.GetScene();
    std::string layer = cfg.GetCurrentLayer();

    std::regex trussRe("truss[^\n]*?(\\d+(?:\\.\\d+)?)\\s*m", std::regex::icase);
    std::regex fixtureLineRe("^(\\d+)\\s+(.+)$");
    std::istringstream iss(text);
    std::string line;
    bool inFixtures = false;
    bool inRigging = false;
    while (std::getline(iss, line)) {
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower.find("ilumin") != std::string::npos ||
            lower.find("robotica") != std::string::npos ||
            lower.find("convencion") != std::string::npos) {
            inFixtures = true;
            inRigging = false;
            continue;
        }
        if (lower.find("rigging") != std::string::npos) {
            inFixtures = false;
            inRigging = true;
            continue;
        }
        if (lower.find("sonido") != std::string::npos ||
            lower.find("audio") != std::string::npos ||
            lower.find("control de p.a.") != std::string::npos ||
            lower.find("monitores") != std::string::npos ||
            lower.find("microfon") != std::string::npos ||
            lower.find("video") != std::string::npos ||
            lower.find("pantalla") != std::string::npos ||
            lower.find("realizacion") != std::string::npos) {
            inFixtures = false;
            inRigging = false;
            continue;
        }

        std::smatch m;
        if (inRigging && std::regex_search(lower, m, trussRe)) {
            Truss t;
            t.uuid = GenerateUuid();
            t.name = "Truss";
            t.layer = layer;
            t.lengthMm = std::stof(m[1]) * 1000.0f;
            scene.trusses[t.uuid] = t;
        } else if (inFixtures && std::regex_match(line, m, fixtureLineRe)) {
            int quantity = std::stoi(m[1]);
            std::string desc = m[2];
            for (int i = 0; i < quantity; ++i) {
                Fixture f;
                f.uuid = GenerateUuid();
                f.instanceName = desc + " " + std::to_string(i + 1);
                f.typeName = "Dummy";
                f.layer = layer;
                scene.fixtures[f.uuid] = f;
            }
        }
    }

    cfg.PushUndoState("import rider");
    return true;
}

