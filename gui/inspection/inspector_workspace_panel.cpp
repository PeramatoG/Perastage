#include "inspection/inspector_workspace_panel.h"

#include "diagnostics/DiagnosticLogger.h"
#include "guiconfigservices.h"
#include "inspection/inspection_report_aggregation.h"
#include "inspection/inspector_presentation.h"
#include "inspector_project_source.h"
#include "inspection/nested_gdtf_inspection.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <memory>
#include <string>
#include <tuple>

#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dataview.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/settings.h>
#include <wx/textctrl.h>

namespace gui::inspection {
namespace {

constexpr char kPageKey[] = "inspector_workspace_details_page";
constexpr std::uint64_t kNestedGdtfLimit = 512U * 1024U * 1024U;

wxString FromUtf8(const std::string &value);

// Adapts the neutral package hierarchy to a native tree with metadata columns.
class PackageDataViewModel final : public wxDataViewModel {
public:
  explicit PackageDataViewModel(std::vector<PackageTreeNode> roots) {
    Build(nullptr, std::move(roots), roots_);
  }

  // Returns the number of visible package metadata columns.
  unsigned int GetColumnCount() const override { return 5; }

  // Returns the string variant type used by every package column.
  wxString GetColumnType(unsigned int) const override { return "string"; }

  // Maps one neutral package node to its requested presentation value.
  void GetValue(wxVariant &variant, const wxDataViewItem &item,
                unsigned int column) const override {
    const auto &node = Node(item).value;
    switch (column) {
    case 0: variant = FromUtf8(node.name); break;
    case 1:
      variant = node.entryType == perastage::inspection::PackageEntryType::Directory
                    ? _("directory") : _("file");
      break;
    case 2: variant = FromUtf8(ResourceKindLabel(Descriptor(node)));
      break;
    case 3: variant = node.sizeKnown ? wxString::Format("%llu", static_cast<unsigned long long>(node.size)) : wxString{}; break;
    case 4: variant = node.pathSafe ? _("Available") : _("Unsafe path"); break;
    }
  }

  // Rejects edits because the Inspector is read-only.
  bool SetValue(const wxVariant &, const wxDataViewItem &,
                unsigned int) override { return false; }

  // Returns the authored parent node or the invisible root.
  wxDataViewItem GetParent(const wxDataViewItem &item) const override {
    return item.IsOk() && Node(item).parent
               ? wxDataViewItem(Node(item).parent)
               : wxDataViewItem{};
  }

  // Reports whether a package node can own hierarchical children.
  bool IsContainer(const wxDataViewItem &item) const override {
    return !item.IsOk() ||
           Node(item).value.entryType ==
               perastage::inspection::PackageEntryType::Directory;
  }

  // Supplies deterministic root or child items to wxDataViewCtrl.
  unsigned int GetChildren(const wxDataViewItem &parent,
                           wxDataViewItemArray &children) const override {
    const auto &source = parent.IsOk() ? Node(parent).children : roots_;
    for (const auto &child : source)
      children.Add(wxDataViewItem(child.get()));
    return static_cast<unsigned int>(source.size());
  }

  // Returns the neutral node associated with one native item.
  const PackageTreeNode *Value(const wxDataViewItem &item) const {
    return item.IsOk() ? &Node(item).value : nullptr;
  }

private:
  struct ViewNode {
    PackageTreeNode value;
    ViewNode *parent = nullptr;
    std::vector<std::unique_ptr<ViewNode>> children;
  };

  // Recursively takes ownership of a neutral subtree.
  static void Build(ViewNode *parent, std::vector<PackageTreeNode> values,
                    std::vector<std::unique_ptr<ViewNode>> &destination) {
    for (auto &value : values) {
      auto node = std::make_unique<ViewNode>();
      auto children = std::move(value.children);
      node->value = std::move(value);
      node->parent = parent;
      Build(node.get(), std::move(children), node->children);
      destination.push_back(std::move(node));
    }
  }

