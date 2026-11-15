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
#include "configmanager.h"
#include "fixture_label_presentation.h"
#include "LayoutManager.h"
#include "LayoutLabelDocument.h"
#include <cassert>

void RunLayoutFixtureLabelPresentationTests() {
  auto &cfg=ConfigManager::Get(); cfg.Reset();
  auto &manager=layouts::LayoutManager::Get(); manager.ResetToDefault(cfg);
  Fixture fixture; fixture.uuid="fixture"; fixture.fixtureId=17; fixture.instanceName="Name"; fixture.address="1.017";
  cfg.GetScene().fixtures.emplace(fixture.uuid, fixture);
  viewer2d::FixtureLabelOverrideMap project;
  viewer2d::LabelFieldStyle base; base.size=.2f;
  project[viewer2d::kLabelDefaultsKey].fieldStyles[0][0]=base;
  auto specific=base; specific.size=.3f;
  project[fixture.uuid].fieldStyles[0][0]=specific;
  viewer2d::SaveFixtureLabelOverrides(cfg, project);
  const auto savedProject=cfg.GetValue("label_fixture_overrides");
  layouts::LayoutDefinition page; page.name="Labels";
  layouts::Layout2DViewDefinition a; a.id=1; a.frame={0,0,200,200};
  auto b=a; b.id=2;
  page.view2dViews={a,b}; assert(manager.AddLayout(page));
  layouts::FixtureLabelStyleProfile profile{"stable-id", "Compact", {}};
  auto compact=base; compact.size=.1f; compact.x=.4f;
  profile.fields[0][0]=compact;
  layouts::FixtureLabelProfiles profiles{{profile.id,profile}};
  a.labelPresentation.profileId=profile.id;
  cfg.ClearHistory(); cfg.MarkSaved();
  const auto renderRevision=cfg.GetRenderRevision();
  assert(manager.CommitLabelPresentation(page.name,a,profiles));
  assert(cfg.GetRenderRevision()==renderRevision);
  assert(cfg.CanUndo()); cfg.Undo(); assert(manager.GetLayouts().labelProfiles.empty());
  assert(!cfg.CanUndo()); cfg.Redo(); assert(manager.GetLayouts().labelProfiles.at(profile.id)==profile);
  b.labelPresentation.profileId=profile.id;
  assert(manager.CommitLabelPresentation(page.name,b,profiles));
  auto other=page; other.name="Other page"; other.view2dViews={a};
  assert(manager.AddLayout(other));
  const auto otherBefore=layouts::ToJson(other);
  const auto resolve=[&](const auto &view) { return viewer2d::BuildLabelPresentationContext(cfg,view.labelPresentation,manager.GetLayouts().labelProfiles); };
  assert(resolve(a)->styles.at(fixture.uuid).fieldStyles[0][0]==compact);
  assert(resolve(b)->styles.at(fixture.uuid).fieldStyles[0][0]==compact);
  profiles.at(profile.id).name="Renamed";
  profiles.at(profile.id).fields[0][0]->size=.15f;
  assert(manager.CommitLabelPresentation(page.name,a,profiles));
  assert(resolve(a)->styles.at(fixture.uuid).fieldStyles[0][0]->size==.15f);
  assert(resolve(b)->styles.at(fixture.uuid).fieldStyles[0][0]->size==.15f);
  assert(manager.GetLayouts().labelProfiles.at(profile.id).id=="stable-id");
  auto duplicate=profiles.at(profile.id); duplicate.id="copy"; duplicate.name="Copy";
  profiles.emplace(duplicate.id,duplicate);
  assert(manager.CommitLabelPresentation(page.name,a,profiles));
  auto removed=profiles; removed.erase(profile.id);
  const auto prior=cfg.GetValue("layouts_collection");
  cfg.ClearHistory();
  assert(!manager.CommitLabelPresentation(page.name,a,removed));
  assert(cfg.GetValue("layouts_collection")==prior && !cfg.CanUndo());
  removed=profiles; removed.erase("copy");
  assert(manager.CommitLabelPresentation(page.name,a,removed)); profiles=removed;
  auto custom=compact; custom.visible=false; custom.y=-.8f; custom.color={1,0,0};
  b.labelPresentation.fields[0][0]=custom;
  assert(manager.CommitLabelPresentation(page.name,b,profiles));
  assert(resolve(a)->styles.at(fixture.uuid).fieldStyles[0][0]->visible);
  assert(!resolve(b)->styles.at(fixture.uuid).fieldStyles[0][0]->visible);
  auto local=custom; local.visible=true; local.x=3;
  b.labelPresentation.fixtures[fixture.uuid][0][0]=local;
  assert(manager.CommitLabelPresentation(page.name,b,profiles));
  assert(resolve(b)->styles.at(fixture.uuid).fieldStyles[0][0]==local);
  b.labelPresentation.fixtures.clear(); b.labelPresentation.fields={};
  assert(manager.CommitLabelPresentation(page.name,b,profiles));
  assert(resolve(b)->styles.at(fixture.uuid).fieldStyles[0][0]->size==.15f);
  b.labelPresentation={};
  assert(manager.CommitLabelPresentation(page.name,b,profiles));
  assert(!resolve(b));
  assert(viewer2d::ResolveFieldStyle(cfg,project,fixture.uuid,0,0)==specific);
  assert(cfg.GetValue("label_fixture_overrides")==savedProject);
  for (const auto &p : manager.GetLayouts().Items()) if (p.name==other.name) assert(layouts::ToJson(p)==otherBefore);
  auto invalid=a; invalid.labelPresentation.profileId="missing";
  const auto beforeInvalid=cfg.GetValue("layouts_collection");
  cfg.ClearHistory();
  assert(!manager.CommitLabelPresentation(page.name,invalid,profiles));
  assert(cfg.GetValue("layouts_collection")==beforeInvalid && !cfg.CanUndo());
  auto invalidFields=b; auto badStyle=base; badStyle.size=-1;
  invalidFields.labelPresentation.fields[0][0]=badStyle;
  assert(!manager.CommitLabelPresentation(page.name,invalidFields,profiles));
  assert(cfg.GetValue("layouts_collection")==beforeInvalid && !cfg.CanUndo());
  // No-op and cancelled drafts never publish history.
  assert(manager.CommitLabelPresentation(page.name,b,profiles)); assert(!cfg.CanUndo());
  auto cancelled=b; cancelled.labelPresentation.fields[0][0]=custom;
  assert(!cfg.CanUndo());
  // Document round trip retains dependencies separately from viewport references.
  const auto json=layouts::LayoutDocumentJson(manager.GetLayouts());
  std::vector<layouts::LayoutDefinition> pages; std::string error;
  assert(layouts::FromTemplateDocument(json,pages,&error));
  layouts::LayoutCollection restored; restored.Items()=pages;
  layouts::ReadLayoutLabelProfiles(json,restored);
  assert(layouts::LayoutDocumentJson(restored)==json);
  // A portable page carries its dependency closure and remaps conflicting IDs.
  auto portable=page; portable.view2dViews={a}; portable.portableLabelProfiles=profiles;
  const auto portableJson=layouts::ToTemplateDocument({portable});
  assert(layouts::FromTemplateDocument(portableJson,pages,&error));
  auto dest=profiles; dest.at(profile.id).fields[0][0]->size=.9f;
  layouts::ImportLabelDependencies(pages[0],dest);
  assert(pages[0].view2dViews[0].labelPresentation.profileId==profile.id+"-2");
  assert(dest.at(profile.id+"-2").fields==profiles.at(profile.id).fields);
  assert(pages[0].portableLabelProfiles.empty());
  auto repeat=portable; layouts::ImportLabelDependencies(repeat,dest);
  assert(repeat.view2dViews[0].labelPresentation.profileId==profile.id+"-2");
  // Missing project profile references reject loading instead of resetting defaults.
  auto missing=json; missing.erase("fixtureLabelProfiles");
  cfg.SetValue("layouts_collection",missing.dump());
  const auto preserved=layouts::LayoutDocumentJson(manager.GetLayouts());
  assert(!manager.LoadFromConfig(cfg));
  assert(layouts::LayoutDocumentJson(manager.GetLayouts())==preserved);
  // Editor inheritance uses the target viewport's legacy metrics without config mutation.
  auto legacyOptions=b.renderOptions; legacyOptions.labelFontSizeId=7;
  const auto editorInheritance=viewer2d::SnapshotLayoutLabelEditorInheritance(cfg,legacyOptions);
  assert(viewer2d::ResolveFieldStyle(cfg,editorInheritance,fixture.uuid,0,1).size==7/25.f);
  assert(cfg.GetValue("label_fixture_overrides")==savedProject);
  // The composition cache checks effective style values when the UUID is identical.
  const typography::FontPair fonts(PERASTAGE_TEST_RESOURCE_DIR);
  viewer2d::FixtureLabelCompositionCache cache;
  auto ca=viewer2d::OverlayLabelPresentation(project,a.labelPresentation,profiles);
  auto cb=ca; cb[fixture.uuid].fieldStyles[0][0]=custom;
  const auto first=cache.Resolve(cfg,ca,fixture,0,{},fonts);
  assert(!first.text.empty());
  assert(!cache.Resolve(cfg,cb,fixture,0,{},fonts).fields[0]);
  assert(!cache.Resolve(cfg,ca,fixture,0,{},fonts).text.empty());
  // Bottom retains the legacy Side style key, with Top-plane bounds projection.
  assert(viewer2d::ResolveFieldStyle(cfg,ca,fixture.uuid,3,0)==viewer2d::ResolveFieldStyle(cfg,ca,fixture.uuid,2,0));
  cfg.Reset();
}
