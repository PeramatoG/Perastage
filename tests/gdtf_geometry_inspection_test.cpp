#include "gdtf_geometry_inspection.h"

#include <cassert>

int main() {
  GdtfGeometryTree tree;
  for (int index = 0; index < 4; ++index) {
    GdtfNode3D node;
    node.stableName = "Geometry" + std::to_string(index);
    node.modelName = index ? "Pixel" : "Base";
    node.modelFile = index ? "pixel" : "base";
    node.parentIndex = index ? 0 : -1;
    node.hasMesh = true;
    node.mesh.vertices = {0, 0, 0, 10, 0, 0, 0, 10, 0};
    node.mesh.indices = {0, 1, 2};
    node.worldTransform.o[0] = static_cast<float>(index * 100);
    tree.nodes.push_back(node);
  }
  assert(GdtfInspectionObjects(tree).size() == 4);
  assert(GdtfInspectionObjects(tree, 0).size() == 4);
  for (int index = 1; index < 4; ++index) {
    const auto solo = GdtfInspectionObjects(tree, index);
    assert(solo.size() == 1 && solo.front().transform.o[0] == index * 100);
  }
  assert(GdtfInspectionObjects(tree, 9).empty());
  assert(GdtfInspectionObjects(tree, -2).empty());
  assert(tree.nodes.size() == 4 && tree.nodes[2].stableName == "Geometry2");
  assert(tree.nodes[2].parentIndex == 0 && tree.nodes[2].worldTransform.o[0] == 200);
  assert(tree.nodes[1].modelFile == tree.nodes[2].modelFile);
}
