#pragma once

#include "gdtf/gdtf_mode_channel_browser.h"

#include <string>
#include <vector>

struct GdtfModeBrowserDetailRow {
  std::string key;
  std::string value;
};

struct GdtfModeBrowserNodePresentation {
  std::string id;
  std::string parentId;
  std::string item;
  std::string address;
  std::string dmxRange;
  std::string physicalRange;
  std::string unit;
  std::vector<GdtfModeBrowserDetailRow> details;
};

std::vector<GdtfModeBrowserNodePresentation>
BuildGdtfModeBrowserPresentation(const gdtf::GdtfDmxModeNode *mode);
