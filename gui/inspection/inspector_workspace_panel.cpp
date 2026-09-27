#include "inspection/inspector_workspace_panel.h"

#include "diagnostics/DiagnosticLogger.h"
#include "guiconfigservices.h"
#include "inspection/inspection_report_aggregation.h"
#include "inspection/inspector_presentation.h"
#include "inspection/resource_inspection.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <string>

#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace gui::inspection {
namespace {

constexpr char kPageKey[] = "inspector_workspace_details_page";

// Converts UTF-8 technical content to a wxWidgets string without localization.
wxString FromUtf8(const std::string &value) {
  return wxString::FromUTF8(value.data(), value.size());
}

// Converts a native path to a display string without changing its spelling.
wxString PathText(const std::filesystem::path &path) {
  return WxPathUtils::WxStringFromFilesystemPath(path);
}

// Returns the stable technical spelling of a diagnostic severity.
const char *SeverityName(perastage::inspection::DiagnosticSeverity severity) {
  using perastage::inspection::DiagnosticSeverity;
  switch (severity) {
  case DiagnosticSeverity::Information:
    return "information";
  case DiagnosticSeverity::Warning:
    return "warning";
  case DiagnosticSeverity::Error:
    return "error";
  case DiagnosticSeverity::Fatal:
    return "fatal";
  }
  return "unknown";
}

// Returns the stable technical spelling of a diagnostic domain.
const char *DomainName(perastage::inspection::DiagnosticDomain domain) {
  using perastage::inspection::DiagnosticDomain;
  switch (domain) {
  case DiagnosticDomain::Input:
    return "input";
  case DiagnosticDomain::Package:
    return "package";
  case DiagnosticDomain::Xml:
    return "xml";
  case DiagnosticDomain::Content:
    return "content";
  }
  return "unknown";
}

// Returns the stable technical spelling of a diagnostic classification.
const char *ClassificationName(
    perastage::inspection::DiagnosticClassification classification) {
  using perastage::inspection::DiagnosticClassification;
  switch (classification) {
  case DiagnosticClassification::General:
    return "general";
  case DiagnosticClassification::Standards:
    return "standards";
  case DiagnosticClassification::Compatibility:
    return "compatibility";
  }
  return "unknown";
}

// Localizes the existing Core GDTF read state at the GUI boundary.
wxString LocalizedGdtfReadStatus(perastage::inspection::GdtfReadStatus status) {
  using perastage::inspection::GdtfReadStatus;
  switch (status) {
  case GdtfReadStatus::Canonical:
    return _("Canonical");
  case GdtfReadStatus::CompatibilityAccepted:
    return _("Compatibility accepted");
  case GdtfReadStatus::Unusable:
    return _("Unusable");
  }
  return _("Unusable");
}

// Returns the stable technical spelling of a validation layer.
const char *ValidationLayerName(perastage::inspection::ValidationLayer layer) {
  using perastage::inspection::ValidationLayer;
  switch (layer) {
  case ValidationLayer::XmlWellFormedness:
    return "xml-well-formedness";
  case ValidationLayer::Schema:
    return "schema";
  case ValidationLayer::SemanticInteroperability:
    return "semantic-interoperability";
  }
  return "unknown";
}

// Returns the stable technical spelling of a validation status.
const char *
ValidationStatusName(perastage::inspection::ValidationStatus status) {
  using perastage::inspection::ValidationStatus;
  switch (status) {
  case ValidationStatus::NotRun:
    return "not-run";
  case ValidationStatus::Unavailable:
    return "unavailable";
  case ValidationStatus::Valid:
    return "valid";
  case ValidationStatus::Invalid:
    return "invalid";
  }
  return "unknown";
}

// Copies one value through the native clipboard when it can be opened.
bool CopyText(const wxString &value) {
  if (!wxTheClipboard->Open())
    return false;
  wxTheClipboard->SetData(new wxTextDataObject(value));
  wxTheClipboard->Close();
  return true;
}

// Reads a bounded integer preference or returns its default value.
int ReadInt(const IGuiPreferencesService &preferences, const char *key,
            int fallback, int minimum, int maximum) {
  const auto stored = preferences.GetValue(key);
  if (!stored)
    return fallback;
  int value = fallback;
  const auto parsed =
      std::from_chars(stored->data(), stored->data() + stored->size(), value);
  return parsed.ec == std::errc{} ? std::clamp(value, minimum, maximum)
                                  : fallback;
}

// Appends validation diagnostics to the primary diagnostic sequence.
std::vector<perastage::inspection::Diagnostic> AllDiagnostics(
    const perastage::inspection::Result &inspection,
    const std::vector<perastage::inspection::ValidationResult> &validation) {
  return perastage::inspection::CollectUniqueDiagnostics(inspection,
                                                         validation);
}

// Appends common file, diagnostic-total, and validation-layer facts.
void AppendCommonSummary(
    wxString &text, const perastage::inspection::Result &inspection,
    const std::vector<perastage::inspection::ValidationResult> &validation) {
  text << _("File:") << ' ' << PathText(inspection.request.sourcePath) << '\n';
  std::array<std::size_t, 4> totals{};
  for (const auto &diagnostic : AllDiagnostics(inspection, validation))
    ++totals[static_cast<std::size_t>(diagnostic.severity)];
  text << _("Diagnostics:") << " information=" << totals[0]
       << ", warning=" << totals[1] << ", error=" << totals[2]
       << ", fatal=" << totals[3] << '\n';
  if (!validation.empty()) {
    text << _("Validation layers:");
    for (const auto &result : validation)
      text << ' ' << ValidationLayerName(result.layer) << '='
           << ValidationStatusName(result.status);
    text << '\n';
  }
}

} // namespace