  // Reconstructs the minimal descriptor needed by the existing label mapper.
  static perastage::inspection::ResourceDescriptor
  Descriptor(const PackageTreeNode &node) {
    perastage::inspection::ResourceDescriptor descriptor;
    descriptor.entryType = node.entryType;
    descriptor.kind = node.resourceKind;
    return descriptor;
  }

  // Resolves one valid native item to its owned node.
  static const ViewNode &Node(const wxDataViewItem &item) {
    return *static_cast<const ViewNode *>(item.GetID());
  }

  std::vector<std::unique_ptr<ViewNode>> roots_;
};

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
  case DiagnosticSeverity::Information: return "information";
  case DiagnosticSeverity::Warning: return "warning";
  case DiagnosticSeverity::Error: return "error";
  case DiagnosticSeverity::Fatal: return "fatal";
  }
  return "unknown";
}

// Returns the stable technical spelling of a diagnostic domain.
const char *DomainName(perastage::inspection::DiagnosticDomain domain) {
  using perastage::inspection::DiagnosticDomain;
  switch (domain) {
  case DiagnosticDomain::Input: return "input";
  case DiagnosticDomain::Package: return "package";
  case DiagnosticDomain::Xml: return "xml";
  case DiagnosticDomain::Content: return "content";
  }
  return "unknown";
}

// Returns the stable technical spelling of a diagnostic classification.
const char *ClassificationName(
    perastage::inspection::DiagnosticClassification classification) {
  using perastage::inspection::DiagnosticClassification;
  switch (classification) {
  case DiagnosticClassification::General: return "general";
  case DiagnosticClassification::Standards: return "standards";
  case DiagnosticClassification::Compatibility: return "compatibility";
  }
  return "unknown";
}

// Returns the localized Core GDTF read state.
wxString LocalizedGdtfReadStatus(perastage::inspection::GdtfReadStatus status) {
  using perastage::inspection::GdtfReadStatus;
  switch (status) {
  case GdtfReadStatus::Canonical: return _("Canonical");
  case GdtfReadStatus::CompatibilityAccepted: return _("Compatibility accepted");
  case GdtfReadStatus::Unusable: return _("Unusable");
  }
  return _("Unusable");
}

// Returns the stable technical spelling of a validation layer.
const char *ValidationLayerName(perastage::inspection::ValidationLayer layer) {
  using perastage::inspection::ValidationLayer;
  switch (layer) {
  case ValidationLayer::XmlWellFormedness: return "xml-well-formedness";
  case ValidationLayer::Schema: return "schema";
  case ValidationLayer::SemanticInteroperability: return "semantic-interoperability";
  }
  return "unknown";
}

// Returns the stable technical spelling of a validation status.
const char *ValidationStatusName(perastage::inspection::ValidationStatus status) {
  using perastage::inspection::ValidationStatus;
  switch (status) {
  case ValidationStatus::NotRun: return "not-run";
  case ValidationStatus::Unavailable: return "unavailable";
  case ValidationStatus::Valid: return "valid";
  case ValidationStatus::Invalid: return "invalid";
  }
  return "unknown";
}

// Copies one value through the native clipboard when it can be opened.
bool CopyText(const wxString &value) {
  if (!wxTheClipboard->Open()) return false;
  wxTheClipboard->SetData(new wxTextDataObject(value));
  wxTheClipboard->Close();
  return true;
}

// Reads a bounded integer preference or returns its default value.
int ReadInt(const IGuiPreferencesService &preferences, const char *key,
            int fallback, int minimum, int maximum) {
  const auto stored = preferences.GetValue(key);
  if (!stored) return fallback;
  int value = fallback;
  const auto parsed = std::from_chars(stored->data(), stored->data() + stored->size(), value);
  return parsed.ec == std::errc{} ? std::clamp(value, minimum, maximum) : fallback;
}

// Collects primary and validation diagnostics once.
std::vector<perastage::inspection::Diagnostic> AllDiagnostics(
    const perastage::inspection::Result &inspection,
    const std::vector<perastage::inspection::ValidationResult> &validation) {
  return perastage::inspection::CollectUniqueDiagnostics(inspection, validation);
}

