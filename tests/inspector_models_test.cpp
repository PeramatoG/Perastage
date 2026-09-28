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
  assert(models->CanCopyArchivePath());
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
  const auto fixtures =
      std::find_if(first.begin(), first.end(), [](const auto &node) {
        return node.name == "fixtures";
      });
  assert(fixtures != first.end() && fixtures->syntheticFolder);
  assert(!fixtures->CanCopyArchivePath());
}

// Counts one UUID throughout a projected scene hierarchy.
std::size_t CountUuid(const std::vector<gui::inspection::SceneTreeNode> &nodes,
                      const std::string &uuid) {
  std::size_t count = 0;
  for (const auto &node : nodes) {
    count += node.uuid == uuid ? 1 : 0;
    count += CountUuid(node.children, uuid);
  }
  return count;
}

// Finds one direct child by UUID.
const gui::inspection::SceneTreeNode *
Child(const gui::inspection::SceneTreeNode &parent, const std::string &uuid) {
  const auto found = std::find_if(
      parent.children.begin(), parent.children.end(),
      [&uuid](const auto &node) { return node.uuid == uuid; });
  return found == parent.children.end() ? nullptr : &*found;
}

// Verifies authored ChildList topology, orphan retention, and cycle handling.
void CheckSceneTree() {
  perastage::inspection::MvrInspectionSnapshot snapshot;
  snapshot.layers.push_back(
      {"Layer", "layer", "Main", {}, {}, {}, {}, {"group"}});
  snapshot.groupObjects.push_back({"GroupObject", "group", "Group", "layer",
                                   "Main", {}, {},
                                   {"screen", "projector"}});
  snapshot.videoScreens.push_back({"VideoScreen", "screen", "Screen", "layer",
                                   "Main", "group", {}, {"fixture"}});
  snapshot.fixtures.push_back({"Fixture", "fixture", "Spot", "layer", "Main",
                               "group", {}, {"focus-a"}});
  snapshot.projectors.push_back({"Projector", "projector", "Projector",
                                 "layer", "Main", "group", {}, {"object"}});
  snapshot.sceneObjects.push_back({"SceneObject", "object", "Object", "layer",
                                   "Main", "group", {}, {"focus-b"}});
  snapshot.focusPoints.push_back(
      {"FocusPoint", "focus-a", "Focus A", "layer", "Main", "group"});
  snapshot.focusPoints.push_back(
      {"FocusPoint", "focus-b", "Focus B", "layer", "Main", "group"});
  snapshot.trusses.push_back({"Truss", "orphan", "Truss", "missing"});
  snapshot.supports.push_back(
      {"Support", "cycle-a", "Cycle A", {}, {}, "cycle-b", {}, {"cycle-b"}});
  snapshot.supports.push_back(
      {"Support", "cycle-b", "Cycle B", {}, {}, "cycle-a", {}, {"cycle-a"}});
  snapshot.positions.push_back({"Position", {}, "Empty UUID A"});
  snapshot.positions.push_back({"Position", {}, "Empty UUID B"});
  snapshot.symdefs.push_back({"symdef", {"model.glb"}});
  snapshot.foreignUserData.push_back({"vendor", "1", "<data/>"});
  const auto first = gui::inspection::BuildSceneTree(snapshot);
  const auto second = gui::inspection::BuildSceneTree(snapshot);
  assert(first == second);

  const auto tree = first;
  const auto layer = std::find_if(tree.begin(), tree.end(), [](const auto &node) {
    return node.uuid == "layer";
  });
  assert(layer != tree.end() && layer->children.size() == 1);
  const auto *group = Child(*layer, "group");
  assert(group);
  const auto *screen = Child(*group, "screen");
  const auto *projector = Child(*group, "projector");
  assert(screen && projector);
  const auto *fixture = Child(*screen, "fixture");
  const auto *object = Child(*projector, "object");
  assert(fixture && object);
  assert(Child(*fixture, "focus-a"));
  assert(Child(*object, "focus-b"));
  for (const std::string uuid : {"layer", "group", "screen", "fixture",
                                 "focus-a", "projector", "object", "focus-b"})
    assert(CountUuid(tree, uuid) == 1);
  assert(std::any_of(tree.begin(), tree.end(), [](const auto &node) {
    return node.uuid == "orphan" && node.unresolved;
  }));
  assert(CountUuid(tree, "cycle-a") == 1);
  assert(CountUuid(tree, "cycle-b") == 1);
  assert(CountUuid(tree, {}) == 3); // Two positions plus foreign data.
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
  assert(groups.front().severity == DiagnosticSeverity::Error);
}

// Verifies forward, reverse, wrap-around, and not-found search behavior.
void CheckSearch() {
  assert(gui::inspection::FindText("one two one", "one", 0, 0, true) == 0);
  assert(gui::inspection::FindText("one two one", "one", 0, 3, true) == 8);
  assert(gui::inspection::FindText("one two one", "one", 8, 11, true) == 0);
  assert(gui::inspection::FindText("one two one", "one", 8, 11, false) == 0);
  assert(gui::inspection::FindText("one two one", "one", 0, 3, false) == 8);
  assert(!gui::inspection::FindText("one", "missing", 0, 0, true));
  const std::string unicode = "<Name>照明</Name><Name>照明</Name>";
  const auto firstUnicode = unicode.find("照明");
  const auto secondUnicode = unicode.find("照明", firstUnicode + 1);
  assert(gui::inspection::FindText(unicode, "照明", 0, 0, true) ==
         firstUnicode);
  assert(gui::inspection::FindText(unicode, "照明", firstUnicode,
                                   firstUnicode + std::string("照明").size(),
                                   true) == secondUnicode);
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