// Constructs the Inspector workspace and restores presentation-only state.
InspectorWorkspacePanel::InspectorWorkspacePanel(
    wxWindow *parent, IGuiPreferencesService &preferences)
    : wxPanel(parent, wxID_ANY), preferences_(preferences) {
  BuildLayout();
  RestoreLayout();
}

// Persists useful workspace state without retaining inspected content.
InspectorWorkspacePanel::~InspectorWorkspacePanel() { SaveLayout(); }

// Builds the package-navigation, XML-content, and details workspace columns.
void InspectorWorkspacePanel::BuildLayout() {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *header = new wxBoxSizer(wxHORIZONTAL);
  auto *open = new wxButton(this, wxID_OPEN, _("Open..."));
  identity_ = new wxStaticText(this, wxID_ANY, _("No file selected"));
  header->Add(open, 0, wxRIGHT, 8);
  header->Add(identity_, 1, wxALIGN_CENTER_VERTICAL);
  root->Add(header, 0, wxEXPAND | wxALL, 8);

  auto *workspace = new wxBoxSizer(wxHORIZONTAL);
  auto *packageColumn = new wxBoxSizer(wxVERTICAL);
  packageColumn->Add(new wxStaticText(this, wxID_ANY, _("Package")), 0,
                     wxBOTTOM, 4);
  package_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL);
  package_->AppendColumn(_("Path"), wxLIST_FORMAT_LEFT, 220);
  package_->AppendColumn(_("Type"), wxLIST_FORMAT_LEFT, 80);
  package_->AppendColumn(_("Resource kind"), wxLIST_FORMAT_LEFT, 100);
  package_->AppendColumn(_("Size"), wxLIST_FORMAT_RIGHT, 80);
  package_->AppendColumn(_("Availability"), wxLIST_FORMAT_LEFT, 90);
  auto *copyPath = new wxButton(this, wxID_ANY, _("Copy path"));
  packageColumn->Add(package_, 1, wxEXPAND | wxBOTTOM, 6);
  packageColumn->Add(copyPath, 0, wxALIGN_RIGHT);
  workspace->Add(packageColumn, 1, wxEXPAND | wxRIGHT, 8);

  auto *xmlColumn = new wxBoxSizer(wxVERTICAL);
  auto *findRow = new wxBoxSizer(wxHORIZONTAL);
  search_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
  auto *previous = new wxButton(this, wxID_ANY, _("Find previous"));
  auto *next = new wxButton(this, wxID_ANY, _("Find next"));
  auto *copyAll = new wxButton(this, wxID_ANY, _("Copy all"));
  findRow->Add(new wxStaticText(this, wxID_ANY, _("Find:")), 0,
               wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  findRow->Add(search_, 1, wxRIGHT, 5);
  findRow->Add(previous, 0, wxRIGHT, 5);
  findRow->Add(next, 0, wxRIGHT, 5);
  findRow->Add(copyAll, 0);
  xml_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
  xmlColumn->Add(findRow, 0, wxEXPAND | wxBOTTOM, 6);
  xmlColumn->Add(xml_, 1, wxEXPAND);
  workspace->Add(xmlColumn, 2, wxEXPAND | wxRIGHT, 8);

  notebook_ = new wxNotebook(this, wxID_ANY);
  summary_ = new wxTextCtrl(notebook_, wxID_ANY, {}, wxDefaultPosition,
                            wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  notebook_->AddPage(summary_, _("Summary"));
  auto *diagnosticPage = new wxPanel(notebook_);
  auto *diagnosticSizer = new wxBoxSizer(wxVERTICAL);
  diagnostics_ = new wxListCtrl(diagnosticPage, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
  diagnostics_->AppendColumn(_("Severity"), wxLIST_FORMAT_LEFT, 80);
  diagnostics_->AppendColumn(_("Domain"), wxLIST_FORMAT_LEFT, 80);
  diagnostics_->AppendColumn(_("Classification"), wxLIST_FORMAT_LEFT, 100);
  diagnostics_->AppendColumn(_("Code"), wxLIST_FORMAT_LEFT, 160);
  diagnostics_->AppendColumn(_("Message"), wxLIST_FORMAT_LEFT, 300);
  diagnostics_->AppendColumn(_("Location"), wxLIST_FORMAT_LEFT, 140);
  auto *copyDiagnostic =
      new wxButton(diagnosticPage, wxID_ANY, _("Copy diagnostic"));
  diagnosticSizer->Add(diagnostics_, 1, wxEXPAND | wxBOTTOM, 6);
  diagnosticSizer->Add(copyDiagnostic, 0, wxALIGN_RIGHT);
  diagnosticPage->SetSizer(diagnosticSizer);
  notebook_->AddPage(diagnosticPage, _("Diagnostics"));
  workspace->Add(notebook_, 1, wxEXPAND);
  root->Add(workspace, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  SetSizer(root);

  open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ChooseFile(); });
  copyPath->Bind(wxEVT_BUTTON,
                 [this](wxCommandEvent &) { CopySelectedPath(); });
  copyDiagnostic->Bind(wxEVT_BUTTON,
                       [this](wxCommandEvent &) { CopySelectedDiagnostic(); });
  previous->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FindXml(false); });
  next->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FindXml(true); });
  search_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { FindXml(true); });
  copyAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { CopyAllXml(); });
}

