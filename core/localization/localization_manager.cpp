#include "localization/localization_manager.h"

#include "diagnostics/DiagnosticLogger.h"

#include <clocale>
#include <cstdlib>
#include <sstream>
#include <string>
#include <system_error>

#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>

namespace localization {
namespace {
constexpr const char *kCatalogName = "perastage";

// Converts a wxString path to a filesystem path.
std::filesystem::path WxPathToFilesystemPath(const wxString &value) {
  const wxScopedCharBuffer utf8 = value.ToUTF8();
  if (utf8)
    return std::filesystem::u8path(std::string(utf8.data(), utf8.length()));
  return std::filesystem::path(value.ToStdString());
}

// Adds a path to the candidate list when it is not already present.
void AddUniquePath(std::vector<std::filesystem::path> &paths,
                   std::filesystem::path path) {
  if (path.empty())
    return;
  std::error_code ec;
  path = std::filesystem::weakly_canonical(path, ec);
  if (ec)
    path = path.lexically_normal();
  for (const auto &existing : paths) {
    if (existing == path)
      return;
  }
  paths.push_back(std::move(path));
}

} // namespace

// Returns the wxWidgets language id for a supported application language.
int WxLanguageId(AppLanguage language) {
  switch (language) {
  case AppLanguage::Spanish:
    return wxLANGUAGE_SPANISH;
  case AppLanguage::SimplifiedChinese:
    return wxLANGUAGE_CHINESE_SIMPLIFIED;
  case AppLanguage::English:
  default:
    return wxLANGUAGE_ENGLISH;
  }
}

namespace {
// Returns the expected gettext catalog path below a locale root.
std::filesystem::path CatalogPathForRoot(const std::filesystem::path &root,
                                         AppLanguage language) {
  return root / std::string(AppLanguageCode(language)) / "LC_MESSAGES" /
         "perastage.mo";
}

// Converts a path to UTF-8 text for wxWidgets APIs.
std::string PathToUtf8String(const std::filesystem::path &path) {
  const auto pathUtf8 = path.u8string();
  return std::string(pathUtf8.begin(), pathUtf8.end());
}

// Returns a short catalog path suffix suitable for diagnostics.
std::string CatalogPathForLog(const std::filesystem::path &path) {
  std::vector<std::string> parts;
  for (const auto &part : path) {
    const auto partUtf8 = part.u8string();
    if (!partUtf8.empty())
      parts.emplace_back(partUtf8.begin(), partUtf8.end());
  }
  const std::size_t start = parts.size() > 4 ? parts.size() - 4 : 0;
  std::ostringstream out;
  if (start > 0)
    out << ".../";
  for (std::size_t i = start; i < parts.size(); ++i) {
    if (i > start)
      out << '/';
    out << parts[i];
  }
  return out.str();
}

// Joins expected catalog paths into one diagnostic string.
std::string ExpectedCatalogPathsForLog(
    const std::vector<std::filesystem::path> &paths) {
  std::ostringstream out;
  for (std::size_t i = 0; i < paths.size(); ++i) {
    if (i > 0)
      out << "; ";
    out << CatalogPathForLog(paths[i]);
  }
  return out.str();
}

} // namespace

// Returns deterministic locale roots for a supplied executable path and working directory.
std::vector<std::filesystem::path> ResolveLocaleRootCandidatesForPaths(
    const std::filesystem::path &executablePath,
    const std::filesystem::path &workingDirectory) {
  std::vector<std::filesystem::path> roots;
  const std::filesystem::path executableDir = executablePath.empty()
                                                ? std::filesystem::path()
                                                : executablePath.parent_path();
  AddUniquePath(roots, executableDir / "resources" / "locale");
  AddUniquePath(roots, executableDir / "locale");
  AddUniquePath(roots, executableDir / ".." / "share" / "locale");
  AddUniquePath(roots, executableDir / ".." / "Resources" / "locale");
  AddUniquePath(roots, workingDirectory / "resources" / "locale");
  AddUniquePath(roots, workingDirectory / ".." / "resources" / "locale");
  AddUniquePath(roots, workingDirectory / ".." / ".." / "resources" / "locale");
  return roots;
}

// Returns deterministic locale roots for the current runtime layout.
std::vector<std::filesystem::path> ResolveLocaleRootCandidates() {
  const auto executablePath =
      WxPathToFilesystemPath(wxStandardPaths::Get().GetExecutablePath());
  const auto workingDirectory = WxPathToFilesystemPath(wxFileName::GetCwd());
  if (const char *localeRoot = std::getenv("PERASTAGE_LOCALE_ROOT")) {
    if (*localeRoot) {
      std::vector<std::filesystem::path> explicitRoots;
      AddUniquePath(explicitRoots, std::filesystem::u8path(localeRoot));
      return explicitRoots;
    }
  }
  return ResolveLocaleRootCandidatesForPaths(executablePath, workingDirectory);
}

// Returns the process-wide localization manager owned for application lifetime.
LocalizationManager &LocalizationManager::Get() {
  static LocalizationManager manager;
  return manager;
}

// Initializes wxWidgets catalog loading for the selected UI language.
LocalizationInitResult LocalizationManager::Initialize(AppLanguage language) {
  LocalizationInitResult result;
  result.requestedLanguage = language;
  result.activeLanguage = AppLanguage::English;
  locale_.reset();
  activeLanguage_ = AppLanguage::English;

  if (IsSourceLanguage(language)) {
    result.diagnostic = "using built-in English source strings";
    diagnostics::DiagnosticLogger::Info(
        "Localization initialized: requested=en active=en catalog_loaded=1 catalog=builtin");
    return result;
  }

  auto locale = std::make_unique<wxLocale>();
  if (!locale->Init(WxLanguageId(language), wxLOCALE_DONT_LOAD_DEFAULT)) {
    result.diagnostic = "wxLocale initialization failed; using English";
    diagnostics::DiagnosticLogger::Warning(
        "Localization initialized: requested=" +
        std::string(AppLanguageCode(language)) +
        " active=en catalog_found=0 catalog_loaded=0 reason=wxLocale_init_failed");
    return result;
  }

  // UI language setup may adjust process locale categories; keep numeric serialization stable.
  std::setlocale(LC_NUMERIC, "C");

  std::vector<std::filesystem::path> expectedCatalogPaths;
  for (const auto &root : ResolveLocaleRootCandidates()) {
    locale->AddCatalogLookupPathPrefix(wxString::FromUTF8(PathToUtf8String(root)));
    expectedCatalogPaths.push_back(CatalogPathForRoot(root, language));
  }

  std::filesystem::path selectedCatalogPath;
  for (const auto &catalogPath : expectedCatalogPaths) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(catalogPath, ec)) {
      selectedCatalogPath = catalogPath;
      result.catalogFound = true;
      result.catalogPath = CatalogPathForLog(catalogPath);
      break;
    }
  }

  const bool catalogLoaded = locale->AddCatalog(kCatalogName);
  if (!catalogLoaded) {
    result.diagnostic = result.catalogFound
                            ? "translation catalog was found but could not be loaded; using English"
                            : "translation catalog was not found; using English";
    diagnostics::DiagnosticLogger::Warning(
        "Localization initialized: requested=" +
        std::string(AppLanguageCode(language)) +
        " active=en catalog_found=" +
        std::string(result.catalogFound ? "1" : "0") +
        " catalog_loaded=0 catalog_path=" +
        (result.catalogFound ? result.catalogPath : std::string("none")) +
        " expected_paths=" + ExpectedCatalogPathsForLog(expectedCatalogPaths));
    return result;
  }

  activeLanguage_ = language;
  locale_ = std::move(locale);
  result.activeLanguage = language;
  result.catalogLoaded = true;
  if (!result.catalogFound && !expectedCatalogPaths.empty()) {
    result.catalogPath = ExpectedCatalogPathsForLog(expectedCatalogPaths);
  }
  result.diagnostic = "translation catalog loaded";
  diagnostics::DiagnosticLogger::Info(
      "Localization initialized: requested=" +
      std::string(AppLanguageCode(result.requestedLanguage)) + " active=" +
      std::string(AppLanguageCode(result.activeLanguage)) +
      " catalog_found=" + std::string(result.catalogFound ? "1" : "0") +
      " catalog_loaded=1 catalog_path=" +
      (result.catalogPath.empty() ? std::string("wx-catalog-lookup")
                                  : result.catalogPath));
  return result;
}

// Returns the language currently active in this process.
AppLanguage LocalizationManager::ActiveLanguage() const { return activeLanguage_; }

} // namespace localization
