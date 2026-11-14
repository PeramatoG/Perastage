/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "scene_alignment.h"
#include "ordered_scene_selection.h"
#include "matrixutils.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace alignment;
using namespace perastage::command;
namespace {
bool Near(double a, double b) { return std::abs(a-b) < 0.001; }
class Host : public ProjectMutationHost {
public:
  int entries = 0;
  bool fail = false;
  MvrScene before;
  MutationPublication CommitMutation(const MvrScene &scene,
      const scene_grouping::ObjectSelection &, const std::string &) override {
    if (fail) throw std::runtime_error("Injected publication failure");
    before = scene; ++entries;
    return {true,true};
  }
};
void Arithmetic() {
  const std::vector<Rectangle> input{{-12,-20,4,7},{8,20,42,100},{-50,-5,-20,55}};
  for (auto reference : {Reference::Extents,Reference::First,Reference::Last}) {
    for (auto operation : {Operation::Top,Operation::Bottom,Operation::Left,Operation::Right,
                           Operation::CenterHorizontal,Operation::CenterVertical}) {
      const auto plan = Calculate(input,reference,operation);
      assert(plan.valid && plan.changed && plan.offsets.size() == 3);
      if (reference == Reference::First) assert(plan.offsets.front() == 0);
      if (reference == Reference::Last) assert(plan.offsets.back() == 0);
      auto moved = input;
      for (size_t i = 0; i < moved.size(); ++i) {
        if (operation == Operation::Left || operation == Operation::Right || operation == Operation::CenterHorizontal) {
          moved[i].left += plan.offsets[i]; moved[i].right += plan.offsets[i];
        } else { moved[i].bottom += plan.offsets[i]; moved[i].top += plan.offsets[i]; }
      }
      assert(!Calculate(moved,reference,operation).changed);
    }
  }
  const auto left = Calculate(input,Reference::Extents,Operation::Left);
  assert(left.offsets[0] == -38 && left.offsets[1] == -58 && left.offsets[2] == 0);
  const auto center = Calculate(input,Reference::Extents,Operation::CenterHorizontal);
  assert(Near(center.offsets[0],0)); // Overall center (-50+42)/2 = -4.
  assert(!Calculate({input.front()},Reference::Extents,Operation::Left).valid);
  auto invalid = input; invalid[1].left = std::numeric_limits<double>::quiet_NaN();
  assert(!Calculate(invalid,Reference::First,Operation::Top).valid);
  invalid[1] = {3,2,-5,9};
  assert(!Calculate(invalid,Reference::First,Operation::Top).valid);
  assert(Calculate({{1e12,1e12,1e12+1,1e12+2},{-1e12,0,-1e12+0.01,0.01}},Reference::Last,Operation::Left).valid);
}
void Chronology() {
  OrderedSceneSelection order;
  order.Reconcile(MvrNodeType::Fixture,{"f"});
  order.Reconcile(MvrNodeType::Truss,{"t"});
  order.Reconcile(MvrNodeType::Support,{"s"});
  order.Reconcile(MvrNodeType::SceneObject,{"o"});
  assert(order.Items()[0].uuid == "f" && order.Items()[3].uuid == "o");
  order.Reconcile(MvrNodeType::Fixture,{"f"});
  assert(order.Items()[0].uuid == "f");
  order.Reconcile(MvrNodeType::Fixture,{});
  order.Reconcile(MvrNodeType::Fixture,{"f"});
  assert(order.Items().back().uuid == "f");
  order.Reconcile(MvrNodeType::Fixture,{"z","f","a","z"});
  assert(order.Items()[3].uuid == "f" && order.Items()[4].uuid == "a" && order.Items()[5].uuid == "z");
  order.Reconcile(MvrNodeType::Fixture,{"a","z","f"}); // Sorted table refresh is membership only.
  assert(order.Items()[3].uuid == "f");
  order.Clear(); assert(order.Items().empty());
}
void Commands() {
  for (auto plane : {Plane::XY,Plane::XZ,Plane::YZ}) {
    for (auto ref : {Reference::Extents,Reference::First,Reference::Last}) {
      for (auto op : {Operation::Top,Operation::Bottom,Operation::Left,Operation::Right,
                      Operation::CenterHorizontal,Operation::CenterVertical}) {
        MvrScene scene;
        Fixture a; a.uuid = "a"; a.transform = MatrixUtils::EulerToMatrix(10,20,30);
        a.transform.u[0] *= 2; a.transform.o = {100,200,300}; scene.fixtures[a.uuid] = a;
        Truss b; b.uuid = "b"; b.transform = MatrixUtils::EulerToMatrix(0,0,45);
        b.transform.o = {-500,600,700}; scene.trusses[b.uuid] = b;
        scene_grouping::ObjectSelection selection{{"a"},{"b"},{},{}};
        Host host; ExecutionContext context{scene,selection,host};
        const std::vector<Target> order{{MvrNodeType::Truss,"b"},{MvrNodeType::Fixture,"a"}};
        auto provider = [](const MvrScene &s,const Target &t,std::string &) -> std::optional<Bounds> {
          const auto m = scene_grouping::GetTargetWorldTransform(s,t);
          return Bounds{{m.o[0]-10,m.o[1]-20,m.o[2]-30},{m.o[0]+40,m.o[1]+70,m.o[2]+90}};
        };
        const auto result = Execute(context,order,ref,op,plane,{},provider);
        assert(result.Success() && result.mutation.sceneChanged && host.entries == 1);
        const auto afterA = scene.fixtures.at("a").transform;
        const auto afterB = scene.trusses.at("b").transform;
        assert(afterA.u == a.transform.u && afterA.v == a.transform.v && afterA.w == a.transform.w);
        assert(afterB.u == b.transform.u && afterB.v == b.transform.v && afterB.w == b.transform.w);
        const auto mapping = WorkPlane(plane);
        const auto d = WorldDelta(1,plane,op);
        for (int axis = 0; axis < 3; ++axis) if (d[axis] == 0) {
          assert(afterA.o[axis] == a.transform.o[axis] && afterB.o[axis] == b.transform.o[axis]);
        }
        if (ref == Reference::First) assert(afterB.o == b.transform.o);
        if (ref == Reference::Last) assert(afterA.o == a.transform.o);
        assert(!Execute(context,order,ref,op,plane,{},provider).mutation.sceneChanged);
        assert(host.entries == 1 && selection.fixtures == std::vector<std::string>{"a"});
        // Undo/Redo snapshots restore exact matrices, using the batch publication boundary.
        const MvrScene redo = scene; scene = host.before;
        assert(scene.fixtures.at("a").transform.o == a.transform.o);
        scene = redo; assert(scene.fixtures.at("a").transform.o == afterA.o);
        host.fail = true;
        const auto beforeFailure = scene;
        auto missing = [](const MvrScene &,const Target &,std::string &message) -> std::optional<Bounds> {
          message = "missing model"; return {};
        };
        assert(!Execute(context,order,ref,op,plane,{},missing).Success());
        assert(scene.fixtures.at("a").transform.o == beforeFailure.fixtures.at("a").transform.o);
        scene = host.before;
        assert(!Execute(context,order,ref,op,plane,{},provider).Success());
        assert(scene.fixtures.at("a").transform.o == a.transform.o && host.entries == 1);
      }
    }
  }
  const auto side = Project({{1,2,3},{4,8,9}},Plane::YZ);
  assert(side.left == -8 && side.right == -2 && side.bottom == 3 && side.top == 9);
  assert(WorldDelta(10,Plane::YZ,Operation::Left)[1] == -10);
}
void CoordinatePrecision() {
  MvrScene scene;
  Fixture fixture; fixture.uuid="f"; fixture.transform.o={1e12f,0,0}; scene.fixtures["f"]=fixture;
  Truss truss; truss.uuid="t"; truss.transform.o=fixture.transform.o; scene.trusses["t"]=truss;
  scene_grouping::ObjectSelection selection{{"f"},{"t"},{},{},{}};
  Host host; ExecutionContext context{scene,selection,host};
  auto bounds=[](const MvrScene &s,const Target &target,std::string &) -> std::optional<Bounds> {
    const auto m=scene_grouping::GetTargetWorldTransform(s,target);
    const double half=target.type==MvrNodeType::Fixture ? 100 : 200;
    return Bounds{{m.o[0]-half,-10,-10},{m.o[0]+half,10,10}};
  };
  const std::vector<Target> targets{{MvrNodeType::Fixture,"f"},{MvrNodeType::Truss,"t"}};
  const auto impossible=Execute(context,targets,Reference::First,Operation::Left,Plane::XY,{},bounds);
  assert(!impossible.Success() && host.entries==0);
  assert(scene.trusses.at("t").transform.o==truss.transform.o);
  scene.fixtures["f"].transform.o[0]=100000;
  scene.trusses["t"].transform.o[0]=110000;
  assert(Execute(context,targets,Reference::First,Operation::Left,Plane::XY,{},bounds).Success());
  assert(host.entries==1);
  assert(!Execute(context,targets,Reference::First,Operation::Left,Plane::XY,{},bounds).mutation.sceneChanged);
  assert(host.entries==1);
}

void Groups() {
  MvrScene scene;
  Fixture f; f.uuid="f"; scene.fixtures["f"]=f;
  Truss t; t.uuid="t"; t.parentGroupUuid="g"; scene.trusses["t"]=t;
  Truss u=t; u.uuid="u"; scene.trusses["u"]=u;
  GroupObject group; group.uuid="g";
  group.children={{MvrNodeType::Truss,"t"},{MvrNodeType::Truss,"u"}};
  scene.groupObjects["g"]=group;
  const auto targets=ResolveTargets(scene,{{MvrNodeType::Truss,"t"},{MvrNodeType::Fixture,"f"},
                                          {MvrNodeType::Truss,"u"}},{});
  assert(targets.diagnostic.empty() && targets.items.size()==2);
  assert(targets.items[0].uuid=="g" && targets.items[1].uuid=="f");
  const auto explicitGroup=ResolveTargets(scene,{{MvrNodeType::GroupObject,"g"},{MvrNodeType::Truss,"t"},
                                                {MvrNodeType::Fixture,"f"}},{});
  assert(explicitGroup.items.size()==2);
  assert(!ResolveTargets(scene,{{MvrNodeType::Fixture,"deleted"}},{}).diagnostic.empty());
  scene.trusses["t"].transform.o = {100,200,300};
  scene.trusses["u"].transform.o = {300,400,500};
  scene.trusses["t"].hasLocalTransform = scene.trusses["u"].hasLocalTransform = true;
  scene.trusses["u"].localTransform.o = {200,200,200};
  scene.groupObjects["g"].transform.o = {100,200,300};
  scene_grouping::ObjectSelection selection{{"f"},{"t","u"},{},{},{"g"}};
  Host host; ExecutionContext context{scene,selection,host};
  auto bounds = [](const MvrScene &s,const Target &target,std::string &) -> std::optional<Bounds> {
    const auto m = scene_grouping::GetTargetWorldTransform(s,target);
    return Bounds{{m.o[0]-10,m.o[1]-10,m.o[2]-10},{m.o[0]+10,m.o[1]+10,m.o[2]+10}};
  };
  const auto moved = Execute(context,{{MvrNodeType::GroupObject,"g"},{MvrNodeType::Truss,"t"},
                                     {MvrNodeType::Fixture,"f"}},Reference::Last,Operation::Left,Plane::XY,{},bounds);
  assert(moved.Success() && host.entries == 1);
  assert(scene.groupObjects.at("g").transform.o[0] == 0);
  assert(scene.trusses.at("t").transform.o[0] == 0);
  assert(scene.trusses.at("u").transform.o[0] == 200);
  assert(scene.fixtures.at("f").transform.o[0] == 0);
  GroupObject outer; outer.uuid="outer"; outer.children={{MvrNodeType::GroupObject,"g"}};
  scene.groupObjects["g"].parentGroupUuid="outer"; scene.groupObjects["outer"]=outer;
  const auto nested=ResolveTargets(scene,{{MvrNodeType::GroupObject,"g"},{MvrNodeType::Fixture,"f"},
                                        {MvrNodeType::GroupObject,"outer"}},{});
  assert(nested.diagnostic.empty() && nested.items.size()==2 && nested.items.front().uuid=="outer");

}
}
int main() { Arithmetic(); Chronology(); Commands(); CoordinatePrecision(); Groups(); }
