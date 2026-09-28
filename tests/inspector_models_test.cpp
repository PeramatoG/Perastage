#include "inspector_models.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

namespace {

// Creates one package descriptor with explicit neutral metadata.
perastage::inspection::ResourceDescriptor Resource(
    std::string path, bool safe = true,
    perastage::inspection::PackageEntryType type =
        perastage::inspection::PackageEntryType::File) {
  perastage::inspection::ResourceDescriptor resource;
  resource.displayPath = std::move(path);
  resource.pathSafe = safe;
  resource.entryType = type;
  resource.kind = perastage::inspection::ResourceKind::Model;
  resource.size = 42;
  resource.sizeKnown = true;
  return resource;
}

// Verifies safe hierarchy, explicit folders, Unicode, metadata, and ordering.
void CheckPackageTree() {
  auto directory = Resource("models/", true,
                            perastage::inspection::PackageEntryType::Directory);
  const auto first = gui::inspection::BuildPackageTree(
      {Resource("wheels/gobo.png"), Resource("models/fixture.3ds"),
       Resource("fixtures/Áyrton.gdtf"), directory,
       Resource("../escape.glb", false), Resource("models/fixture.3ds")});
  const auto second = gui::inspection::BuildPackageTree(
      {Resource("wheels/gobo.png"), Resource("models/fixture.3ds"),
       Resource("fixtures/Áyrton.gdtf"), directory,
       Resource("../escape.glb", false), Resource("models/fixture.3ds")});
  assert(first.size() == second.size());
  const auto models = std::find_if(first.begin(), first.end(), [](const auto &node) {
    return node.name == "models";
  });
  assert(models != first.end() && !models->syntheticFolder);
  assert(models->children.size() == 2);
  assert(models->children.front().size == 42);
  assert(models->children.front().CanCopyArchivePath());
  const auto unsafe = std::find_if(first.begin(), first.end(), [](const auto &node) {
    return node.archivePath == "../escape.glb";
  });
  assert(unsafe != first.end() && unsafe->children.empty());
  assert(std::none_of(first.begin(), first.end(), [](const auto &node) {
    return node.name == "..";
  }));
}

// Verifies scene relationships, orphan retention, and auxiliary collections.
void CheckSceneTree() {
  perastage::inspection::MvrInspectionSnapshot snapshot;
  snapshot.layers.push_back({"Layer", "layer", "Main"});
  snapshot.groupObjects.push_back(
      {"GroupObject", "group", "Group", "layer", "Main"});
  snapshot.fixtures.push_back(
      {"Fixture", "fixture", "Spot", "layer", "Main", "group"});
  snapshot.trusses.push_back(
      {"Truss", "orphan", "Truss", "missing", "Missing"});
  snapshot.symdefs.push_back({"symdef", {"model.glb"}});
  snapshot.foreignUserData.push_back({"vendor", "1", "<data/>"});
  const auto tree = gui::inspection::BuildSceneTree(snapshot);
  assert(tree.size() == 4);
  const auto layer = std::find_if(tree.begin(), tree.end(), [](const auto &node) {
    return node.uuid == "layer";
  });
  assert(layer != tree.end() && layer->children.size() == 1);
  assert(layer->children.front().children.front().uuid == "fixture");
  assert(std::any_of(tree.begin(), tree.end(), [](const auto &node) {
    return node.uuid == "orphan" && node.unresolved;
  }));
}

// Verifies issue grouping uses structured fields rather than message text.
void CheckIssues() {
  using namespace perastage::inspection;
  Diagnostic standard;
  standard.severity = DiagnosticSeverity::Error;
  standard.classification = DiagnosticClassification::Standards;
  standard.code = "mvr.example";
  standard.message = "first arbitrary message";
  Diagnostic same = standard;
  same.message = "different arbitrary message";
  Diagnostic compatibility = standard;
  compatibility.classification = DiagnosticClassification::Compatibility;
  const auto groups = gui::inspection::BuildIssueGroups(
      {compatibility, same, standard});
  assert(groups.size() == 2);
  assert(groups[0].count + groups[1].count == 3);
}

// Verifies forward, reverse, wrap-around, and not-found search behavior.
void CheckSearch() {
  assert(gui::inspection::FindText("one two one", "one", 0, 0, true) == 0);
  assert(gui::inspection::FindText("one two one", "one", 0, 3, true) == 8);
  assert(gui::inspection::FindText("one two one", "one", 8, 11, true) == 0);
  assert(gui::inspection::FindText("one two one", "one", 8, 11, false) == 0);
  assert(gui::inspection::FindText("one two one", "one", 0, 3, false) == 8);
  assert(!gui::inspection::FindText("one", "missing", 0, 0, true));
}

} // namespace

// Exercises the Inspector's toolkit-independent GUI-310 presentation models.
int main() {
  CheckPackageTree();
  CheckSceneTree();
  CheckIssues();
  CheckSearch();
  return 0;
}
