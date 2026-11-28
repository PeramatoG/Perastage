// This file is part of Perastage. License: GPL-3.0-or-later.
#pragma once

#include "LayoutCollection.h"
#include "LayoutElementRef.h"
#include "properties_panel.h"

namespace layouts { class LayoutManager; }
namespace gui::layoutproperties {
enum Property {
  Name, Paper, Orientation, X, Y, Width, Height, Frame, Background, ChannelColumn,
  Font, Size, Bold, Italic, Alignment, Ink, Advanced, Labels, BringFront, SendBack, Info
};
struct Context {
  std::string page;
  std::vector<layouts::LayoutElementRef> selection;
  std::size_t revision = 0;
  bool operator==(const Context &) const = default;
};
inline const layouts::LayoutDefinition *FindPage(const layouts::LayoutCollection &collection,
                                                 const std::string &name) {
  for (const auto &page : collection.Items())
    if (page.name == name) return &page;
  return nullptr;
}
properties::Sheet Build(const layouts::LayoutDefinition *page,
                        const layouts::FixtureLabelProfiles &profiles, const Context &context);
bool Edit(layouts::LayoutManager &manager, const Context &context,
          Property property, const properties::Value &value);
} // namespace gui::layoutproperties
