#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspector_models.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"
#include "inspection/inspector_presentation.h"

#include <filesystem>
#include <optional>
#include <vector>

#include <wx/panel.h>

class IGuiPreferencesService;
class IGuiProjectSessionService;
class wxButton;
class wxDataViewEvent;
class wxDataViewCtrl;
class wxDataViewModel;
class wxDataViewTreeCtrl;
class wxListCtrl;
class wxNotebook;
class wxStaticText;
class wxStyledTextCtrl;
class wxTextCtrl;

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
                const std::vector<std::uint8_t> *packageBytes = nullptr);
  void ShowMvr(const perastage::inspection::MvrInspectionResult &result);
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
  void FindXml(bool forward);
  void ShowPackageContextMenu(wxDataViewEvent &event);
  void ActivatePackageEntry(wxDataViewEvent &event);
  void OpenNestedGdtf(const std::string &archivePath, std::uint64_t size);
  void ReturnToParentMvr();
  void CopySelectedDiagnostic();
  void CopyAllXml();

  IGuiPreferencesService &preferences_;
  const IGuiProjectSessionService &project_;
  SourceKind sourceKind_ = SourceKind::None;
  std::filesystem::path externalPath_;
  std::vector<std::uint8_t> retainedMvrBytes_;
  std::optional<perastage::inspection::MvrInspectionResult> parentMvr_;
  wxStaticText *identity_ = nullptr;
  wxStaticText *sourceType_ = nullptr;
  wxButton *back_ = nullptr;
  wxNotebook *navigation_ = nullptr;
  wxNotebook *notebook_ = nullptr;
  wxTextCtrl *summary_ = nullptr;
  GdtfInspectorDetailsPanel *gdtfDetails_ = nullptr;
  wxTextCtrl *issues_ = nullptr;
  wxPanel *diagnosticPage_ = nullptr;
  wxDataViewCtrl *package_ = nullptr;
  wxDataViewModel *packageModel_ = nullptr;
  wxDataViewTreeCtrl *scene_ = nullptr;
  wxListCtrl *diagnostics_ = nullptr;
  wxTextCtrl *search_ = nullptr;
  wxStyledTextCtrl *xml_ = nullptr;
  std::vector<perastage::inspection::Diagnostic> diagnosticRows_;
  InspectorDetailsPage preferredDetailsPage_ = InspectorDetailsPage::Summary;
  bool configuringDetailsPage_ = false;
};

} // namespace gui::inspection
