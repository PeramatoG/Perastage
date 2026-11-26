/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "viewport_context_menu.h"
#include "configmanager.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "mainwindow.h"
#include "mainwindow/ids/mainwindow_command_ids.h"
#include "positiontoolpanel.h"
#include "scene_object_primitive_editing.h"
#include "selection_movement_settings.h"
#include "trusstablepanel.h"
#include "viewport_scene_editing.h"
#include "../viewer3d/interaction/scene_selection_policy.h"
#include "../viewer3d/render/viewer3d_render_style.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <wx/menu.h>

namespace gui::viewport {
namespace {
using namespace viewport_context;
using Selection = scene_grouping::ObjectSelection;

Target Resolve(const MvrScene &scene, const std::string &uuid) {
  if (scene.fixtures.contains(uuid)) return {Kind::Fixture, uuid};
  if (scene.trusses.contains(uuid)) return {Kind::Truss, uuid};
  if (scene.supports.contains(uuid)) return {Kind::Support, uuid};
  if (scene.sceneObjects.contains(uuid)) return {Kind::SceneObject, uuid};
  if (scene.groupObjects.contains(uuid)) return {Kind::Group, uuid};
  return {};
}

Selection CurrentSelection(const ConfigManager &config) {
  return {config.GetSelectedFixtures(), config.GetSelectedTrusses(),
          config.GetSelectedSupports(), config.GetSelectedSceneObjects(),
          config.GetSelectedGroupObjects()};
}

Selection ClickSelection(const ConfigManager &config, const std::string &uuid) {
  const auto clicked = viewer3d::interaction::BuildClickSelection(
      config.GetScene(), uuid,
      selection_movement_settings::LoadInteractiveTransformPolicy(config));
  return {clicked.fixtures, clicked.trusses, clicked.supports, clicked.sceneObjects, {}};
}

std::vector<Target> Targets(const Selection &selection) {
  std::vector<Target> result;
  const auto append = [&](Kind kind, const auto &values) {
    for (const auto &uuid : values) result.push_back({kind, uuid});
  };
  append(Kind::Fixture, selection.fixtures); append(Kind::Truss, selection.trusses);
  append(Kind::Support, selection.supports); append(Kind::SceneObject, selection.sceneObjects);
  append(Kind::Group, selection.groupObjects);
  return result;
}

void Select(const Selection &selection) {
  if (auto *main = MainWindow::Instance())
    main->RefreshAfterToolSelectionUpdate(selection);
}

// Preserve the existing truss type/source grouping identity.
std::string ModelKey(const Truss &truss) {
  const auto normalize = [](std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
  };
  if (!truss.perastageTypeKey.empty()) return "perastage-type:" + normalize(truss.perastageTypeKey);
  if (!truss.gdtfSpec.empty()) return "gdtf:" + normalize(truss.gdtfSpec);
  if (!truss.modelFile.empty()) return "model-file:" + normalize(truss.modelFile);
  if (!truss.symbolFile.empty()) return "symbol-file:" + normalize(truss.symbolFile);
  if (!truss.model.empty()) return "display-model:" + normalize(truss.model);
  return {};
}

bool HasGroupedMember(const MvrScene &scene, const Selection &selection) {
  if (!selection.groupObjects.empty()) return true;
  for (const auto &target : Targets(selection)) {
    std::string parent;
    switch (target.kind) {
    case Kind::Fixture:
      if (scene.fixtures.contains(target.uuid)) parent = scene.fixtures.at(target.uuid).parentGroupUuid;
      break;
    case Kind::Truss:
      if (scene.trusses.contains(target.uuid)) parent = scene.trusses.at(target.uuid).parentGroupUuid;
      break;
    case Kind::Support:
      if (scene.supports.contains(target.uuid)) parent = scene.supports.at(target.uuid).parentGroupUuid;
      break;
    case Kind::SceneObject:
      if (scene.sceneObjects.contains(target.uuid)) parent = scene.sceneObjects.at(target.uuid).parentGroupUuid;
      break;
    default: break;
    }
    if (!parent.empty() && scene.groupObjects.contains(parent)) return true;
  }
  return false;
}

// Each live menu owns dynamically allocated IDs until dispatch has completed.
struct Menu {
  wxMenu root;
  std::vector<wxWindowIDRef> ids;
  std::map<int, std::function<void()>> commands;
  void Add(wxMenu &menu, const wxString &label, std::function<void()> command) {
    ids.emplace_back(wxWindow::NewControlId());
    const int id = ids.back();
    menu.Append(id, label);
    commands.emplace(id, std::move(command));
  }
};

void Dispatch(int id, const std::string &uuid = {}) {
  if (auto *main = MainWindow::Instance()) {
    wxCommandEvent event(wxEVT_MENU, id);
    event.SetString(wxString::FromUTF8(uuid));
    main->ProcessWindowEvent(event);
  }
}

wxString Label(Action action) {
  switch (action) {
  case Action::EditFixture: return _("Edit Fixture...");
  case Action::EditPatch: return _("Edit DMX Patch...");
  case Action::EditTruss: return _("Edit Truss...");
  case Action::EditPrimitive: return _("Edit Object...");
  case Action::ConvertToTruss: return _("Convert to Truss");
  case Action::SelectType: return _("Select Similar by Type");
  case Action::SelectPosition: return _("Select Similar by Position");
  case Action::PositionTool: return _("Position Tool");
  case Action::Group: return _("Group");
  case Action::Ungroup: return _("Ungroup");
  case Action::Duplicate: return _("Duplicate");
  case Action::Delete: return _("Delete");
  default: return {};
  }
}

void SelectSimilar(const MvrScene &scene, const Target &hit, bool position) {
  Selection selection = CurrentSelection(GetDefaultGuiConfigServices().LegacyConfigManager());
  if (hit.kind == Kind::Fixture) {
    selection.fixtures.clear();
    const auto &reference = scene.fixtures.at(hit.uuid);
    for (const auto &[uuid, fixture] : scene.fixtures)
      if (position ? fixture.positionName == reference.positionName
                   : fixture.typeName == reference.typeName) selection.fixtures.push_back(uuid);
  } else if (hit.kind == Kind::Truss) {
    selection.trusses.clear();
    const auto &reference = scene.trusses.at(hit.uuid);
    for (const auto &[uuid, truss] : scene.trusses)
      if (position ? truss.positionName == reference.positionName
                   : ModelKey(truss) == ModelKey(reference)) selection.trusses.push_back(uuid);
  }
  Select(selection);
}

void AddSelectionShortcuts(Menu &menu, const MvrScene &scene, Kind kind) {
  if (kind != Kind::Fixture && kind != Kind::Truss) return;
  std::map<std::string, std::vector<std::string>> types, positions;
  std::vector<std::string> all;
  const auto collect = [&](const auto &objects, auto key) {
    for (const auto &[uuid, object] : objects) {
      all.push_back(uuid);
      const auto type = key(object);
      if (!type.empty()) types[type].push_back(uuid);
      positions[object.positionName].push_back(uuid);
    }
  };
  if (kind == Kind::Fixture) collect(scene.fixtures, [](const Fixture &f) { return f.typeName; });
  else collect(scene.trusses, ModelKey);
  if (all.empty()) return;
  positions.try_emplace("");
  const auto apply = [kind](const auto &uuids) {
    Selection selection = CurrentSelection(GetDefaultGuiConfigServices().LegacyConfigManager());
    if (kind == Kind::Fixture) selection.fixtures = uuids;
    else selection.trusses = uuids;
    Select(selection);
  };
  auto typeMenu = std::make_unique<wxMenu>();
  menu.Add(*typeMenu, kind == Kind::Fixture ? _("All fixtures") : _("All trusses"),
           [=] { apply(all); });
  for (const auto &[name, uuids] : types)
    menu.Add(*typeMenu, wxString::FromUTF8(name), [=] { apply(uuids); });
  menu.root.AppendSubMenu(typeMenu.release(), kind == Kind::Fixture
      ? _("Select by fixture type") : _("Select by model"));
  auto positionMenu = std::make_unique<wxMenu>();
  for (const auto &[name, uuids] : positions)
    menu.Add(*positionMenu, name.empty() ? _("No position") : wxString::FromUTF8(name),
             [=] { apply(uuids); });
  menu.root.AppendSubMenu(positionMenu.release(), _("Select by position"));
}

void AddRenderStyles(Menu &menu, ConfigManager &config, wxWindow &window) {
  auto styles = std::make_unique<wxMenu>();
  const std::pair<Viewer3DRenderStyle, wxString> choices[] = {
      {Viewer3DRenderStyle::Standard, _("Standard")},
      {Viewer3DRenderStyle::WhiteModel, _("Sketch mode")},
      {Viewer3DRenderStyle::Textured, _("Textured")},
      {Viewer3DRenderStyle::Wireframe, _("Wireframe")},
      {Viewer3DRenderStyle::White, _("White")},
      {Viewer3DRenderStyle::ByDeviceType, _("By device type")},
      {Viewer3DRenderStyle::ByLayer, _("By layer")},
      {Viewer3DRenderStyle::ByUniverse, _("By universe")}};
  for (const auto &[style, label] : choices) {
    menu.ids.emplace_back(wxWindow::NewControlId());
    const int id = menu.ids.back();
    styles->AppendRadioItem(id, label);
    styles->Check(id, style == ResolveViewer3DRenderStyle(config));
    menu.commands.emplace(id, [&, style] {
      config.SetValue("viewer3d_render_style", ToConfigValue(style));
      window.Refresh();
    });
  }
  menu.root.AppendSubMenu(styles.release(), _("Render style"));
}
} // namespace

void ShowContextMenu(wxWindow &window, const wxPoint &position,
                     const std::string &uuid, Kind activeKind,
                     bool threeDimensional, std::function<void()> exportImage) {
  auto *main = MainWindow::Instance();
  if (!main) return;
  auto &config = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto &scene = config.GetScene();
  Input input;
  input.hit = Resolve(scene, uuid);
  const Selection current = CurrentSelection(config);
  input.selection = Targets(current);
  for (const auto &member : scene_grouping::ExpandSelectionForGroupHighlights(
           scene, current, selection_movement_settings::LoadInteractiveTransformPolicy(config))) {
    const auto target = Resolve(scene, member);
    if (std::find(input.selection.begin(), input.selection.end(), target) == input.selection.end())
      input.selection.push_back(target);
  }
  const Selection clicked = uuid.empty() ? current : ClickSelection(config, uuid);
  input.clickedSelection = Targets(clicked);
  const bool alreadySelected = std::find(input.selection.begin(), input.selection.end(), input.hit)
                               != input.selection.end();
  const auto &effective = alreadySelected || uuid.empty() ? current : clicked;
  auto &c = input.capabilities;
  c.editFixture = FixtureTablePanel::Instance(); c.editPatch = true;
  c.editTruss = TrussTablePanel::Instance();
  if (input.hit.kind == Kind::SceneObject) {
    const auto &object = scene.sceneObjects.at(uuid);
    c.editPrimitive = scene_object_primitives::CanEditPrimitiveObject(object);
    c.convertToTruss = !object.GetPrimaryModel().empty();
  }
  const auto editableCount = effective.fixtures.size() + effective.trusses.size() +
      effective.supports.size() + effective.sceneObjects.size();
  c.positionTool = true;
  c.duplicate = editableCount > 0;
  c.group = editableCount >= 2;
  c.remove = editableCount > 0;
  c.ungroup = editableCount > 0 && HasGroupedMember(scene, effective);
  c.renderStyle = threeDimensional; c.exportImage = threeDimensional && bool(exportImage);
  c.selectionShortcuts = activeKind == Kind::Fixture || activeKind == Kind::Truss;
  const auto model = Build(input);
  if (model.replaceSelection) Select(clicked);
  Menu menu;
  for (const auto action : model.actions) {
    if (action == Action::SelectionShortcuts) { AddSelectionShortcuts(menu, scene, activeKind); continue; }
    if (action == Action::RenderStyle) { AddRenderStyles(menu, config, window); continue; }
    if (action == Action::ExportImage) { menu.Add(menu.root, _("Export image..."), exportImage); continue; }
    menu.Add(menu.root, Label(action), [&, action] {
      switch (action) {
      case Action::EditFixture: FixtureTablePanel::Instance()->EditByUuid(uuid); break;
      case Action::EditPatch: EditFixturePatch(&window, uuid); break;
      case Action::EditTruss: TrussTablePanel::Instance()->EditByUuid(uuid); break;
      case Action::EditPrimitive:
        if (scene_object_primitives::EditPrimitiveObjectByUuid(&window, config, uuid))
          main->RefreshAfterToolSceneUpdate();
        break;
      case Action::ConvertToTruss: Dispatch(ID_Tools_ConvertSceneObjectsToTruss, uuid); break;
      case Action::SelectType: SelectSimilar(scene, input.hit, false); break;
      case Action::SelectPosition: SelectSimilar(scene, input.hit, true); break;
      case Action::PositionTool:
        main->FocusPositionTool();
        break;
      case Action::Group: Dispatch(ID_Edit_Group); break;
      case Action::Ungroup: Dispatch(ID_Edit_Ungroup); break;
      case Action::Duplicate: main->DuplicateSceneSelection(threeDimensional); break;
      case Action::Delete: Dispatch(ID_Edit_Delete); break;
      default: break;
      }
    });
  }
  if (menu.root.GetMenuItemCount() == 0) return;
  const int id = window.GetPopupMenuSelectionFromUser(menu.root, position);
  const auto command = menu.commands.find(id);
  if (command != menu.commands.end()) command->second();
}
} // namespace gui::viewport
