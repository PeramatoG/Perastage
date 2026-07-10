#include "gdtf/gdtf_mode_data_view_model.h"

struct GdtfModeDataViewModel::Node {
  GdtfModeBrowserNodePresentation presentation;
  Node *parent = nullptr;
  std::vector<Node *> children;
};

// Creates an empty read-only mode data-view model.
GdtfModeDataViewModel::GdtfModeDataViewModel() = default;

// Replaces the node hierarchy and notifies associated controls safely.
void GdtfModeDataViewModel::SetNodes(const std::vector<GdtfModeBrowserNodePresentation> &nodes) {
  Cleared();
  ownedNodes.clear();
  roots.clear();
  byId.clear();
  for (const auto &presentation : nodes) {
    auto node = std::make_unique<Node>();
    node->presentation = presentation;
    byId[presentation.id] = node.get();
    ownedNodes.push_back(std::move(node));
  }
  for (auto &node : ownedNodes) {
    if (!node->presentation.parentId.empty()) {
      auto parent = byId.find(node->presentation.parentId);
      if (parent != byId.end()) {
        node->parent = parent->second;
        parent->second->children.push_back(node.get());
      }
    }
    if (!node->parent)
      roots.push_back(node.get());
  }
  Cleared();
}

// Returns the immutable presentation node for a data-view item.
const GdtfModeBrowserNodePresentation *GdtfModeDataViewModel::GetNode(const wxDataViewItem &item) const {
  if (!item.IsOk())
    return nullptr;
  return &static_cast<Node *>(item.GetID())->presentation;
}

// Returns a data-view item for a stable node identity.
wxDataViewItem GdtfModeDataViewModel::GetItemById(const std::string &id) const {
  auto it = byId.find(id);
  return it == byId.end() ? wxDataViewItem() : wxDataViewItem(it->second);
}

// Returns top-level browser items in source order.
std::vector<wxDataViewItem> GdtfModeDataViewModel::GetTopLevelItems() const {
  std::vector<wxDataViewItem> result;
  for (auto *root : roots)
    result.emplace_back(root);
  return result;
}

// Returns the fixed browser column count.
unsigned int GdtfModeDataViewModel::GetColumnCount() const { return ColumnCount; }

// Returns string column types for all browser columns.
wxString GdtfModeDataViewModel::GetColumnType(unsigned int) const { return "string"; }

// Reads a browser cell from the presentation adapter.
void GdtfModeDataViewModel::GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const {
  const auto *node = GetNode(item);
  if (!node) {
    variant = wxString();
    return;
  }
  switch (col) {
  case Item: variant = wxString::FromUTF8(node->item); break;
  case Address: variant = wxString::FromUTF8(node->address); break;
  case DmxRange: variant = wxString::FromUTF8(node->dmxRange); break;
  case PhysicalRange: variant = wxString::FromUTF8(node->physicalRange); break;
  case Unit: variant = wxString::FromUTF8(node->unit); break;
  default: variant = wxString(); break;
  }
}

// Rejects all edits because the 08E2 browser is read-only.
bool GdtfModeDataViewModel::SetValue(const wxVariant &, const wxDataViewItem &, unsigned int) { return false; }

// Returns the parent item for hierarchical display.
wxDataViewItem GdtfModeDataViewModel::GetParent(const wxDataViewItem &item) const {
  if (!item.IsOk())
    return wxDataViewItem();
  auto *node = static_cast<Node *>(item.GetID());
  return node->parent ? wxDataViewItem(node->parent) : wxDataViewItem();
}

// Reports whether an item has children without making cells editable.
bool GdtfModeDataViewModel::IsContainer(const wxDataViewItem &item) const {
  if (!item.IsOk())
    return true;
  return !static_cast<Node *>(item.GetID())->children.empty();
}

// Allows container rows to show browser data in every column.
bool GdtfModeDataViewModel::HasContainerColumns(const wxDataViewItem &) const {
  return true;
}

// Returns child items in the original GDTF source order.
unsigned int GdtfModeDataViewModel::GetChildren(const wxDataViewItem &item, wxDataViewItemArray &children) const {
  const auto &source = item.IsOk() ? static_cast<Node *>(item.GetID())->children : roots;
  for (auto *child : source)
    children.Add(wxDataViewItem(child));
  return static_cast<unsigned int>(source.size());
}
