#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <wx/bitmap.h>
#include <wx/combo.h>

class wxSizer;
namespace typography { class FontCatalog; }

namespace gui {
class DocumentFontChoice : public wxComboCtrl {
public:
  enum class Origin { Included, Installed };
  struct Entry {
    std::string family;
    Origin origin;
  };
  using PreviewHandler = std::function<void(std::optional<std::string_view>)>;
  using SampleRenderer = std::function<wxBitmap(std::string_view, const wxSize &)>;

  DocumentFontChoice(wxWindow *parent, std::string original);
  ~DocumentFontChoice() override;
  const std::vector<Entry> &Entries() const { return entries_; }
  const std::string &GetReference() const { return requested_; }
  std::string GetFamily() const;
  void SelectFamily(std::string_view reference);
  bool ConfirmFamily(std::string_view family);
  void RefreshFamilies();
  void CancelBrowsing();
  void SetFaceStyle(bool bold, bool italic);
  void SetPreviewHandler(PreviewHandler handler) { preview_ = std::move(handler); }
  void SetSampleRenderer(SampleRenderer renderer) { renderSample_ = std::move(renderer); }
  void SetCommitHandler(std::function<void()> handler) { commit_ = std::move(handler); }

private:
  friend class DocumentFontPopup;
  void Populate();
  void UpdateCapability();
  bool implicitDefault_;
  bool bold_ = false, italic_ = false;
  std::string requested_;
  std::optional<size_t> selected_;
  std::vector<Entry> entries_;
  std::shared_ptr<const typography::FontCatalog> catalog_;
  PreviewHandler preview_;
  SampleRenderer renderSample_;
  std::function<void()> commit_;
};

wxSizer *DocumentFontRow(wxWindow *parent, DocumentFontChoice *choice);
}