// Appends common structured inspection facts.
void AppendCommonSummary(
    wxString &text, const perastage::inspection::Result &inspection,
    const std::vector<perastage::inspection::ValidationResult> &validation) {
  if (!inspection.request.sourcePath.empty())
    text << _("File:") << ' ' << PathText(inspection.request.sourcePath) << '\n';
  const auto diagnostics = AllDiagnostics(inspection, validation);
  text << _("Diagnostics:") << ' ' << diagnostics.size() << '\n';
  if (!validation.empty()) {
    text << _("Validation layers:");
    for (const auto &result : validation)
      text << ' ' << ValidationLayerName(result.layer) << '=' << ValidationStatusName(result.status);
    text << '\n';
  }
}

// Adds scene nodes recursively from the neutral presentation tree.
void AppendSceneNodes(wxDataViewTreeCtrl &tree, const wxDataViewItem &parent,
                      const std::vector<SceneTreeNode> &nodes) {
  for (const auto &node : nodes) {
    wxString label = FromUtf8(node.name.empty() ? node.kind : node.name);
    if (!node.uuid.empty()) label << "  [" << FromUtf8(node.uuid) << ']';
    if (node.unresolved) label << ' ' << _("(unresolved parent)");
    const auto item = node.children.empty() ? tree.AppendItem(parent, label)
                                           : tree.AppendContainer(parent, label);
    AppendSceneNodes(tree, item, node.children);
  }
}

// Configures theme-aware XML syntax presentation and folding.
void ConfigureXmlEditor(wxStyledTextCtrl &editor) {
  const wxColour foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
  const wxColour background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  editor.StyleSetForeground(wxSTC_STYLE_DEFAULT, foreground);
  editor.StyleSetBackground(wxSTC_STYLE_DEFAULT, background);
  editor.StyleClearAll();
  editor.SetLexer(wxSTC_LEX_XML);
  editor.StyleSetForeground(wxSTC_H_TAG, wxColour(0, 96, 160));
  editor.StyleSetForeground(wxSTC_H_ATTRIBUTE, wxColour(128, 64, 0));
  editor.StyleSetForeground(wxSTC_H_DOUBLESTRING, wxColour(0, 112, 48));
  editor.StyleSetForeground(wxSTC_H_SINGLESTRING, wxColour(0, 112, 48));
  editor.StyleSetForeground(wxSTC_H_COMMENT, wxColour(96, 96, 96));
  editor.SetProperty("fold", "1");
  editor.SetMarginType(1, wxSTC_MARGIN_SYMBOL);
  editor.SetMarginMask(1, wxSTC_MASK_FOLDERS);
  editor.SetMarginWidth(1, 16);
  editor.SetMarginSensitive(1, true);
  editor.SetReadOnly(true);
}

} // namespace

// Constructs the Inspector workspace and restores presentation-only state.
InspectorWorkspacePanel::InspectorWorkspacePanel(
    wxWindow *parent, IGuiPreferencesService &preferences,
    const IGuiProjectSessionService &project)
    : wxPanel(parent, wxID_ANY), preferences_(preferences), project_(project) {
  BuildLayout();
  RestoreLayout();
}

// Persists useful workspace state without retaining inspected content.
InspectorWorkspacePanel::~InspectorWorkspacePanel() { SaveLayout(); }

// Lazily inspects the active project the first time the workspace is opened.
void InspectorWorkspacePanel::Activate() {
  if (sourceKind_ == SourceKind::None) InspectCurrentProject();
}

