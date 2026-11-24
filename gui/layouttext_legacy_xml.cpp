#include "layouttext_legacy_xml.h"

#include <wx/base64.h>
#include <wx/log.h>
#include <wx/sstream.h>
#include <wx/xml/xml.h>

namespace layouttext {
namespace {
const wxString kArchiveStart = "<!-- Perastage legacy rich text (UTF-8 Base64): ";
const wxString kArchiveEnd = " -->";

bool SameNodes(const wxXmlNode *left, const wxXmlNode *right) {
  while (left && right) {
    if (left->GetType() != right->GetType() || left->GetName() != right->GetName() ||
        left->GetContent() != right->GetContent()) return false;
    size_t leftCount = 0, rightCount = 0;
    for (auto *attr = left->GetAttributes(); attr; attr = attr->GetNext()) {
      wxString value;
      if (!right->GetAttribute(attr->GetName(), &value) || value != attr->GetValue())
        return false;
      ++leftCount;
    }
    for (auto *attr = right->GetAttributes(); attr; attr = attr->GetNext()) ++rightCount;
    if (leftCount != rightCount || !SameNodes(left->GetChildren(), right->GetChildren())) return false;
    left = left->GetNext();
    right = right->GetNext();
  }
  return left == right;
}
}

wxString LegacyRichTextTail(const wxString &original, const wxString &nativeRoundTrip) {
  if (original.empty() || original == nativeRoundTrip) return {};

  // Carry the first recovery copy forward even if wxWidgets normalizes the
  // active XML differently when it is reopened.
  wxString active = original;
  active.Trim();
  const auto marker = active.rfind(kArchiveStart);
  if (marker != wxString::npos && active.EndsWith(kArchiveEnd)) {
    const auto bytes = active.Mid(marker + kArchiveStart.size(),
        active.size() - marker - kArchiveStart.size() - kArchiveEnd.size());
    if (!wxBase64Decode(bytes).IsEmpty())
      return "\n" + active.Mid(marker) + "\n";
  }

  const wxLogNull quietXmlProbe;
  wxStringInputStream source(active), saved(nativeRoundTrip);
  wxXmlDocument sourceXml, savedXml;
  if (sourceXml.Load(source) && savedXml.Load(saved) &&
      SameNodes(sourceXml.GetDocumentNode(), savedXml.GetDocumentNode()))
    return {};

  // Base64 keeps even invalid XML and comment delimiters out of the active XML.
  auto bytes = original.utf8_str();
  return "\n" + kArchiveStart + wxBase64Encode(bytes.data(), bytes.length()) + kArchiveEnd + "\n";
}
}
