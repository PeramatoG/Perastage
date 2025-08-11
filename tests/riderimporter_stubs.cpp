#include <string>
#include <optional>
#include <unordered_map>

#include "../core/gdtfdictionary.h"
#include "../core/trussdictionary.h"
#include "../core/trussloader.h"
#include "../core/pdftext.h"
#include "../mvr/mvrimporter.h"
#include "../mvr/mvrexporter.h"
#include "../models/truss.h"

std::string ExtractPdfText(const std::string &) { return {}; }

namespace GdtfDictionary {
std::optional<std::unordered_map<std::string, Entry>> Load() { return std::unordered_map<std::string, Entry>(); }
void Save(const std::unordered_map<std::string, Entry> &) {}
std::optional<Entry> Get(const std::string &) { return std::nullopt; }
void Update(const std::string &, const std::string &, const std::string &) {}
}

namespace TrussDictionary {
std::optional<std::unordered_map<std::string, std::string>> Load() { return std::unordered_map<std::string, std::string>(); }
void Save(const std::unordered_map<std::string, std::string> &) {}
std::optional<std::string> Get(const std::string &) { return std::nullopt; }
void Update(const std::string &, const std::string &) {}
}

bool LoadTrussArchive(const std::string &, Truss &) { return false; }

bool MvrImporter::ImportFromFile(const std::string &, bool, bool) { return false; }
bool MvrImporter::ImportAndRegister(const std::string &, bool, bool) { return false; }
bool MvrExporter::ExportToFile(const std::string &) { return false; }