// Restores the selected details page through GUI preferences.
void InspectorWorkspacePanel::RestoreLayout() {
  notebook_->SetSelection(ReadInt(preferences_, kPageKey, 0, 0, 1));
}

// Saves only the selected details page for the next workspace activation.
void InspectorWorkspacePanel::SaveLayout() const {
  preferences_.SetValue(kPageKey, std::to_string(notebook_->GetSelection()));
  preferences_.SaveUserConfig();
}

// Opens the native supported-file chooser and inspects its selection.
void InspectorWorkspacePanel::ChooseFile() {
  wxFileDialog chooser(this, _("Open MVR or GDTF file"), {}, {},
                       _("MVR and GDTF files (*.mvr;*.gdtf)|*.mvr;*.gdtf|MVR "
                         "files (*.mvr)|*.mvr|GDTF files (*.gdtf)|*.gdtf"),
                       wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (chooser.ShowModal() == wxID_OK)
    OpenFile(WxPathUtils::FilesystemPathFromWxString(chooser.GetPath()));
}

// Dispatches a supported file to the established read-only Inspection Core.
void InspectorWorkspacePanel::OpenFile(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  if (extension != ".mvr" && extension != ".gdtf") {
    wxMessageBox(_("Only MVR and GDTF files can be inspected."),
                 _("MVR / GDTF Inspector"), wxOK | wxICON_ERROR, this);
    return;
  }
  try {
    ClearResult();
    identity_->SetLabel(PathText(path));
    if (extension == ".gdtf")
      ShowGdtf(perastage::inspection::InspectGdtf(path));
    else
      ShowMvr(perastage::inspection::InspectMvr(path));
  } catch (const std::exception &error) {
    diagnostics::DiagnosticLogger::Error(
        std::string("Inspector failed unexpectedly: ") + error.what());
    wxMessageBox(
        _("The file could not be inspected because of an unexpected error."),
        _("MVR / GDTF Inspector"), wxOK | wxICON_ERROR, this);
  }
}

// Clears all presentation controls before showing a new immutable result.
void InspectorWorkspacePanel::ClearResult() {
  summary_->Clear();
  package_->DeleteAllItems();
  diagnostics_->DeleteAllItems();
  diagnosticRows_.clear();
  xml_->Clear();
  search_->Clear();
}

// Projects high-level GDTF facts without reparsing the retained XML.
void InspectorWorkspacePanel::ShowGdtf(
    const perastage::inspection::GdtfInspectionResult &result) {
  wxString text;
  text << _("Format:") << " GDTF\n"
       << _("Status:") << ' ' << LocalizedGdtfReadStatus(result.status) << '\n';
  AppendCommonSummary(text, result.inspection, result.validation);
  std::vector<perastage::inspection::ResourceDescriptor> resources;
  if (result.packageInventory)
    resources = perastage::inspection::DescribePackageResources(
        result.inspection.request.sourcePath, *result.packageInventory);
  if (result.packageInventory)
    text << _("Package entries:") << ' '
         << result.packageInventory->entries.size() << "\n"
         << _("Resources:") << ' ' << resources.size() << '\n';
  if (result.document) {
    const auto &description = result.document->Description();
    text << "DataVersion: " << FromUtf8(description.dataVersion) << "\n"
         << _("Fixture type:") << ' ' << FromUtf8(description.fixtureTypeName)
         << "\n"
         << _("Manufacturer:") << ' ' << FromUtf8(description.manufacturer)
         << "\n"
         << _("DMX modes:") << ' ' << description.dmxModeNames.size() << '\n';
    SetXml(result.document->Archive().descriptionXml);
  }
  summary_->SetValue(text);
  PopulatePackage(resources);
  PopulateDiagnostics(result.inspection, result.validation);
}

// Projects high-level MVR facts without traversing or changing application
// data.
void InspectorWorkspacePanel::ShowMvr(
    const perastage::inspection::MvrInspectionResult &result) {
  wxString text;
  text << _("Format:") << " MVR\n"
       << _("Status:") << ' '
       << (result.Success() ? _("Readable") : _("Unusable")) << '\n';
  AppendCommonSummary(text, result.inspection, result.validation);
  std::vector<perastage::inspection::ResourceDescriptor> resources;
  if (result.packageInventory)
    resources = perastage::inspection::DescribePackageResources(
        result.inspection.request.sourcePath, *result.packageInventory);
  if (result.packageInventory)
    text << _("Package entries:") << ' '
         << result.packageInventory->entries.size() << "\n"
         << _("Resources:") << ' ' << resources.size() << '\n';
  if (result.snapshot) {
    const auto &snapshot = *result.snapshot;
    text << _("MVR version:") << ' ' << snapshot.versionMajor << '.'
         << snapshot.versionMinor << "\n"
         << _("Provider:") << ' ' << FromUtf8(snapshot.provider) << "\n"
         << _("Provider version:") << ' ' << FromUtf8(snapshot.providerVersion)
         << '\n';
    for (const auto &count : snapshot.nodeCounts)
      text << FromUtf8(count.type) << ": " << count.count << '\n';
    SetXml(snapshot.sceneDescriptionXml);
  }
  summary_->SetValue(text);
  PopulatePackage(resources);
  PopulateDiagnostics(result.inspection, result.validation);
}

// Populates generic package rows solely from inventory and resource
// descriptors.
void InspectorWorkspacePanel::PopulatePackage(
    const std::vector<perastage::inspection::ResourceDescriptor> &resources) {
  for (std::size_t index = 0; index < resources.size(); ++index) {
    const auto &resource = resources[index];
    const long row = package_->InsertItem(package_->GetItemCount(),
                                          FromUtf8(resource.displayPath));
    package_->SetItem(row, 1,
                      resource.entryType ==
                              perastage::inspection::PackageEntryType::Directory
                          ? "directory"
                          : "file");
    package_->SetItem(row, 2, ResourceKindLabel(resource));
    package_->SetItem(row, 3,
                      resource.sizeKnown ? std::to_string(resource.size) : "");
    package_->SetItem(row, 4,
                      resource.pathSafe ? _("Available") : _("Unsafe path"));
    package_->SetItemData(row, static_cast<long>(index));
  }
}

// Populates structured diagnostic columns without parsing technical messages.
void InspectorWorkspacePanel::PopulateDiagnostics(
    const perastage::inspection::Result &inspection,
    const std::vector<perastage::inspection::ValidationResult> &validation) {
  diagnosticRows_ = AllDiagnostics(inspection, validation);
  std::stable_sort(diagnosticRows_.begin(), diagnosticRows_.end(),
                   [](const auto &left, const auto &right) {
                     if (left.severity != right.severity)
                       return left.severity > right.severity;
                     return left.domain < right.domain;
                   });
  for (std::size_t index = 0; index < diagnosticRows_.size(); ++index) {
    const auto &diagnostic = diagnosticRows_[index];
    const long row = diagnostics_->InsertItem(
        diagnostics_->GetItemCount(), SeverityName(diagnostic.severity));
    diagnostics_->SetItem(row, 1, DomainName(diagnostic.domain));
    diagnostics_->SetItem(row, 2,
                          ClassificationName(diagnostic.classification));
    diagnostics_->SetItem(row, 3, FromUtf8(diagnostic.code));
    diagnostics_->SetItem(row, 4, FromUtf8(diagnostic.message));
    diagnostics_->SetItem(row, 5, FromUtf8(DiagnosticLocationText(diagnostic)));
    diagnostics_->SetItemData(row, static_cast<long>(index));
  }
}

// Displays retained root XML exactly as supplied by Inspection Core.
void InspectorWorkspacePanel::SetXml(const std::string &xml) {
  xml_->SetValue(FromUtf8(xml));
}

// Finds literal text in the retained XML with deterministic wrap-around.
void InspectorWorkspacePanel::FindXml(bool forward) {
  const wxString needle = search_->GetValue();
  if (needle.empty())
    return;
  const wxString haystack = xml_->GetValue();
  long start = 0, end = 0;
  xml_->GetSelection(&start, &end);
  const auto reverseStart =
      start > 0 ? static_cast<std::size_t>(start - 1) : wxString::npos;
  long found = forward ? haystack.find(needle, end)
                       : haystack.rfind(needle, reverseStart);
  if (found == wxNOT_FOUND)
    found = forward ? haystack.find(needle) : haystack.rfind(needle);
  if (found != wxNOT_FOUND) {
    xml_->SetSelection(found, found + static_cast<long>(needle.length()));
    xml_->ShowPosition(found);
    xml_->SetFocus();
  }
}

// Copies the exact selected archive path.
void InspectorWorkspacePanel::CopySelectedPath() {
  const long row =
      package_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (row >= 0 && !CopyText(package_->GetItemText(row)))
    wxMessageBox(_("The clipboard could not be opened."),
                 _("MVR / GDTF Inspector"), wxOK | wxICON_WARNING, this);
}

// Copies a deterministic representation of the selected diagnostic.
void InspectorWorkspacePanel::CopySelectedDiagnostic() {
  const long row =
      diagnostics_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (row < 0)
    return;
  const auto &diagnostic = diagnosticRows_.at(diagnostics_->GetItemData(row));
  std::string value = std::string(SeverityName(diagnostic.severity)) + " [" +
                      diagnostic.code + "] " + diagnostic.message;
  const auto location = DiagnosticLocationText(diagnostic);
  if (!location.empty())
    value += " (" + location + ')';
  if (!CopyText(FromUtf8(value)))
    wxMessageBox(_("The clipboard could not be opened."),
                 _("MVR / GDTF Inspector"), wxOK | wxICON_WARNING, this);
}

// Copies the complete unchanged retained XML buffer.
void InspectorWorkspacePanel::CopyAllXml() {
  if (!CopyText(xml_->GetValue()))
    wxMessageBox(_("The clipboard could not be opened."),
                 _("MVR / GDTF Inspector"), wxOK | wxICON_WARNING, this);
}

} // namespace gui::inspection
