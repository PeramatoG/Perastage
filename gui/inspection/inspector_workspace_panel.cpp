#include "inspection/inspector_workspace_panel.h"

#include "diagnostics/DiagnosticLogger.h"
#include "guiconfigservices.h"
#include "inspection/inspection_report_aggregation.h"
#include "inspection/gdtf_inspector_details_panel.h"
#include "inspection/inspector_presentation.h"
#include "inspection/inspector_preview_policy.h"
#include "inspection/inspector_primary_xml.h"
#include "inspection/inspector_navigation_dispatch.h"
#include "inspection/inspector_navigation_request.h"
#include "inspection/inspector_xml_editor.h"
#include "gdtf/inspector_model_preview.h"
#include "inspector_project_source.h"
#include "inspection/nested_gdtf_inspection.h"
#include "fixturepreviewpanel.h"
#include "gdtf/gdtf_resource_bitmap_cache.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <memory>
#include <stdexcept>
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
#include <wx/statbmp.h>
#include <wx/splitter.h>
#include <wx/stc/stc.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/weakref.h>

wxDEFINE_EVENT(wxEVT_INSPECTOR_ASYNC_RESULT, wxThreadEvent);

namespace gui::inspection {
namespace {

constexpr char kPageKey[] = "inspector_workspace_details_page";
constexpr char kGdtfPageKey[] = "inspector_workspace_gdtf_page";
constexpr char kNavigationRatioKey[] = "inspector_workspace_navigation_ratio";
// Version 2 migrates unreleased equal-width layouts to the intended 2:1 split.
constexpr char kDetailsRatioKey[] = "inspector_workspace_details_ratio_v2";
constexpr char kPreviewRatioKey[] = "inspector_workspace_preview_ratio";
constexpr int kXmlLineNumberMargin = 0;
constexpr int kXmlFoldMargin = 1;
constexpr char kUuidColumnLabel[] = "UUID";

// Owns one immutable background inspection result until the GUI accepts it.
struct WorkspaceAsyncResult final : InspectorAsyncPayload {
  enum class Kind { Mvr, Gdtf, NestedGdtf, ResourcePreview };
  Kind kind = Kind::Mvr;
  std::shared_ptr<const perastage::inspection::MvrInspectionResult> mvr;
  std::shared_ptr<const perastage::inspection::GdtfInspectionResult> gdtf;
  ImmutablePackageBytes sourceBytes;
  std::filesystem::path filesystemPath;
  std::string archivePath;
  std::optional<perastage::inspection::ResourceReadResult> resource;
  std::optional<perastage::inspection::TextPreviewResult> textPreview;
  perastage::inspection::ResourceKind resourceKind =
      perastage::inspection::ResourceKind::Binary;
  std::string sourceFingerprint;
  std::shared_ptr<const std::vector<perastage::inspection::ResourceDescriptor>>
      resources;
  std::uint64_t sourceGeneration = 0;
  std::optional<InspectorPreviewTicket> previewTicket;
  // Allows one GUI-thread move into the renderer without copying large geometry.
  mutable std::optional<Mesh> preparedModel;
  std::string previewError;
};

// Carries a full-width generation through the wx event queue.
struct WorkspaceAsyncEvent {
  bool preview = false;
  std::uint64_t workerGeneration = 0;
  InspectorAsyncWorker::Result result;
};

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
      variant = node.syntheticFolder
                    ? _("Folder")
                    : node.entryType ==
                              perastage::inspection::PackageEntryType::Directory
                          ? _("Directory")
                          : _("File");
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

public:
  // Reconstructs the minimal descriptor needed by presentation and preview.
  static perastage::inspection::ResourceDescriptor
  Descriptor(const PackageTreeNode &node) {
    perastage::inspection::ResourceDescriptor descriptor;
    descriptor.entryType = node.entryType;
    descriptor.kind = node.resourceKind;
    descriptor.displayPath = node.archivePath;
    descriptor.size = node.size;
    descriptor.sizeKnown = node.sizeKnown;
    descriptor.pathSafe = node.pathSafe;
    descriptor.rawReadSupported = node.rawReadSupported;
    descriptor.textPreviewSupported = node.textPreviewSupported;
    return descriptor;
  }

private:
  // Resolves one valid native item to its owned node.
  static const ViewNode &Node(const wxDataViewItem &item) {
    return *static_cast<const ViewNode *>(item.GetID());
  }

  std::vector<std::unique_ptr<ViewNode>> roots_;
};

// Adapts the neutral scene topology to a lazy native Name/Type/UUID model.
class SceneDataViewModel final : public wxDataViewModel {
public:
  explicit SceneDataViewModel(std::vector<SceneTreeNode> roots) {
    Build(nullptr, std::move(roots), roots_);
  }

  // Returns the three explicit scene identity columns.
  unsigned int GetColumnCount() const override { return 3; }

  // Returns the string type shared by the scene columns.
  wxString GetColumnType(unsigned int) const override { return "string"; }

  // Supplies authored scene identity without deriving type from display text.
  void GetValue(wxVariant &value, const wxDataViewItem &item,
                unsigned int column) const override {
    const auto &node = Node(item).value;
    if (column == 0) {
      wxString name = FromUtf8(node.name.empty() ? node.kind : node.name);
      if (node.unresolved)
        name << ' ' << _("(unresolved parent)");
      value = name;
    }
    else if (column == 1)
      value = FromUtf8(node.kind);
    else
      value = FromUtf8(node.uuid);
  }

  // Rejects all scene edits.
  bool SetValue(const wxVariant &, const wxDataViewItem &,
                unsigned int) override { return false; }

  // Returns the authored parent or the invisible model root.
  wxDataViewItem GetParent(const wxDataViewItem &item) const override {
    return item.IsOk() && Node(item).parent
               ? wxDataViewItem(Node(item).parent)
               : wxDataViewItem{};
  }

  // Reports whether a scene node owns child topology.
  bool IsContainer(const wxDataViewItem &item) const override {
    return !item.IsOk() || !Node(item).children.empty();
  }

  // Materializes only the requested sibling range in the native control.
  unsigned int GetChildren(const wxDataViewItem &parent,
                           wxDataViewItemArray &children) const override {
    const auto &source = parent.IsOk() ? Node(parent).children : roots_;
    for (const auto &child : source)
      children.Add(wxDataViewItem(child.get()));
    return static_cast<unsigned int>(source.size());
  }

private:
  struct ViewNode {
    SceneTreeNode value;
    ViewNode *parent = nullptr;
    std::vector<std::unique_ptr<ViewNode>> children;
  };

