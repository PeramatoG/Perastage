#pragma once

#include "gdtf/gdtf_mode_channel_browser.h"

#include <string>
#include <vector>

struct GdtfModeChannelPresentation {
  std::string channelLabel;
  std::string functionLabel;
};

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
std::vector<GdtfModeChannelPresentation>
BuildGdtfModeChannelSummaryPresentation(const gdtf::GdtfDmxModeNode *mode);
