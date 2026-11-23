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
#pragma once
#include "fixture_label_preview.h"
#include "viewer2dpdfexporter.h"

// Compare actual viewer/capture commands with the actual editor composition for
// an unstyled fixture: this is the path previously rendered in a system font.
inline void CheckFixtureLabelCompositionParity(MainWindow &window,const std::filesystem::path &scratch,
    bool bidi = false) {
  auto &cfg=ConfigManager::Get(); auto *panel=Viewer2DPanel::Instance();
  const auto originalOverrides=cfg.GetValue("label_fixture_overrides");
  const auto originalSelection=cfg.GetSelectedFixtures();
  Fixture fixture=cfg.GetScene().fixtures.begin()->second;
  fixture.uuid="22222222-2222-4222-8222-222222222222";
  fixture.instanceName=bidi ? "Lamp24 مرحبا Rack12 שלום x́ ffi" : "MAC Quantum Profile"; fixture.fixtureId=204; fixture.address="3.94";
  fixture.transform.o={0,0,0};
  cfg.GetScene().fixtures.emplace(fixture.uuid,fixture);
  struct Restore {
    ConfigManager &cfg; Viewer2DPanel &panel; std::string uuid;
    ~Restore() { cfg.GetScene().fixtures.erase(uuid); panel.UpdateScene(true); }
  } restore{cfg,*panel,fixture.uuid};
  panel->UpdateScene(true); panel->Update();
  CommandBuffer captured; Viewer2DViewState capturedState;
  panel->CaptureFrameNow([&](CommandBuffer data,Viewer2DViewState state){captured=std::move(data);capturedState=state;});
  std::vector<TextCommand> actual;
  for (size_t i=0;i<captured.commands.size();++i)
    if (i<captured.sources.size() && captured.sources[i]=="label:"+fixture.uuid)
      if (const auto *text=std::get_if<TextCommand>(&captured.commands[i])) actual.push_back(*text);
  if (actual.empty()) throw std::runtime_error("Inherited fixture was omitted from canonical viewer capture");
  FixtureLabelEditSession session{_("Project defaults"),_("Selected project fixtures"),
      viewer2d::LoadFixtureLabelOverrides(cfg),{},[](const auto &){},std::vector<std::string>{fixture.uuid}};
  FixtureLabelEditor editor(&window,cfg,0,&session); editor.Show();
  FixtureLabelPreview *preview=nullptr;
  for (auto *child:editor.GetChildren()) if (auto *candidate=dynamic_cast<FixtureLabelPreview *>(child)) preview=candidate;
  if (!preview) throw std::runtime_error("Missing actual editor preview");
  preview->Update(); const auto &expected=preview->GetComposition();
  if (actual.size()!=expected.text.size()) throw std::runtime_error("Viewer/editor inherited glyph counts differ");
  std::string name;
  for (size_t i=0;i<actual.size();++i) {
    const auto &a=actual[i], &b=expected.text[i]; if (!a.style.clusterContinuation) name+=a.text;
    if (a.style.clusterContinuation!=b.style.clusterContinuation || a.text!=b.text || a.style.fontResource!=b.style.fontResource || a.style.glyphId!=b.style.glyphId ||
        std::abs(a.style.fontSize-b.style.fontSize)>1e-6 ||
        std::abs(a.x-b.x)>1e-5 || std::abs(a.y-b.y)>1e-5 ||
        std::abs(a.style.lineHeight-b.style.lineHeight)>1e-6 ||
        std::abs(a.style.outlineWidth-b.style.outlineWidth)>1e-6)
      throw std::runtime_error("Viewer/editor font, advance, baseline or outline metrics differ");
  }
  if (name!=(bidi ? "Lamp24 ابحرمRack12 םולשx́ ffiID: 2043.94" : "MAC QuantumProfileID: 2043.94")) throw std::runtime_error("Canonical sample lost label fields");
  Viewer2DPrintOptions options; options.compressStreams=false;
  if (!ExportViewer2DToPdf(captured,capturedState,options,scratch/(bidi ? "canonical-label-bidi.pdf" : "canonical-label-parity.pdf"),panel->GetBottomSymbolCacheSnapshot()).success)
    throw std::runtime_error("Canonical inherited label PDF export failed");
  if (cfg.GetValue("label_fixture_overrides")!=originalOverrides || cfg.GetSelectedFixtures()!=originalSelection)
    throw std::runtime_error("Inspecting inherited typography changed project overrides or selection");
  editor.Hide();
  std::cout << "Viewer/editor/PDF parity passed: " << fixture.instanceName << std::endl;
}