  // Recursively owns neutral nodes without creating native tree items.
  static void Build(ViewNode *parent, std::vector<SceneTreeNode> values,
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

  // Resolves a valid native item to its stable owned node.
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
  std::array<std::size_t, 4> totals{};
  for (const auto &diagnostic : AllDiagnostics(inspection, validation))
    ++totals[static_cast<std::size_t>(diagnostic.severity)];
  text << _("Diagnostics:") << " information=" << totals[0]
       << ", warning=" << totals[1] << ", error=" << totals[2]
       << ", fatal=" << totals[3] << '\n';
  if (!validation.empty()) {
    text << _("Validation layers:");
    for (const auto &result : validation)
      text << ' ' << ValidationLayerName(result.layer) << '=' << ValidationStatusName(result.status);
    text << '\n';
  }
}

} // namespace

// Constructs the Inspector workspace and restores presentation-only state.
InspectorWorkspacePanel::InspectorWorkspacePanel(
    wxWindow *parent, IGuiPreferencesService &preferences,
    const IGuiProjectSessionService &project)
    : wxPanel(parent, wxID_ANY), preferences_(preferences), project_(project) {
  BuildLayout();
  RestoreLayout();
  Bind(wxEVT_INSPECTOR_ASYNC_RESULT, [this](wxThreadEvent &event) {
    auto result = event.GetPayload<WorkspaceAsyncEvent>();
    HandleAsyncResult(result.preview, result.workerGeneration,
                      std::move(result.result));
  });
  worker_ = std::make_unique<InspectorAsyncWorker>(
      [this](InspectorTaskDomain domain, std::uint64_t generation,
             InspectorAsyncWorker::Result result) {
        auto *event = new wxThreadEvent(wxEVT_INSPECTOR_ASYNC_RESULT);
        event->SetPayload(WorkspaceAsyncEvent{
            domain == InspectorTaskDomain::Preview, generation,
            std::move(result)});
        wxQueueEvent(this, event);
      });
}

// Persists useful workspace state without retaining inspected content.
InspectorWorkspacePanel::~InspectorWorkspacePanel() {
  worker_.reset();
  DeletePendingEvents();
  previewModel_->ResetPreview();
  SaveLayout();
}

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

