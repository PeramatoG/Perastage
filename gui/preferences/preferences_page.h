#pragma once

#include <vector>
#include <wx/scrolwin.h>
#include <wx/stattext.h>

class IGuiPreferencesService;

// Pages stage control edits until the dialog explicitly applies all pages.
class PreferencesPage : public wxScrolledWindow {
public:
  explicit PreferencesPage(wxWindow *parent);
  virtual void LoadPreferences(const IGuiPreferencesService &preferences) = 0;
  virtual void ApplyPreferences(IGuiPreferencesService &preferences) = 0;

protected:
  void PrepareLayout();

private:
  void CollectWrappingLabels(wxWindow *window);
  void ReflowLabels();
  void InvalidateLayoutSizes(wxWindow *window);
  std::vector<std::pair<wxControl *, wxString>> wrappingLabels;
};