// Builds source actions, navigation trees, XML, and diagnostic details.
void InspectorWorkspacePanel::BuildLayout() {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *header = new wxBoxSizer(wxHORIZONTAL);
  auto *refresh = new wxButton(this, wxID_REFRESH, _("Refresh"));
  auto *open = new wxButton(this, wxID_OPEN, _("Open external..."));
  back_ = new wxButton(this, wxID_BACKWARD, _("Back to parent MVR"));
  identity_ = new wxStaticText(this, wxID_ANY, _("Source: Current project MVR"));
  sourceType_ = new wxStaticText(this, wxID_ANY, _("Active project"));
  auto *identityBox = new wxBoxSizer(wxVERTICAL);
  identityBox->Add(identity_); identityBox->Add(sourceType_);
  header->Add(refresh, 0, wxRIGHT, 6); header->Add(open, 0, wxRIGHT, 6);
  header->Add(back_, 0, wxRIGHT, 8); header->Add(identityBox, 1, wxALIGN_CENTER_VERTICAL);
  back_->Hide(); root->Add(header, 0, wxEXPAND | wxALL, 8);

  auto *workspace = new wxBoxSizer(wxHORIZONTAL);
  navigation_ = new wxNotebook(this, wxID_ANY);
  package_ = new wxDataViewCtrl(navigation_, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxDV_SINGLE);
  package_->AppendTextColumn(_("Path"), 0, wxDATAVIEW_CELL_INERT, 220);
  package_->AppendTextColumn(_("Type"), 1, wxDATAVIEW_CELL_INERT, 80);
  package_->AppendTextColumn(_("Resource kind"), 2, wxDATAVIEW_CELL_INERT, 100);
  package_->AppendTextColumn(_("Size"), 3, wxDATAVIEW_CELL_INERT, 80, wxALIGN_RIGHT);
  package_->AppendTextColumn(_("Availability"), 4, wxDATAVIEW_CELL_INERT, 90);
  scene_ = new wxDataViewTreeCtrl(navigation_, wxID_ANY, wxDefaultPosition,
                                  wxDefaultSize, wxDV_SINGLE);
  navigation_->AddPage(package_, _("Package"));
  navigation_->AddPage(scene_, _("Scene"));
  workspace->Add(navigation_, 1, wxEXPAND | wxRIGHT, 8);

  auto *xmlColumn = new wxBoxSizer(wxVERTICAL);
  auto *findRow = new wxBoxSizer(wxHORIZONTAL);
  search_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
  auto *previous = new wxButton(this, wxID_ANY, _("Find previous"));
  auto *next = new wxButton(this, wxID_ANY, _("Find next"));
  auto *copyAll = new wxButton(this, wxID_ANY, _("Copy all"));
  findRow->Add(new wxStaticText(this, wxID_ANY, _("Find:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  findRow->Add(search_, 1, wxRIGHT, 5); findRow->Add(previous, 0, wxRIGHT, 5);
  findRow->Add(next, 0, wxRIGHT, 5); findRow->Add(copyAll);
  xml_ = new wxStyledTextCtrl(this, wxID_ANY);
  ConfigureXmlEditor(*xml_);
  xmlColumn->Add(findRow, 0, wxEXPAND | wxBOTTOM, 6); xmlColumn->Add(xml_, 1, wxEXPAND);
  workspace->Add(xmlColumn, 2, wxEXPAND | wxRIGHT, 8);

  notebook_ = new wxNotebook(this, wxID_ANY);
  summary_ = new wxTextCtrl(notebook_, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  issues_ = new wxTextCtrl(notebook_, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  notebook_->AddPage(summary_, _("Summary")); notebook_->AddPage(issues_, _("Issues"));
  auto *diagnosticPage = new wxPanel(notebook_);
  auto *diagnosticSizer = new wxBoxSizer(wxVERTICAL);
  diagnostics_ = new wxListCtrl(diagnosticPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
  diagnostics_->AppendColumn(_("Severity"), wxLIST_FORMAT_LEFT, 80);
  diagnostics_->AppendColumn(_("Domain"), wxLIST_FORMAT_LEFT, 80);
  diagnostics_->AppendColumn(_("Classification"), wxLIST_FORMAT_LEFT, 100);
  diagnostics_->AppendColumn(_("Code"), wxLIST_FORMAT_LEFT, 160);
  diagnostics_->AppendColumn(_("Message"), wxLIST_FORMAT_LEFT, 300);
  diagnostics_->AppendColumn(_("Location"), wxLIST_FORMAT_LEFT, 140);
  auto *copyDiagnostic = new wxButton(diagnosticPage, wxID_ANY, _("Copy diagnostic"));
  diagnosticSizer->Add(diagnostics_, 1, wxEXPAND | wxBOTTOM, 6); diagnosticSizer->Add(copyDiagnostic, 0, wxALIGN_RIGHT);
  diagnosticPage->SetSizer(diagnosticSizer); notebook_->AddPage(diagnosticPage, _("Diagnostics"));
  workspace->Add(notebook_, 1, wxEXPAND);
  root->Add(workspace, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8); SetSizer(root);

  refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { RefreshSource(); });
  open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ChooseFile(); });
  back_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ReturnToParentMvr(); });
  package_->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &InspectorWorkspacePanel::ShowPackageContextMenu, this);
  package_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &InspectorWorkspacePanel::ActivatePackageEntry, this);
  copyDiagnostic->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { CopySelectedDiagnostic(); });
  previous->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FindXml(false); });
  next->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FindXml(true); });
  search_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { FindXml(true); });
  copyAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { CopyAllXml(); });
}

