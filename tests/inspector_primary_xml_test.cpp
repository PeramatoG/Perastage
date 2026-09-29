#include "inspection/inspector_primary_xml_policy.h"

#include <cassert>

// Verifies only the authoritative root XML entry suppresses text preview.
int main() {
  using namespace gui::inspection;
  using perastage::inspection::ResourceDescriptor;
  using perastage::inspection::ResourceKind;

  ResourceDescriptor resource;
  resource.kind = ResourceKind::XmlText;
  resource.displayPath = "GeneralSceneDescription.xml";
  assert(IsInspectorPrimaryXmlResource(
      resource, std::string("GeneralSceneDescription.xml")));

  resource.displayPath = "Folder/DESCRIPTION.XML";
  assert(IsInspectorPrimaryXmlResource(
      resource, std::string("Folder/DESCRIPTION.XML")));

  resource.displayPath = "extra/metadata.xml";
  assert(!IsInspectorPrimaryXmlResource(
      resource, std::string("Folder/DESCRIPTION.XML")));
  resource.kind = ResourceKind::Text;
  assert(!IsInspectorPrimaryXmlResource(
      resource, std::string("extra/metadata.xml")));
  return 0;
}
