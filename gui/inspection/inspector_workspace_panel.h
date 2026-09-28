#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspector_models.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"
#include "inspection/inspector_presentation.h"
#include "inspection/inspector_async_worker.h"
#include "inspection/inspector_source_context.h"
#include "inspection/inspector_nested_transition.h"

#include <filesystem>
#include <optional>
#include <memory>
#include <vector>

#include <wx/panel.h>

class IGuiPreferencesService;
class IGuiProjectSessionService;
class wxButton;
class wxStaticBitmap;
class wxDataViewEvent;
class wxDataViewCtrl;
class wxDataViewModel;
class wxListCtrl;
class wxNotebook;
class wxSplitterWindow;
class wxStaticText;
class wxStyledTextCtrl;
class wxTextCtrl;
class FixturePreviewPanel;
class GdtfResourceBitmapCache;

namespace gui::inspection {

class GdtfInspectorDetailsPanel;

// Presents read-only MVR and GDTF inspection results in native controls.
class InspectorWorkspacePanel final : public wxPanel {
public:
  InspectorWorkspacePanel(wxWindow *parent, IGuiPreferencesService &preferences,
                          const IGuiProjectSessionService &project);
  ~InspectorWorkspacePanel() override;

  void Activate();
  void OpenFile(const std::filesystem::path &path);

private:
  enum class SourceKind { None, CurrentProject, ExternalMvr, ExternalGdtf };

  void BuildLayout();
  void RestoreLayout();
  void SaveLayout() const;
  void ChooseFile();
  void RefreshSource();
  void InspectCurrentProject();
  void ClearResult();
  std::uint64_t BeginSourceLoad(const wxString &identity,
                                const wxString &sourceType);
  std::uint64_t BeginNestedSourceLoad(
      std::shared_ptr<const DisplayedPackageContext> parent);
  void FinishNestedSourceFailure(const wxString &message);
  void SetBackNavigationVisible(bool visible);
  void ShowGdtf(const perastage::inspection::GdtfInspectionResult &result,
                const DisplayedPackageContext &context);
  void ShowMvr(const perastage::inspection::MvrInspectionResult &result,
               const DisplayedPackageContext &context);
  void PopulatePackage(
      const std::vector<perastage::inspection::ResourceDescriptor> &resources);
  void PopulateScene(
      const perastage::inspection::MvrInspectionSnapshot &snapshot);
  void PopulateDiagnostics(
      const perastage::inspection::Result &inspection,
      const std::vector<perastage::inspection::ValidationResult> &validation);
  void ConfigureNavigation(bool gdtf);
  InspectorDetailsPage CurrentDetailsPage() const;
  void SelectDetailsPage(InspectorDetailsPage page, bool gdtf);
  void SetXml(const std::string &xml);
  void LoadCompleteXml();
  void FindXml(bool forward);
  void FoldXml(bool fold);
  void ShowPackageContextMenu(wxDataViewEvent &event);
  void ActivatePackageEntry(wxDataViewEvent &event);
  void RequestResourcePreview(wxDataViewEvent &event);
  void OpenNestedGdtf(const std::string &archivePath, bool sizeKnown,
                      std::uint64_t size);
  void ReturnToParentMvr();
  void CopySelectedDiagnostic();
  void CopyAllXml();
  void HandleAsyncResult(bool preview, std::uint64_t workerGeneration,
                         InspectorAsyncWorker::Result result);

  IGuiPreferencesService &preferences_;
  const IGuiProjectSessionService &project_;
  SourceKind sourceKind_ = SourceKind::None;
  InspectorRequestCoordinator requestCoordinator_;
  std::shared_ptr<const DisplayedPackageContext> parentContext_;
  InspectorNestedTransition nestedTransition_;
  wxStaticText *identity_ = nullptr;
  wxStaticText *sourceType_ = nullptr;
  wxButton *back_ = nullptr;
  wxNotebook *navigation_ = nullptr;
  wxNotebook *notebook_ = nullptr;
  wxTextCtrl *summary_ = nullptr;
  GdtfInspectorDetailsPanel *gdtfDetails_ = nullptr;
  wxTextCtrl *issues_ = nullptr;
  wxPanel *diagnosticPage_ = nullptr;
  wxPanel *previewPage_ = nullptr;
  wxStaticText *previewStatus_ = nullptr;
  wxStaticBitmap *previewImage_ = nullptr;
  wxTextCtrl *previewText_ = nullptr;
  FixturePreviewPanel *previewModel_ = nullptr;
  std::unique_ptr<GdtfResourceBitmapCache> previewBitmapCache_;
  wxDataViewCtrl *package_ = nullptr;
  wxDataViewModel *packageModel_ = nullptr;
  wxDataViewCtrl *scene_ = nullptr;
  wxDataViewModel *sceneModel_ = nullptr;
  wxSplitterWindow *navigationSplitter_ = nullptr;
  wxSplitterWindow *detailsSplitter_ = nullptr;
  wxSplitterWindow *previewSplitter_ = nullptr;
  wxListCtrl *diagnostics_ = nullptr;
  wxTextCtrl *search_ = nullptr;
  wxStyledTextCtrl *xml_ = nullptr;
  wxStaticText *xmlStatus_ = nullptr;
  wxButton *loadCompleteXml_ = nullptr;
  std::string exactXml_;
  std::vector<perastage::inspection::Diagnostic> diagnosticRows_;
  InspectorDetailsPage preferredDetailsPage_ = InspectorDetailsPage::Summary;
  bool configuringDetailsPage_ = false;
  double navigationRatio_ = 0.25;
  double detailsRatio_ = 0.67;
  double previewRatio_ = 0.68;
  bool splitterRatiosApplied_ = false;
  std::unique_ptr<InspectorAsyncWorker> worker_;
};

} // namespace gui::inspection