  navigationSplitter_ = new wxSplitterWindow(
      this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_LIVE_UPDATE | wxSP_3D);
  navigationSplitter_->SetMinimumPaneSize(FromDIP(150));
  navigation_ = new wxNotebook(navigationSplitter_, wxID_ANY);
  package_ = new wxDataViewCtrl(navigation_, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxDV_SINGLE);
  package_->AppendTextColumn(_("Path"), 0, wxDATAVIEW_CELL_INERT, 220);
  package_->AppendTextColumn(_("Type"), 1, wxDATAVIEW_CELL_INERT, 80);
  package_->AppendTextColumn(_("Resource kind"), 2, wxDATAVIEW_CELL_INERT, 100);
  package_->AppendTextColumn(_("Size"), 3, wxDATAVIEW_CELL_INERT, 80, wxALIGN_RIGHT);
  package_->AppendTextColumn(_("Availability"), 4, wxDATAVIEW_CELL_INERT, 90);
  scene_ = new wxDataViewCtrl(navigation_, wxID_ANY, wxDefaultPosition,
                              wxDefaultSize, wxDV_SINGLE);
  scene_->AppendTextColumn(_("Name"), 0, wxDATAVIEW_CELL_INERT, 200);
  scene_->AppendTextColumn(_("Type"), 1, wxDATAVIEW_CELL_INERT, 110);
  scene_->AppendTextColumn(FromUtf8(kUuidColumnLabel), 2,
                           wxDATAVIEW_CELL_INERT, 230);
  navigation_->AddPage(package_, _("Package"));
  navigation_->AddPage(scene_, _("Scene"));
  detailsSplitter_ = new wxSplitterWindow(
      navigationSplitter_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_LIVE_UPDATE | wxSP_3D);
  detailsSplitter_->SetMinimumPaneSize(FromDIP(180));
  auto *xmlPanel = new wxPanel(detailsSplitter_);
  auto *xmlColumn = new wxBoxSizer(wxVERTICAL);
  auto *findRow = new wxBoxSizer(wxHORIZONTAL);
  search_ = new wxTextCtrl(xmlPanel, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
  auto *previous = new wxButton(xmlPanel, wxID_ANY, _("Find previous"));
  auto *next = new wxButton(xmlPanel, wxID_ANY, _("Find next"));
  auto *copyAll = new wxButton(xmlPanel, wxID_ANY, _("Copy all"));
  auto *foldAll = new wxButton(xmlPanel, wxID_ANY, _("Fold all"));
  auto *unfoldAll = new wxButton(xmlPanel, wxID_ANY, _("Unfold all"));
  loadCompleteXml_ = new wxButton(xmlPanel, wxID_ANY, _("Load complete XML"));
  xmlStatus_ = new wxStaticText(xmlPanel, wxID_ANY, {});
  findRow->Add(new wxStaticText(xmlPanel, wxID_ANY, _("Find:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  findRow->Add(search_, 1, wxRIGHT, 5); findRow->Add(previous, 0, wxRIGHT, 5);
  findRow->Add(next, 0, wxRIGHT, 5); findRow->Add(copyAll, 0, wxRIGHT, 5);
  findRow->Add(foldAll, 0, wxRIGHT, 5);
  findRow->Add(unfoldAll, 0, wxRIGHT, 5);
  findRow->Add(loadCompleteXml_);
  xml_ = new wxStyledTextCtrl(xmlPanel, wxID_ANY);
  ConfigureInspectorXmlEditor(*xml_);
  xmlColumn->Add(findRow, 0, wxEXPAND | wxBOTTOM, 6);
  xmlColumn->Add(xmlStatus_, 0, wxEXPAND | wxBOTTOM, 4);
  xmlColumn->Add(xml_, 1, wxEXPAND);
  loadCompleteXml_->Hide();
  xmlPanel->SetSizer(xmlColumn);

  previewSplitter_ = new wxSplitterWindow(
      detailsSplitter_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_LIVE_UPDATE | wxSP_3D);
  previewSplitter_->SetMinimumPaneSize(FromDIP(120));
  notebook_ = new wxNotebook(previewSplitter_, wxID_ANY);
  summary_ = new wxTextCtrl(notebook_, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  gdtfDetails_ = new GdtfInspectorDetailsPanel(notebook_);
  issues_ = new wxTextCtrl(notebook_, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  notebook_->AddPage(summary_, _("Summary"));
  notebook_->AddPage(gdtfDetails_, _("GDTF details"));
  notebook_->AddPage(issues_, _("Issues"));
  diagnosticPage_ = new wxPanel(notebook_);
  auto *diagnosticSizer = new wxBoxSizer(wxVERTICAL);
  diagnostics_ = new wxListCtrl(diagnosticPage_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
  diagnostics_->AppendColumn(_("Severity"), wxLIST_FORMAT_LEFT, 80);
  diagnostics_->AppendColumn(_("Domain"), wxLIST_FORMAT_LEFT, 80);
  diagnostics_->AppendColumn(_("Classification"), wxLIST_FORMAT_LEFT, 100);
  diagnostics_->AppendColumn(_("Code"), wxLIST_FORMAT_LEFT, 160);
  diagnostics_->AppendColumn(_("Message"), wxLIST_FORMAT_LEFT, 300);
  diagnostics_->AppendColumn(_("Location"), wxLIST_FORMAT_LEFT, 140);
  auto *copyDiagnostic = new wxButton(diagnosticPage_, wxID_ANY, _("Copy diagnostic"));
  diagnosticSizer->Add(diagnostics_, 1, wxEXPAND | wxBOTTOM, 6); diagnosticSizer->Add(copyDiagnostic, 0, wxALIGN_RIGHT);
  diagnosticPage_->SetSizer(diagnosticSizer); notebook_->AddPage(diagnosticPage_, _("Diagnostics"));
  previewPage_ = new wxPanel(previewSplitter_);
  auto *previewSizer = new wxBoxSizer(wxVERTICAL);
  previewSizer->Add(new wxStaticText(previewPage_, wxID_ANY, _("Preview")),
                    0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
  previewStatus_ = new wxStaticText(previewPage_, wxID_ANY,
                                    _("Select a package resource to preview it."));
  previewImage_ = new wxStaticBitmap(previewPage_, wxID_ANY, wxNullBitmap);
  previewText_ = new wxTextCtrl(previewPage_, wxID_ANY, {}, wxDefaultPosition,
                                wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY);
  previewModel_ = new FixturePreviewPanel(previewPage_);
  previewSizer->Add(previewStatus_, 0, wxEXPAND | wxALL, 6);
  previewSizer->Add(previewImage_, 1, wxEXPAND | wxALL, 6);
  previewSizer->Add(previewText_, 1, wxEXPAND | wxALL, 6);
  previewSizer->Add(previewModel_, 1, wxEXPAND | wxALL, 6);
  previewImage_->Hide(); previewText_->Hide(); previewModel_->Hide();
  previewPage_->SetSizer(previewSizer);
  previewBitmapCache_ = std::make_unique<GdtfResourceBitmapCache>(
      kInspectorImageCacheBytes);
  previewSplitter_->SplitHorizontally(notebook_, previewPage_);
  detailsSplitter_->SplitVertically(xmlPanel, previewSplitter_);
  navigationSplitter_->SplitVertically(navigation_, detailsSplitter_);
  root->Add(navigationSplitter_, 1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  SetSizer(root);

  Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    if (!splitterRatiosApplied_ && navigationSplitter_->GetClientSize().x > 0 &&
        detailsSplitter_->GetClientSize().x > 0 &&
        previewSplitter_->GetClientSize().y > 0) {
      navigationSplitter_->SetSashPosition(static_cast<int>(
          navigationSplitter_->GetClientSize().x * navigationRatio_));
      detailsSplitter_->SetSashPosition(static_cast<int>(
          detailsSplitter_->GetClientSize().x * detailsRatio_));
      previewSplitter_->SetSashPosition(static_cast<int>(
          previewSplitter_->GetClientSize().y * previewRatio_));
      splitterRatiosApplied_ = true;
    }
    event.Skip();
  });

  refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { RefreshSource(); });
  open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ChooseFile(); });
  back_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ReturnToParentMvr(); });
  package_->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &InspectorWorkspacePanel::ShowPackageContextMenu, this);
  package_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &InspectorWorkspacePanel::ActivatePackageEntry, this);
  package_->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                 &InspectorWorkspacePanel::RequestResourcePreview, this);
  copyDiagnostic->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { CopySelectedDiagnostic(); });
  previous->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FindXml(false); });
  next->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FindXml(true); });
  search_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { FindXml(true); });
  copyAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { CopyAllXml(); });
  foldAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FoldXml(true); });
  unfoldAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { FoldXml(false); });
  loadCompleteXml_->Bind(wxEVT_BUTTON,
                         [this](wxCommandEvent &) { LoadCompleteXml(); });
  xml_->Bind(wxEVT_STC_MARGINCLICK, [this](wxStyledTextEvent &event) {
    const int line = xml_->LineFromPosition(event.GetPosition());
    if (event.GetMargin() == kXmlFoldMargin &&
        (xml_->GetFoldLevel(line) & wxSTC_FOLDLEVELHEADERFLAG) != 0)
      xml_->ToggleFold(line);
  });
  notebook_->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent &event) {
    if (!configuringDetailsPage_)
      preferredDetailsPage_ = CurrentDetailsPage();
    event.Skip();
  });
}

// Restores the selected details page through GUI preferences.
void InspectorWorkspacePanel::RestoreLayout() {
  preferredDetailsPage_ = ParseInspectorDetailsPageToken(
      preferences_.GetValue(kPageKey).value_or("summary"));
  gdtfDetails_->SetSelectedPage(
      ReadInt(preferences_, kGdtfPageKey, 0, 0, 2));
  navigationRatio_ = ParseSplitterRatio(
      preferences_.GetValue(kNavigationRatioKey), 0.25);
  detailsRatio_ =
      ParseSplitterRatio(preferences_.GetValue(kDetailsRatioKey), 0.67);
  previewRatio_ =
      ParseSplitterRatio(preferences_.GetValue(kPreviewRatioKey), 0.68);
}