// Restores the selected details page through GUI preferences.
void InspectorWorkspacePanel::RestoreLayout() {
  notebook_->SetSelection(ReadInt(preferences_, kPageKey, 0, 0, 2));
}

// Saves only the selected details page for the next workspace activation.
void InspectorWorkspacePanel::SaveLayout() const {
  preferences_.SetValue(kPageKey, std::to_string(notebook_->GetSelection()));
  preferences_.SaveUserConfig();
}

// Opens the native supported-file chooser and inspects its selection.
void InspectorWorkspacePanel::ChooseFile() {
  wxFileDialog chooser(this, _("Open MVR or GDTF file"), {}, {},
                       _("MVR and GDTF files (*.mvr;*.gdtf)|*.mvr;*.gdtf|MVR files (*.mvr)|*.mvr|GDTF files (*.gdtf)|*.gdtf"),
                       wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (chooser.ShowModal() == wxID_OK) OpenFile(WxPathUtils::FilesystemPathFromWxString(chooser.GetPath()));
}

// Refreshes the explicitly retained current-project or external source.
void InspectorWorkspacePanel::RefreshSource() {
  if (sourceKind_ == SourceKind::CurrentProject || sourceKind_ == SourceKind::None)
    InspectCurrentProject();
  else
    OpenFile(externalPath_);
}

// Captures and retains exactly one canonical current-project MVR snapshot.
void InspectorWorkspacePanel::InspectCurrentProject() {
  try {
    const auto captured = CurrentProjectInspector(project_).Capture();
    if (!captured) {
      wxMessageBox(_("The current project could not be exported as a canonical MVR snapshot."),
                   _("MVR / GDTF Inspector"), wxOK | wxICON_ERROR, this);
      return;
    }
    sourceKind_ = SourceKind::CurrentProject;
    externalPath_.clear(); retainedMvrBytes_ = captured->bytes;
    parentMvr_.reset(); identity_->SetLabel(_("Source: Current project MVR"));
    sourceType_->SetLabel(_("Active project")); back_->Hide();
    ShowMvr(captured->result);
  } catch (const std::exception &error) {
    diagnostics::DiagnosticLogger::Error(std::string("Inspector snapshot failed unexpectedly: ") + error.what());
    wxMessageBox(_("The current project could not be inspected because of an unexpected error."),
                 _("MVR / GDTF Inspector"), wxOK | wxICON_ERROR, this);
  }
}

// Dispatches a supported external file to the established Inspection Core.
void InspectorWorkspacePanel::OpenFile(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
  if (extension != ".mvr" && extension != ".gdtf") {
    wxMessageBox(_("Only MVR and GDTF files can be inspected."), _("MVR / GDTF Inspector"), wxOK | wxICON_ERROR, this);
    return;
  }
  try {
    externalPath_ = path; retainedMvrBytes_.clear(); parentMvr_.reset(); back_->Hide();
    identity_->SetLabel(wxString::Format(_("Source: %s"), PathText(path).AfterLast(wxFILE_SEP_PATH)));
    sourceType_->SetLabel(_("External file — not part of the current project"));
    if (extension == ".gdtf") {
      sourceKind_ = SourceKind::ExternalGdtf; ShowGdtf(perastage::inspection::InspectGdtf(path));
    } else {
      sourceKind_ = SourceKind::ExternalMvr; ShowMvr(perastage::inspection::InspectMvr(path));
    }
  } catch (const std::exception &error) {
    diagnostics::DiagnosticLogger::Error(std::string("Inspector failed unexpectedly: ") + error.what());
    wxMessageBox(_("The file could not be inspected because of an unexpected error."), _("MVR / GDTF Inspector"), wxOK | wxICON_ERROR, this);
  }
}

// Clears all presentation controls before showing a new immutable result.
void InspectorWorkspacePanel::ClearResult() {
  summary_->Clear(); issues_->Clear();
  if (packageModel_) {
    package_->AssociateModel(nullptr);
    packageModel_ = nullptr;
  }
  scene_->DeleteAllItems();
  diagnostics_->DeleteAllItems(); diagnosticRows_.clear();
  xml_->SetReadOnly(false); xml_->ClearAll(); xml_->SetReadOnly(true); search_->Clear();
}

// Projects high-level GDTF facts without reparsing the retained XML.
void InspectorWorkspacePanel::ShowGdtf(const perastage::inspection::GdtfInspectionResult &result, bool nested) {
  ClearResult(); wxString text;
  text << _("Format:") << " GDTF\n" << _("Status:") << ' ' << LocalizedGdtfReadStatus(result.status) << '\n';
  if (nested) text << _("Embedded resource in parent MVR") << '\n';
  AppendCommonSummary(text, result.inspection, result.validation);
  std::vector<perastage::inspection::ResourceDescriptor> resources;
  if (result.packageInventory)
    resources = nested
                    ? perastage::inspection::DescribePackageResources(
                          *result.packageInventory)
                    : perastage::inspection::DescribePackageResources(
                          result.inspection.request.sourcePath,
                          *result.packageInventory);
  if (result.document) {
    const auto &description = result.document->Description();
    text << "DataVersion: " << FromUtf8(description.dataVersion) << "\n" << _("Fixture type:") << ' ' << FromUtf8(description.fixtureTypeName) << "\n" << _("Manufacturer:") << ' ' << FromUtf8(description.manufacturer) << '\n';
    SetXml(result.document->Archive().descriptionXml);
  }
  summary_->SetValue(text); PopulatePackage(resources); PopulateDiagnostics(result.inspection, result.validation);
  navigation_->SetSelection(0);
}

// Projects high-level MVR facts from the immutable Inspection Core snapshot.
void InspectorWorkspacePanel::ShowMvr(const perastage::inspection::MvrInspectionResult &result) {
  ClearResult(); wxString text;
  text << _("Format:") << " MVR\n" << _("Status:") << ' ' << (result.Success() ? _("Readable") : _("Unusable")) << '\n';
  AppendCommonSummary(text, result.inspection, result.validation);
  std::vector<perastage::inspection::ResourceDescriptor> resources;
  if (result.packageInventory) {
    resources = retainedMvrBytes_.empty()
                    ? perastage::inspection::DescribePackageResources(result.inspection.request.sourcePath, *result.packageInventory)
                    : perastage::inspection::DescribePackageResources(retainedMvrBytes_, *result.packageInventory, 4096);
  }
  if (result.snapshot) {
    const auto &snapshot = *result.snapshot;
    text << _("MVR version:") << ' ' << snapshot.versionMajor << '.' << snapshot.versionMinor << "\n" << _("Provider:") << ' ' << FromUtf8(snapshot.provider) << '\n';
    SetXml(snapshot.sceneDescriptionXml); PopulateScene(snapshot);
  }
  summary_->SetValue(text); PopulatePackage(resources); PopulateDiagnostics(result.inspection, result.validation);
}

// Populates the safe hierarchical package projection.
void InspectorWorkspacePanel::PopulatePackage(const std::vector<perastage::inspection::ResourceDescriptor> &resources) {
  auto *model = new PackageDataViewModel(BuildPackageTree(resources));
  package_->AssociateModel(model);
  model->DecRef();
  packageModel_ = model;
}

// Populates the deterministic scene projection without traversing live state.
void InspectorWorkspacePanel::PopulateScene(const perastage::inspection::MvrInspectionSnapshot &snapshot) {
  AppendSceneNodes(*scene_, wxDataViewItem{}, BuildSceneTree(snapshot));
}

// Populates structured issues and diagnostic rows without message heuristics.
void InspectorWorkspacePanel::PopulateDiagnostics(
    const perastage::inspection::Result &inspection,
    const std::vector<perastage::inspection::ValidationResult> &validation) {
  diagnosticRows_ = AllDiagnostics(inspection, validation);
  const auto groups = BuildIssueGroups(diagnosticRows_);
  wxString issueText;
  for (const auto &group : groups)
    issueText << group.count << " × " << FromUtf8(group.code) << "  ["
              << ClassificationName(group.classification) << ", " << SeverityName(group.severity) << "]\n";
  issues_->SetValue(issueText);
  std::stable_sort(diagnosticRows_.begin(), diagnosticRows_.end(), [](const auto &left, const auto &right) {
    return std::tie(left.severity, left.classification, left.code) > std::tie(right.severity, right.classification, right.code);
  });
  for (std::size_t index = 0; index < diagnosticRows_.size(); ++index) {
    const auto &diagnostic = diagnosticRows_[index];
    const long row = diagnostics_->InsertItem(diagnostics_->GetItemCount(), SeverityName(diagnostic.severity));
    diagnostics_->SetItem(row, 1, DomainName(diagnostic.domain)); diagnostics_->SetItem(row, 2, ClassificationName(diagnostic.classification));
    diagnostics_->SetItem(row, 3, FromUtf8(diagnostic.code)); diagnostics_->SetItem(row, 4, FromUtf8(diagnostic.message));
    diagnostics_->SetItem(row, 5, FromUtf8(DiagnosticLocationText(diagnostic))); diagnostics_->SetItemData(row, static_cast<long>(index));
  }
}

// Displays retained root XML exactly as supplied by Inspection Core.
void InspectorWorkspacePanel::SetXml(const std::string &xml) {
  xml_->SetReadOnly(false); xml_->SetTextRaw(xml.c_str()); xml_->SetReadOnly(true);
}

// Finds literal UTF-8 text with deterministic directional wrap-around.
void InspectorWorkspacePanel::FindXml(bool forward) {
  const std::string query = search_->GetValue().ToUTF8().data();
  const auto rawText = xml_->GetTextRaw();
  const std::string text(rawText.data(),
                         static_cast<std::size_t>(xml_->GetTextLength()));
  const auto found = FindText(text, query, xml_->GetSelectionStart(), xml_->GetSelectionEnd(), forward);
  if (found) {
    xml_->SetSelection(static_cast<int>(*found), static_cast<int>(*found + query.size()));
    xml_->ScrollCaret(); xml_->SetFocus();
  }
}

// Opens a contextual archive-path action for the selected actual entry.
void InspectorWorkspacePanel::ShowPackageContextMenu(wxDataViewEvent &event) {
  const auto item = event.GetItem();
  const auto *data = packageModel_
                         ? static_cast<PackageDataViewModel *>(packageModel_)
                               ->Value(item)
                         : nullptr;
  wxMenu menu; constexpr int kCopyArchivePath = wxID_HIGHEST + 310;
  auto *copy = menu.Append(kCopyArchivePath, _("Copy archive path"));
  copy->Enable(data && data->CanCopyArchivePath());
  menu.Bind(wxEVT_MENU, [this, data](wxCommandEvent &) {
    if (data && !CopyText(FromUtf8(data->archivePath)))
      wxMessageBox(_("The clipboard could not be opened."), _("MVR / GDTF Inspector"), wxOK | wxICON_WARNING, this);
  }, kCopyArchivePath);
  PopupMenu(&menu);
}

// Opens an embedded GDTF when its existing Core descriptor is activated.
void InspectorWorkspacePanel::ActivatePackageEntry(wxDataViewEvent &event) {
  const auto *data = packageModel_
                         ? static_cast<PackageDataViewModel *>(packageModel_)
                               ->Value(event.GetItem())
                         : nullptr;
  if (data && data->resourceKind == perastage::inspection::ResourceKind::NestedGdtf)
    OpenNestedGdtf(data->archivePath, data->sizeKnown ? data->size : kNestedGdtfLimit);
}

// Uses the established filesystem or byte-oriented nested GDTF inspection path.
void InspectorWorkspacePanel::OpenNestedGdtf(const std::string &archivePath, std::uint64_t size) {
  const auto limit = std::max<std::uint64_t>(size, 1);
  const auto nested = sourceKind_ == SourceKind::CurrentProject
                          ? perastage::inspection::InspectNestedGdtf(retainedMvrBytes_, archivePath, limit)
                          : perastage::inspection::InspectNestedGdtf(externalPath_, archivePath, limit);
  if (!nested.gdtf) {
    wxMessageBox(_("The embedded GDTF could not be inspected."), _("MVR / GDTF Inspector"), wxOK | wxICON_ERROR, this);
    return;
  }
  if (!parentMvr_) parentMvr_ = sourceKind_ == SourceKind::CurrentProject
                                   ? perastage::inspection::InspectMvrBytes(retainedMvrBytes_)
                                   : perastage::inspection::InspectMvr(externalPath_);
  identity_->SetLabel(wxString::Format(_("Embedded GDTF: %s"), FromUtf8(archivePath)));
  sourceType_->SetLabel(_("Embedded in inspected MVR")); back_->Show(); Layout();
  ShowGdtf(*nested.gdtf, true);
}

// Restores the retained parent MVR result without rebuilding its source.
void InspectorWorkspacePanel::ReturnToParentMvr() {
  if (!parentMvr_) return;
  const auto result = std::move(*parentMvr_); parentMvr_.reset(); back_->Hide();
  if (sourceKind_ == SourceKind::CurrentProject) {
    identity_->SetLabel(_("Source: Current project MVR")); sourceType_->SetLabel(_("Active project"));
  } else {
    identity_->SetLabel(wxString::Format(_("Source: %s"), PathText(externalPath_).AfterLast(wxFILE_SEP_PATH)));
    sourceType_->SetLabel(_("External file — not part of the current project"));
  }
  ShowMvr(result); Layout();
}

// Copies a deterministic representation of the selected diagnostic.
void InspectorWorkspacePanel::CopySelectedDiagnostic() {
  const long row = diagnostics_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (row < 0) return;
  const auto &diagnostic = diagnosticRows_.at(diagnostics_->GetItemData(row));
  std::string value = std::string(SeverityName(diagnostic.severity)) + " [" + diagnostic.code + "] " + diagnostic.message;
  const auto location = DiagnosticLocationText(diagnostic); if (!location.empty()) value += " (" + location + ')';
  if (!CopyText(FromUtf8(value))) wxMessageBox(_("The clipboard could not be opened."), _("MVR / GDTF Inspector"), wxOK | wxICON_WARNING, this);
}

// Copies the complete unchanged retained XML buffer.
void InspectorWorkspacePanel::CopyAllXml() {
  if (!CopyText(xml_->GetText())) wxMessageBox(_("The clipboard could not be opened."), _("MVR / GDTF Inspector"), wxOK | wxICON_WARNING, this);
}

} // namespace gui::inspection
