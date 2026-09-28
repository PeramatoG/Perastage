#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspector_models.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"
#include "inspection/inspector_presentation.h"
#include "inspection/inspector_async_worker.h"

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
  void ShowGdtf(const perastage::inspection::GdtfInspectionResult &result,
                const std::vector<std::uint8_t> *packageBytes = nullptr,
                const std::vector<perastage::inspection::ResourceDescriptor>
                    *preparedResources = nullptr);
  void ShowMvr(const perastage::inspection::MvrInspectionResult &result,
               const std::vector<perastage::inspection::ResourceDescriptor>
                   *preparedResources = nullptr);
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
  void ShowPackageContextMenu(wxDataViewEvent &event);
  void ActivatePackageEntry(wxDataViewEvent &event);
  void RequestResourcePreview(wxDataViewEvent &event);
  void OpenNestedGdtf(const std::string &archivePath, std::uint64_t size);
  void ReturnToParentMvr();
  void CopySelectedDiagnostic();
  void CopyAllXml();
  void HandleAsyncResult(std::uint64_t generation,
                         InspectorAsyncWorker::Payload payload);

  IGuiPreferencesService &preferences_;
  const IGuiProjectSessionService &project_;
  SourceKind sourceKind_ = SourceKind::None;
  std::filesystem::path externalPath_;
  std::vector<std::uint8_t> retainedMvrBytes_;
  std::vector<std::uint8_t> displayedPackageBytes_;
  perastage::inspection::PackageKind displayedPackageKind_ =
      perastage::inspection::PackageKind::Mvr;
  std::optional<perastage::inspection::MvrInspectionResult> parentMvr_;
  std::vector<perastage::inspection::ResourceDescriptor> parentResources_;
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
  double detailsRatio_ = 0.66;
  bool splitterRatiosApplied_ = false;
  std::unique_ptr<InspectorAsyncWorker> worker_;
  std::uint64_t displayedGeneration_ = 0;
  std::string sourceFingerprint_;
};

} // namespace gui::inspection