// Saves only the selected details page for the next workspace activation.
void InspectorWorkspacePanel::SaveLayout() const {
  preferences_.SetValue(kPageKey,
                        InspectorDetailsPageToken(preferredDetailsPage_));
  preferences_.SetValue(kGdtfPageKey,
                        std::to_string(gdtfDetails_->SelectedPage()));
  if (navigationSplitter_->GetClientSize().x >= FromDIP(300))
    preferences_.SetValue(
        kNavigationRatioKey,
        FormatSplitterRatio(static_cast<double>(
                                navigationSplitter_->GetSashPosition()) /
                            navigationSplitter_->GetClientSize().x));
  if (detailsSplitter_->GetClientSize().x >= FromDIP(360))
    preferences_.SetValue(
        kDetailsRatioKey,
        FormatSplitterRatio(static_cast<double>(
                                detailsSplitter_->GetSashPosition()) /
                            detailsSplitter_->GetClientSize().x));
  if (previewSplitter_->GetClientSize().y >= FromDIP(240))
    preferences_.SetValue(
        kPreviewRatioKey,
        FormatSplitterRatio(static_cast<double>(
                                previewSplitter_->GetSashPosition()) /
                            previewSplitter_->GetClientSize().y));
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
  else if (const auto context = parentContext_
                                    ? parentContext_
                                    : requestCoordinator_.DisplayedContext())
    OpenFile(context->filesystemPath);
}

