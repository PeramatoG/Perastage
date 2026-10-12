#include "gdtf_thumbnail.h"
#include "gdtf_archive_reader.h"

#include <algorithm>
#include <tinyxml2.h>

namespace gdtf {

std::vector<ThumbnailResource> InspectGdtfThumbnails(const std::filesystem::path &source) {
  std::vector<ThumbnailResource> result;
  const auto archive = ReadGdtfArchive(source);
  if (!archive.Success())
    return result;
  tinyxml2::XMLDocument document;
  if (document.Parse(archive.descriptionXml.c_str(), archive.descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS)
    return result;
  const auto *root = document.FirstChildElement("GDTF");
  const auto *fixture = root ? root->FirstChildElement("FixtureType") : nullptr;
  const char *attribute = fixture ? fixture->Attribute("Thumbnail") : nullptr;
  const std::string base = attribute ? attribute : "";
  const auto add = [&](const std::string &path, bool svg, bool compatibility) {
    const auto count = std::count_if(archive.entries.begin(), archive.entries.end(),
        [&](const auto &entry) { return !entry.directory && entry.path == path; });
    if (count == 1)
      result.push_back({path, svg, compatibility});
  };
  if (!base.empty() && base != "." && base != ".." &&
      base.find_first_of("/\\:") == std::string::npos) {
    add(base + ".svg", true, false);
    add(base + ".png", false, false);
    if (!result.empty())
      return result;
    add(base, base.ends_with(".svg"), true);
    for (const auto *extension : {".png", ".jpg", ".jpeg"}) {
      if (std::string(extension) != ".png")
        add(base + extension, false, true);
      add("thumbnails/" + base + extension, false, true);
    }
  }
  if (base != "thumbnail") {
    add("thumbnail.svg", true, true);
    add("thumbnail.png", false, true);
  }
  add("thumbnail.jpg", false, true);
  add("thumbnail.jpeg", false, true);
  return result;
}

} // namespace gdtf
