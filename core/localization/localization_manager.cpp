#include "localization/localization_manager.h"

#include "diagnostics/DiagnosticLogger.h"

#include <clocale>
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

// Returns the wxWidgets language id for a supported application language.
int WxLanguageId(AppLanguage language) {
  switch (language) {
  case AppLanguage::Spanish:
    return wxLANGUAGE_SPANISH;
  case AppLanguage::English:
  default:
    return wxLANGUAGE_ENGLISH;
  }
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
  AddUniquePath(roots, executableDir / "resources" / "resources" / "locale");
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
    diagnostics::DiagnosticLogger::Info("Localization initialized: language=en catalog=builtin");
    return result;
  }

  auto locale = std::make_unique<wxLocale>();
  if (!locale->Init(WxLanguageId(language), wxLOCALE_DONT_LOAD_DEFAULT)) {
    result.diagnostic = "wxLocale initialization failed; using English";
    diagnostics::DiagnosticLogger::Warning("Localization warning: language=" +
                                          std::string(AppLanguageCode(language)) +
                                          " init_failed=1");
    return result;
  }

  // UI language setup may adjust process locale categories; keep numeric serialization stable.
  std::setlocale(LC_NUMERIC, "C");

  for (const auto &root : ResolveLocaleRootCandidates()) {
    const auto rootUtf8 = root.u8string();
    const std::string rootText(rootUtf8.begin(), rootUtf8.end());
    locale->AddCatalogLookupPathPrefix(wxString::FromUTF8(rootText));
  }

  if (!locale->AddCatalog(kCatalogName)) {
    result.diagnostic = "translation catalog was not found or could not be loaded; using English";
    diagnostics::DiagnosticLogger::Warning("Localization warning: language=" +
                                          std::string(AppLanguageCode(language)) +
                                          " catalog_loaded=0 fallback=en");
    return result;
  }

  activeLanguage_ = language;
  locale_ = std::move(locale);
  result.activeLanguage = language;
  result.catalogLoaded = true;
  result.diagnostic = "translation catalog loaded";
  diagnostics::DiagnosticLogger::Info("Localization initialized: language=" +
                                      std::string(AppLanguageCode(language)) +
                                      " catalog_loaded=1");
  return result;
}

// Returns the language currently active in this process.
AppLanguage LocalizationManager::ActiveLanguage() const { return activeLanguage_; }

} // namespace localization