// Captures and retains exactly one canonical current-project MVR snapshot.
void InspectorWorkspacePanel::InspectCurrentProject() {
  try {
    const auto input =
        CurrentProjectInspector(project_, preferences_).CaptureInput();
    sourceKind_ = SourceKind::CurrentProject;
    parentContext_.reset(); SetBackNavigationVisible(false);
    const auto sourceGeneration = BeginSourceLoad(
        _("Source: Current project MVR"), _("Active project"));
    worker_->Submit(
        [input, sourceGeneration](InspectorStopToken token)
            -> InspectorAsyncWorker::Payload {
          if (token.stop_requested())
            return InspectorAsyncWorker::Payload{};
          auto serialized = CurrentProjectInspector::Serialize(input);
          if (!serialized)
            throw std::runtime_error(
                "Canonical current-project serialization failed.");
          auto bytes = std::make_shared<const std::vector<std::uint8_t>>(
              std::move(*serialized));
          if (token.stop_requested())
            return InspectorAsyncWorker::Payload{};
          auto payload = std::make_shared<WorkspaceAsyncResult>();
          payload->kind = WorkspaceAsyncResult::Kind::Mvr;
          payload->sourceBytes = bytes;
          payload->sourceGeneration = sourceGeneration;
          payload->mvr = std::make_shared<const perastage::inspection::MvrInspectionResult>(
              perastage::inspection::InspectMvrBytes(*bytes));
          if (token.stop_requested())
            return InspectorAsyncWorker::Payload{};
          if (payload->mvr->packageInventory) {
            payload->resources = std::make_shared<const std::vector<
                perastage::inspection::ResourceDescriptor>>(
                perastage::inspection::DescribePackageResources(
                    *bytes, *payload->mvr->packageInventory, 4096));
          }
          return payload;
        });
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
    parentContext_.reset(); SetBackNavigationVisible(false);
    const auto sourceGeneration = BeginSourceLoad(
        wxString::Format(_("Source: %s"),
                         PathText(path).AfterLast(wxFILE_SEP_PATH)),
        _("External file - not part of the current project"));
    const bool gdtf = extension == ".gdtf";
    sourceKind_ = gdtf ? SourceKind::ExternalGdtf : SourceKind::ExternalMvr;
    worker_->Submit(
        [path, gdtf, sourceGeneration](InspectorStopToken token)
            -> InspectorAsyncWorker::Payload {
          if (token.stop_requested())
            return InspectorAsyncWorker::Payload{};
          auto payload = std::make_shared<WorkspaceAsyncResult>();
          payload->kind = gdtf ? WorkspaceAsyncResult::Kind::Gdtf
                               : WorkspaceAsyncResult::Kind::Mvr;
          payload->sourceGeneration = sourceGeneration;
          payload->filesystemPath = path;
          if (gdtf)
            payload->gdtf = std::make_shared<const perastage::inspection::GdtfInspectionResult>(
                perastage::inspection::InspectGdtf(path));
          else
            payload->mvr = std::make_shared<const perastage::inspection::MvrInspectionResult>(
                perastage::inspection::InspectMvr(path));
          if (token.stop_requested())
            return InspectorAsyncWorker::Payload{};
          const auto *inventory = gdtf
              ? (payload->gdtf->packageInventory
                     ? &*payload->gdtf->packageInventory : nullptr)
              : (payload->mvr->packageInventory
                     ? &*payload->mvr->packageInventory : nullptr);
          if (inventory)
            payload->resources = std::make_shared<const std::vector<
                perastage::inspection::ResourceDescriptor>>(
                perastage::inspection::DescribePackageResources(
                    path, *inventory, 4096));
          return payload;
        });
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
  if (sceneModel_) {
    scene_->AssociateModel(nullptr);
    sceneModel_ = nullptr;
  }
  diagnostics_->DeleteAllItems(); diagnosticRows_.clear();
  xml_->SetReadOnly(false); xml_->ClearAll(); xml_->SetReadOnly(true);
  xml_->SetMarginWidth(kXmlLineNumberMargin,
                       xml_->TextWidth(wxSTC_STYLE_LINENUMBER, "9") + 8);
  exactXml_.clear(); xmlStatus_->SetLabel({}); loadCompleteXml_->Hide();
  search_->Clear();
  previewStatus_->SetLabel(_("Select a package resource to preview it."));
  previewModel_->ResetPreview();
  previewImage_->Hide(); previewText_->Hide(); previewModel_->Hide();
  previewPage_->Layout();
}

// Shows or hides parent navigation and immediately recomputes header geometry.
void InspectorWorkspacePanel::SetBackNavigationVisible(bool visible) {
  back_->Show(visible);
  Layout();
}

// Invalidates old interactions and presents an immediate source-loading state.
std::uint64_t InspectorWorkspacePanel::BeginSourceLoad(
    const wxString &identity, const wxString &sourceType) {
  nestedTransition_.Cancel();
  worker_->Cancel(InspectorTaskDomain::Preview);
  const auto generation = requestCoordinator_.BeginSourceRequest();
  ClearResult();
  package_->Enable(false);
  scene_->Enable(false);
  identity_->SetLabel(identity);
  sourceType_->SetLabel(sourceType);
  summary_->SetValue(_("Inspecting source..."));
  return generation;
}

// Starts transactional nested loading without clearing the committed parent UI.
std::uint64_t InspectorWorkspacePanel::BeginNestedSourceLoad(
    std::shared_ptr<const DisplayedPackageContext> parent) {
  worker_->Cancel(InspectorTaskDomain::Preview);
  const auto generation = requestCoordinator_.BeginRetainedSourceRequest();
  nestedTransition_.Begin(std::move(parent), generation);
  package_->Enable(false);
  scene_->Enable(false);
  return generation;
}

// Restores parent interaction after a current nested transition fails.
void InspectorWorkspacePanel::FinishNestedSourceFailure(
    const wxString &message) {
  if (!nestedTransition_.IsPending())
    return;
  package_->Enable(true);
  scene_->Enable(true);
  previewStatus_->SetLabel(message);
  nestedTransition_.Cancel();
  SetBackNavigationVisible(false);
}

// Projects high-level GDTF facts without reparsing the retained XML.
void InspectorWorkspacePanel::ShowGdtf(
    const perastage::inspection::GdtfInspectionResult &result,
    const DisplayedPackageContext &context) {
  ClearResult(); ConfigureNavigation(true); wxString text;
  package_->Enable(true); scene_->Enable(true);
  text << _("Format:") << " GDTF\n" << _("Status:") << ' ' << LocalizedGdtfReadStatus(result.status) << '\n';
  if (context.packageBytes) text << _("Embedded resource in parent MVR") << '\n';
  AppendCommonSummary(text, result.inspection, result.validation);
  std::vector<perastage::inspection::ResourceDescriptor> resources;
  if (result.packageInventory) {
    resources = context.resources ? *context.resources : context.packageBytes
                    ? perastage::inspection::DescribePackageResources(
                          *context.packageBytes, *result.packageInventory, 4096,
                          result.inspection.request)
                    : perastage::inspection::DescribePackageResources(
                          result.inspection.request.sourcePath,
                          *result.packageInventory);
    text << _("Package entries:") << ' '
         << result.packageInventory->entries.size() << "\n"
         << _("Resources:") << ' ' << resources.size() << '\n';
  }
  if (result.document) {
    const auto &description = result.document->Description();
    text << "DataVersion: " << FromUtf8(description.dataVersion) << "\n"
         << _("Fixture type:") << ' ' << FromUtf8(description.fixtureTypeName)
         << "\n" << _("Manufacturer:") << ' '
         << FromUtf8(description.manufacturer) << "\n"
         << _("DMX modes:") << ' ' << description.dmxModeNames.size() << '\n';
    SetXml(result.document->Archive().descriptionXml);
  }
  summary_->SetValue(text); PopulatePackage(resources); PopulateDiagnostics(result.inspection, result.validation);
  gdtfDetails_->SetResult(result);
  navigation_->SetSelection(0);
}

// Projects high-level MVR facts from the immutable Inspection Core snapshot.
void InspectorWorkspacePanel::ShowMvr(
    const perastage::inspection::MvrInspectionResult &result,
    const DisplayedPackageContext &context) {
  ClearResult(); ConfigureNavigation(false); wxString text;
  package_->Enable(true); scene_->Enable(true);
  text << _("Format:") << " MVR\n" << _("Status:") << ' ' << (result.Success() ? _("Readable") : _("Unusable")) << '\n';
  AppendCommonSummary(text, result.inspection, result.validation);
  std::vector<perastage::inspection::ResourceDescriptor> resources;
  if (result.packageInventory) {
    resources = context.resources ? *context.resources : !context.packageBytes
                    ? perastage::inspection::DescribePackageResources(result.inspection.request.sourcePath, *result.packageInventory)
                    : perastage::inspection::DescribePackageResources(*context.packageBytes, *result.packageInventory, 4096);
    text << _("Package entries:") << ' '
         << result.packageInventory->entries.size() << "\n"
         << _("Resources:") << ' ' << resources.size() << '\n';
  }
  if (result.snapshot) {
    const auto &snapshot = *result.snapshot;
    text << _("MVR version:") << ' ' << snapshot.versionMajor << '.'
         << snapshot.versionMinor << "\n" << _("Provider:") << ' '
         << FromUtf8(snapshot.provider) << "\n" << _("Provider version:")
         << ' ' << FromUtf8(snapshot.providerVersion) << '\n';
    for (const auto &count : snapshot.nodeCounts)
      text << FromUtf8(count.type) << ": " << count.count << '\n';
    SetXml(snapshot.sceneDescriptionXml); PopulateScene(snapshot);
  }
  summary_->SetValue(text); PopulatePackage(resources); PopulateDiagnostics(result.inspection, result.validation);
}

// Switches only format-specific pages while retaining shared package and diagnostics views.
void InspectorWorkspacePanel::ConfigureNavigation(bool gdtf) {
  configuringDetailsPage_ = true;
  const int scenePage = navigation_->FindPage(scene_);
  if (gdtf && scenePage != wxNOT_FOUND)
    navigation_->RemovePage(static_cast<std::size_t>(scenePage));
  else if (!gdtf && scenePage == wxNOT_FOUND)
    navigation_->AddPage(scene_, _("Scene"));

  const int detailsPage = notebook_->FindPage(gdtfDetails_);
  if (gdtf && detailsPage == wxNOT_FOUND)
    notebook_->InsertPage(1, gdtfDetails_, _("GDTF details"));
  else if (!gdtf && detailsPage != wxNOT_FOUND)
    notebook_->RemovePage(static_cast<std::size_t>(detailsPage));
  summary_->SetName(gdtf ? _("GDTF summary") : _("MVR summary"));
  SelectDetailsPage(preferredDetailsPage_, gdtf);
  configuringDetailsPage_ = false;
  Layout();
}

// Identifies the selected details page independently of dynamic notebook indices.
InspectorDetailsPage InspectorWorkspacePanel::CurrentDetailsPage() const {
  wxWindow *selected = notebook_->GetCurrentPage();
  if (selected == gdtfDetails_) return InspectorDetailsPage::GdtfDetails;
  if (selected == issues_) return InspectorDetailsPage::Issues;
  if (selected == diagnosticPage_) return InspectorDetailsPage::Diagnostics;
  return InspectorDetailsPage::Summary;
}

// Selects an available semantic page and falls back to Summary when necessary.
void InspectorWorkspacePanel::SelectDetailsPage(InspectorDetailsPage page,
                                                bool gdtf) {
  wxWindow *target = summary_;
  if (page == InspectorDetailsPage::GdtfDetails && gdtf)
    target = gdtfDetails_;
  else if (page == InspectorDetailsPage::Issues)
    target = issues_;
  else if (page == InspectorDetailsPage::Diagnostics)
    target = diagnosticPage_;
  const int index = notebook_->FindPage(target);
  if (index != wxNOT_FOUND)
    notebook_->SetSelection(static_cast<std::size_t>(index));
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
  auto *model = new SceneDataViewModel(BuildSceneTree(snapshot));
  scene_->AssociateModel(model);
  model->DecRef();
  sceneModel_ = model;
}

// Populates structured issues and diagnostic rows without message heuristics.
void InspectorWorkspacePanel::PopulateDiagnostics(
    const perastage::inspection::Result &inspection,
    const std::vector<perastage::inspection::ValidationResult> &validation) {
  diagnosticRows_ = AllDiagnostics(inspection, validation);
  const auto groups = BuildIssueGroups(diagnosticRows_);
  wxString issueText;
  for (const auto &group : groups)
    issueText << FromUtf8(FormatIssueGroupLine(
        group.count, group.code, ClassificationName(group.classification),
        SeverityName(group.severity)));
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
  exactXml_ = xml;
  const bool bounded = xml.size() > kInspectorEagerXmlBytes;
  const std::size_t shownBytes = Utf8PrefixLength(
      xml, bounded ? static_cast<std::size_t>(kInspectorEagerXmlBytes)
                   : xml.size());
  const std::string shown = xml.substr(0, shownBytes);
  xml_->SetReadOnly(false); xml_->SetTextRaw(shown.c_str()); xml_->SetReadOnly(true);
  ColouriseInspectorXml(*xml_);
  xmlStatus_->SetLabel(bounded
                           ? _("Bounded XML preview; Find searches the loaded preview only, and the exact retained document is available on demand.")
                           : _("Complete exact XML"));
  loadCompleteXml_->Show(bounded);
  const int digits =
      static_cast<int>(XmlLineNumberDigits(xml_->GetLineCount()));
  xml_->SetMarginWidth(kXmlLineNumberMargin,
                       xml_->TextWidth(wxSTC_STYLE_LINENUMBER,
                                       wxString('9', digits)) + 8);
}

// Loads the exact retained XML only after an explicit user action.
void InspectorWorkspacePanel::LoadCompleteXml() {
  xml_->SetReadOnly(false);
  xml_->SetTextRaw(exactXml_.c_str());
  xml_->SetReadOnly(true);
  ColouriseInspectorXml(*xml_);
  xmlStatus_->SetLabel(_("Complete exact XML"));
  loadCompleteXml_->Hide();
  xml_->SetMarginWidth(
      kXmlLineNumberMargin,
      xml_->TextWidth(wxSTC_STYLE_LINENUMBER,
                      wxString('9', static_cast<int>(
                                        XmlLineNumberDigits(xml_->GetLineCount())))) +
          8);
  Layout();
}

// Finds literal UTF-8 text with deterministic directional wrap-around.
void InspectorWorkspacePanel::FindXml(bool forward) {
  const std::string query = search_->GetValue().ToUTF8().data();
  const auto rawText = xml_->GetTextRaw();
  const std::string text(rawText.data(),
                         static_cast<std::size_t>(xml_->GetTextLength()));
  const auto found = FindText(text, query, xml_->GetSelectionStart(), xml_->GetSelectionEnd(), forward);
  if (found) {
    const int line = xml_->LineFromPosition(static_cast<int>(*found));
    xml_->EnsureVisible(line);
    xml_->SetSelection(static_cast<int>(*found), static_cast<int>(*found + query.size()));
    xml_->EnsureCaretVisible(); xml_->SetFocus();
  }
}

// Expands or contracts every foldable XML section in the current buffer.
void InspectorWorkspacePanel::FoldXml(bool fold) {
  xml_->FoldAll(fold ? wxSTC_FOLDACTION_CONTRACT : wxSTC_FOLDACTION_EXPAND);
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
  if (!data)
    return;
  const auto request = BuildNestedGdtfNavigationRequest(*data);
  if (!request)
    return;
  const std::string displayName =
      diagnostics::DiagnosticLogger::FileNameOnly(request->archivePath);
  diagnostics::DiagnosticLogger::Info(
      "Inspector package item activated: " + displayName);
  wxWeakRef<InspectorWorkspacePanel> weakThis(this);
  QueueNestedGdtfNavigation(*this, *request,
                           [weakThis, displayName](
                               NestedGdtfNavigationRequest request) {
                             if (!weakThis)
                               return;
                             diagnostics::DiagnosticLogger::Info(
                                 "Inspector deferred nested GDTF open begins: " +
                                 displayName);
                             weakThis->OpenNestedGdtf(
                                 request.archivePath, request.sizeKnown,
                                 request.size);
                           });
  diagnostics::DiagnosticLogger::Info(
      "Inspector nested GDTF open queued: " + displayName);
}

// Reads and classifies only the selected payload under the explicit preview cap.
void InspectorWorkspacePanel::RequestResourcePreview(wxDataViewEvent &event) {
  worker_->Cancel(InspectorTaskDomain::Preview);
  requestCoordinator_.InvalidatePreview();
  const auto *node = packageModel_
                         ? static_cast<PackageDataViewModel *>(packageModel_)
                               ->Value(event.GetItem())
                         : nullptr;
  if (!node || node->syntheticFolder ||
      node->entryType == perastage::inspection::PackageEntryType::Directory)
    return;
  const auto descriptor = PackageDataViewModel::Descriptor(*node);
  const auto decision = DecidePreview(descriptor);
  const auto context = requestCoordinator_.DisplayedContext();
  previewModel_->ResetPreview();
  previewImage_->Hide(); previewText_->Hide(); previewModel_->Hide();
  if (context && IsInspectorPrimaryXmlResource(
                     descriptor, InspectorPrimaryXmlEntry(*context))) {
    previewStatus_->SetLabel(
        _("This document is already shown in the XML pane."));
    previewPage_->Layout();
    return;
  }
  previewStatus_->SetLabel(FromUtf8(decision.status));
  previewPage_->Layout();
  if (!decision.allowed)
    return;

  const auto ticket = requestCoordinator_.BeginPreview(context);
  if (!ticket)
    return;
  const auto entry = node->archivePath;
  const auto resourceKind = node->resourceKind;
  worker_->SubmitPreview(
      [ticket = *ticket, entry, resourceKind,
       limit = decision.maxBytes](InspectorStopToken token)
          -> InspectorAsyncWorker::Payload {
        if (token.stop_requested())
          return InspectorAsyncWorker::Payload{};
        auto payload = std::make_shared<WorkspaceAsyncResult>();
        payload->kind = WorkspaceAsyncResult::Kind::ResourcePreview;
        payload->archivePath = entry;
        payload->resourceKind = resourceKind;
        payload->sourceFingerprint = ticket.context->fingerprint;
        payload->previewTicket = ticket;
        const bool owned = static_cast<bool>(ticket.context->packageBytes);
        const auto &packageKind = ticket.context->packageKind;
        if (resourceKind == perastage::inspection::ResourceKind::Text ||
            resourceKind == perastage::inspection::ResourceKind::XmlText) {
          payload->textPreview = owned
              ? perastage::inspection::PreviewPackageText(
                    *ticket.context->packageBytes, packageKind, entry, limit)
              : perastage::inspection::PreviewPackageText(
                    ticket.context->filesystemPath, packageKind, entry, limit);
        } else {
          payload->resource = owned
              ? perastage::inspection::ReadPackageResource(
                    *ticket.context->packageBytes, packageKind, entry, limit)
              : perastage::inspection::ReadPackageResource(
                    ticket.context->filesystemPath, packageKind, entry, limit);
        }
        if (token.stop_requested())
          return InspectorAsyncWorker::Payload{};
        if (resourceKind == perastage::inspection::ResourceKind::Model &&
            payload->resource && payload->resource->Success()) {
          payload->preparedModel = PrepareInspectorModel(
              payload->resource->bytes, entry, &payload->previewError);
          payload->resource->bytes.clear();
          payload->resource->bytes.shrink_to_fit();
        }
        if (token.stop_requested())
          return InspectorAsyncWorker::Payload{};
        return payload;
      });
}

// Uses the established filesystem or byte-oriented nested GDTF inspection path.
void InspectorWorkspacePanel::OpenNestedGdtf(
    const std::string &archivePath, bool sizeKnown, std::uint64_t size) {
  const auto parent = requestCoordinator_.DisplayedContext();
  if (!parent || !parent->mvr)
    return;
  const auto decision = DecideNestedGdtfOpen(sizeKnown, size);
  if (!decision.allowed) {
    previewStatus_->SetLabel(
        _("This embedded GDTF exceeds the Inspector open limit."));
    return;
  }
  const auto sourceGeneration = BeginNestedSourceLoad(parent);
  diagnostics::DiagnosticLogger::Info(
      "Inspector nested source generation started: " +
      std::to_string(sourceGeneration));
  nestedTransition_.SetWorkerGeneration(worker_->Submit(
      [parent, archivePath, sourceGeneration,
       limit = decision.maxBytes](InspectorStopToken token)
          -> InspectorAsyncWorker::Payload {
        if (token.stop_requested())
          return InspectorAsyncWorker::Payload{};
        auto nested = parent->packageBytes
                          ? perastage::inspection::InspectNestedGdtf(
                                *parent->packageBytes, archivePath, limit,
                                parent->mvr->inspection.request)
                          : perastage::inspection::InspectNestedGdtf(
                                parent->filesystemPath, archivePath, limit);
        auto payload = std::make_shared<WorkspaceAsyncResult>();
        payload->kind = WorkspaceAsyncResult::Kind::NestedGdtf;
        payload->sourceGeneration = sourceGeneration;
        payload->archivePath = archivePath;
        if (!nested.resource.Success() || !nested.gdtf) {
          std::string codes;
          for (const auto &diagnostic : nested.resource.inspection.diagnostics) {
            if (!codes.empty())
              codes += ',';
            codes += diagnostic.code;
          }
          diagnostics::DiagnosticLogger::Warning(
              "Inspector nested GDTF resource read failed: " +
              diagnostics::DiagnosticLogger::FileNameOnly(archivePath) +
              "; codes=" + (codes.empty() ? "unknown" : codes));
          return payload;
        }
        diagnostics::DiagnosticLogger::Info(
            "Inspector nested GDTF resource read completed: " +
            diagnostics::DiagnosticLogger::FileNameOnly(archivePath));
        payload->sourceBytes =
            std::make_shared<const std::vector<std::uint8_t>>(
                std::move(nested.resource.bytes));
        if (token.stop_requested())
          return InspectorAsyncWorker::Payload{};
        payload->gdtf = std::make_shared<
            const perastage::inspection::GdtfInspectionResult>(
            std::move(*nested.gdtf));
        diagnostics::DiagnosticLogger::Info(
            std::string("Inspector nested GDTF inspection ") +
            (payload->gdtf && payload->gdtf->inspection.Success()
                 ? "completed: "
                 : "failed: ") +
            diagnostics::DiagnosticLogger::FileNameOnly(archivePath));
        if (token.stop_requested())
          return InspectorAsyncWorker::Payload{};
        if (payload->gdtf && payload->gdtf->packageInventory)
          payload->resources = std::make_shared<const std::vector<
              perastage::inspection::ResourceDescriptor>>(
              perastage::inspection::DescribePackageResources(
                  *payload->sourceBytes, *payload->gdtf->packageInventory,
                  4096));
        return payload;
      }));
}

// Restores the retained parent MVR result without rebuilding its source.
void InspectorWorkspacePanel::ReturnToParentMvr() {
  if (!parentContext_ || !parentContext_->mvr) return;
  worker_->Cancel();
  const auto parent = parentContext_;
  const auto generation = requestCoordinator_.BeginSourceRequest();
  requestCoordinator_.PublishSource(generation, parent);
  parentContext_.reset();
  SetBackNavigationVisible(false);
  if (sourceKind_ == SourceKind::CurrentProject) {
    identity_->SetLabel(_("Source: Current project MVR")); sourceType_->SetLabel(_("Active project"));
  } else {
    identity_->SetLabel(wxString::Format(
        _("Source: %s"),
        PathText(parent->filesystemPath).AfterLast(wxFILE_SEP_PATH)));
    sourceType_->SetLabel(_("External file - not part of the current project"));
  }
  ShowMvr(*parent->mvr, *parent); Layout();
}

// Publishes only the latest generation after it reaches the GUI event loop.
void InspectorWorkspacePanel::HandleAsyncResult(
    bool preview, std::uint64_t workerGeneration,
    InspectorAsyncWorker::Result completion) {
  const auto domain = preview ? InspectorTaskDomain::Preview
                              : InspectorTaskDomain::Source;
  if (workerGeneration != worker_->LatestGeneration(domain))
    return;
  if (!completion.error.empty()) {
    diagnostics::DiagnosticLogger::Error(
        std::string("Inspector operation failed unexpectedly: ") +
        completion.error);
    if (preview)
      previewStatus_->SetLabel(
          _("The selected resource could not be previewed."));
    else if (nestedTransition_.MatchesWorker(workerGeneration))
      FinishNestedSourceFailure(
          _("The embedded GDTF could not be inspected."));
    else
      summary_->SetValue(
          _("The source could not be inspected because of an unexpected error."));
    return;
  }
  if (!completion.payload) {
    if (!preview && nestedTransition_.MatchesWorker(workerGeneration))
      FinishNestedSourceFailure(
          _("The embedded GDTF could not be inspected."));
    return;
  }
  const auto &result =
      static_cast<const WorkspaceAsyncResult &>(*completion.payload);
  if (result.kind == WorkspaceAsyncResult::Kind::ResourcePreview) {
    if (!result.previewTicket ||
        !requestCoordinator_.AcceptPreview(*result.previewTicket))
      return;
    previewImage_->Hide(); previewText_->Hide(); previewModel_->Hide();
    if (result.textPreview && result.textPreview->Success()) {
      previewText_->SetValue(FromUtf8(result.textPreview->text));
      previewText_->Show();
      previewStatus_->SetLabel(_("Complete bounded text preview"));
    } else if (result.resource && result.resource->Success() &&
               result.resourceKind == perastage::inspection::ResourceKind::Image) {
      const auto decoded = previewBitmapCache_->GetOrCreate(
          result.sourceFingerprint, result.archivePath, result.resource->bytes,
          FromDIP(wxSize(480, 320)),
          wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
      previewImage_->SetBitmap(decoded.bitmap);
      previewImage_->Show();
      previewStatus_->SetLabel(FromUtf8(decoded.diagnostic));
    } else if (result.resourceKind ==
                   perastage::inspection::ResourceKind::Model) {
      const bool loaded = result.preparedModel &&
                          previewModel_->ApplyPreparedMesh(
                              std::move(*result.preparedModel));
      previewModel_->Show();
      previewStatus_->SetLabel(
          loaded ? _("Model preview loaded from prepared geometry.")
                 : _("The model preview could not be loaded."));
    } else if (result.resource && result.resource->Success() &&
               result.resourceKind ==
                   perastage::inspection::ResourceKind::NestedGdtf) {
      const bool loaded = previewModel_->LoadOwnedResource(
          result.resource->bytes, result.archivePath);
      previewModel_->Show();
      previewStatus_->SetLabel(loaded ? _("Model preview loaded from temporary owned bytes.")
                                      : _("The model preview could not be loaded."));
    } else {
      previewStatus_->SetLabel(_("The selected resource could not be previewed."));
    }
    previewPage_->Layout();
    return;
  }
  if (result.sourceGeneration != requestCoordinator_.SourceGeneration()) {
    if (result.kind == WorkspaceAsyncResult::Kind::NestedGdtf)
      diagnostics::DiagnosticLogger::Info(
          "Inspector nested GDTF result rejected as stale.");
    return;
  }
  if (result.kind == WorkspaceAsyncResult::Kind::NestedGdtf &&
      !nestedTransition_.Matches(result.sourceGeneration)) {
    diagnostics::DiagnosticLogger::Info(
        "Inspector nested GDTF result rejected without a matching transition.");
    return;
  }
  if ((result.kind == WorkspaceAsyncResult::Kind::Mvr && !result.mvr) ||
      (result.kind != WorkspaceAsyncResult::Kind::Mvr && !result.gdtf)) {
    if (result.kind == WorkspaceAsyncResult::Kind::NestedGdtf) {
      diagnostics::DiagnosticLogger::Warning(
          "Inspector nested GDTF result accepted without a usable document.");
      FinishNestedSourceFailure(
          _("The embedded GDTF could not be inspected."));
    } else {
      summary_->SetValue(_("The source could not be inspected."));
    }
    return;
  }
  auto context = std::make_shared<DisplayedPackageContext>();
  context->packageKind = result.kind == WorkspaceAsyncResult::Kind::Mvr
                             ? perastage::inspection::PackageKind::Mvr
                             : perastage::inspection::PackageKind::Gdtf;
  context->filesystemPath = result.filesystemPath;
  context->packageBytes = result.sourceBytes;
  context->fingerprint = std::to_string(result.sourceGeneration) + ':' +
                         (result.archivePath.empty() ? "source"
                                                     : result.archivePath);
  context->mvr = result.mvr;
  context->gdtf = result.gdtf;
  context->resources = result.resources
                           ? result.resources
                           : std::make_shared<const std::vector<
                                 perastage::inspection::ResourceDescriptor>>();
  if (!requestCoordinator_.PublishSource(result.sourceGeneration, context)) {
    diagnostics::DiagnosticLogger::Info(
        "Inspector source result rejected during publication.");
    return;
  }
  if (result.kind == WorkspaceAsyncResult::Kind::Mvr && result.mvr) {
    ShowMvr(*result.mvr, *context);
    return;
  }
  if (result.kind == WorkspaceAsyncResult::Kind::NestedGdtf) {
    diagnostics::DiagnosticLogger::Info(
        "Inspector nested GDTF result accepted; presentation begins.");
    parentContext_ = nestedTransition_.Commit();
  }
  ShowGdtf(*result.gdtf, *context);
  if (result.kind == WorkspaceAsyncResult::Kind::NestedGdtf) {
    identity_->SetLabel(wxString::Format(_("Embedded GDTF: %s"),
                                         FromUtf8(result.archivePath)));
    sourceType_->SetLabel(_("Embedded in inspected MVR"));
    SetBackNavigationVisible(true);
    diagnostics::DiagnosticLogger::Info(
        "Inspector nested GDTF presentation completed.");
  }
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
  if (!CopyText(FromUtf8(exactXml_))) wxMessageBox(_("The clipboard could not be opened."), _("MVR / GDTF Inspector"), wxOK | wxICON_WARNING, this);
}

} // namespace gui::inspection
