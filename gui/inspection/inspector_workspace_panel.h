#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"

#include <filesystem>
#include <optional>
#include <vector>

#include <wx/panel.h>

class IGuiPreferencesService;
class wxButton;
class wxListCtrl;
class wxNotebook;
class wxStaticText;
class wxTextCtrl;

namespace gui::inspection {

// Presents read-only MVR and GDTF inspection results in native controls.
class InspectorWorkspacePanel final : public wxPanel {
public:
  InspectorWorkspacePanel(wxWindow *parent,
                          IGuiPreferencesService &preferences);
  ~InspectorWorkspacePanel() override;

  void OpenFile(const std::filesystem::path &path);

private:
  void BuildLayout();
  void RestoreLayout();
  void SaveLayout() const;
  void ChooseFile();
  void ClearResult();
  void ShowGdtf(const perastage::inspection::GdtfInspectionResult &result);
  void ShowMvr(const perastage::inspection::MvrInspectionResult &result);
  void PopulatePackage(
      const std::vector<perastage::inspection::ResourceDescriptor> &resources);
  void PopulateDiagnostics(
      const perastage::inspection::Result &inspection,
      const std::vector<perastage::inspection::ValidationResult> &validation);
  void SetXml(const std::string &xml);
  void FindXml(bool forward);
  void CopySelectedPath();
  void CopySelectedDiagnostic();
  void CopyAllXml();

  IGuiPreferencesService &preferences_;
  wxStaticText *identity_ = nullptr;
  wxNotebook *notebook_ = nullptr;
  wxTextCtrl *summary_ = nullptr;
  wxListCtrl *package_ = nullptr;
  wxListCtrl *diagnostics_ = nullptr;
  wxTextCtrl *search_ = nullptr;
  wxTextCtrl *xml_ = nullptr;
  std::vector<perastage::inspection::Diagnostic> diagnosticRows_;
};

} // namespace gui::inspection
