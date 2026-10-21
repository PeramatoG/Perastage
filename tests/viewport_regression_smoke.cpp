// Native smoke against the built application objects; see run_viewport_regression_smoke.py.
#include <GL/glew.h>
#include <nanovg.h>
#define NANOVG_GL2
#include <nanovg_gl.h>
#include "nanovg_canvas_text.h"
#include "../viewer_common/gl_framebuffer_capture_target.h"
#include "../viewer3d/render/opengl_state_guard.h"
#include <wx/wx.h>
#include "fixture_label_preview.h"
#include <wx/apptrait.h>
#include <wx/stdpaths.h>
#include <wx/aui/aui.h>
#include "configmanager.h"
#include "fixture_label_editor.h"
#include "fixturetablepanel.h"
#include "mainwindow.h"
#include "mainwindow/ids/mainwindow_command_ids.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include "startup_profile.h"
#include <cmath>
#include <cstdlib>
#include <wx/dcscreen.h>
#include <wx/dcmemory.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
std::filesystem::path scratch;
constexpr const char *fixtureUuid = "11111111-1111-4111-8111-111111111111";
void Require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
class SmokePaths : public wxStandardPaths {
public:
  wxString GetUserDataDir() const override { return scratch.string(); }
};
class SmokeTraits : public wxGUIAppTraits {
  SmokePaths paths_;
public:
  wxStandardPaths &GetStandardPaths() override { return paths_; }
};

// Measure a straight contour through the real display adapter, avoiding glyph hinting.
void CheckScreenOutlineRaster() {
  viewer3d::render::OpenGLStateGuard state;
  GLfloat clear[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
  glcapture::FramebufferCaptureTarget target;
  Require(target.Initialize(512,512),"Outline probe framebuffer failed");
  std::unique_ptr<NVGcontext,decltype(&nvgDeleteGL2)> vg(
      nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES),nvgDeleteGL2);
  Require(vg!=nullptr,"Outline probe NanoVG context failed");
  auto shape=std::make_shared<typography::GlyphOutline>();
  using Kind=typography::GlyphPathElement::Kind;
  shape->path={{Kind::Move,0,0},{Kind::Line,1,0},{Kind::Line,1,1},
               {Kind::Line,0,1},{Kind::Close}};
  TextCommand text; text.style.fontSize=2; text.style.glyphOutline=shape;
  text.style.outlineColor={.25f,.5f,.75f,1};
  const float reference=typography::kFixtureLabelOutlineReferenceMetres;
  for(double dpi : {1.,1.5,2.}) for(double zoom : {.25,.5,1.,2.,4.}) {
    for(float radius : {reference,2*reference,0.f}) {
      text.style.outlineWidth=radius;
      target.BindForRendering(); glViewport(0,0,512,512);
      glDisable(GL_SCISSOR_TEST); glClearColor(0,0,0,1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
      nvgBeginFrame(vg.get(),512/dpi,512/dpi,dpi);
      viewer2d::DrawCanvasText(vg.get(),text,64/dpi,448/dpi,25*zoom,
                              typography::TextInkPass::Outline,dpi);
      nvgEndFrame(vg.get()); target.BindForReading();
      const int row=511-static_cast<int>(std::lround(448-25*zoom*dpi));
      std::array<unsigned char,17*4> pixels{};
      glReadPixels(56,row,17,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
      int width=0;
      for(size_t i=0;i<pixels.size();i+=4) if(pixels[i+2]>80) {
        ++width;
        Require(std::abs(2*static_cast<int>(pixels[i])-pixels[i+1])<=3 &&
                std::abs(3*static_cast<int>(pixels[i+1])-2*pixels[i+2])<=4,
                "Display outline changed its authored color");
      }
      const int expected=radius==0?0:radius==reference?2:4;
      Require(width==expected,"Rendered outline width changed with zoom or DPI");
    }
  }
  glClearColor(clear[0],clear[1],clear[2],clear[3]);
  std::cout<<"Outline raster probe passed: 5 zooms, 3 pixel ratios, default/custom/disabled"<<std::endl;
}
}
class ViewportSmoke : public wxApp {
  MainWindow *frame_ = nullptr;
  FixtureLabelEditor *editor_ = nullptr;
  wxTimer timer_;
  int stage_ = 0, ticks_ = 0, paints_ = 0, resumePaints_ = 0;
  int outcome_ = 1;
  float zoom_ = 0;
  std::string labels_, firstApply_, secondApply_;
  std::vector<unsigned char> selectedPixels_;
  std::vector<std::pair<float,float>> legacyPositions_;
  std::vector<std::pair<float,float>> LegacyPositions() {
    std::vector<std::pair<float,float>> positions;
    Viewer2DPanel::Instance()->CaptureFrameNow([&](CommandBuffer buffer,Viewer2DViewState){
      for(const auto &command:buffer.commands)
        if(const auto *text=std::get_if<TextCommand>(&command))
          if(!text->style.fontFamily.starts_with("perastage-sans") && text->text.starts_with("ID:"))
            positions.emplace_back(text->x,text->y);
    });
    return positions;
  }
  std::shared_ptr<startup::Metrics> metrics_ = std::make_shared<startup::Metrics>();
  void Menu(int id) {
    wxCommandEvent event(wxEVT_MENU, id);
    frame_->GetEventHandler()->ProcessEvent(event);
  }
  std::vector<unsigned char> Pixels() {
    std::vector<unsigned char> pixels; int width, height;
    Require(Viewer2DPanel::Instance()->RenderToRGBA(pixels, width, height, wxSize(600,600)),
            "2D framebuffer capture failed");
    return pixels;
  }
  void Bold() {
    for (wxWindow *child : editor_->GetChildren()) {
      auto *check = dynamic_cast<wxCheckBox *>(child);
      if (check && check->GetLabel() == "Bold") {
        check->SetValue(!check->GetValue());
        wxCommandEvent event(wxEVT_CHECKBOX, check->GetId());
        event.SetEventObject(check); check->GetEventHandler()->ProcessEvent(event);
        return;
      }
    }
    throw std::runtime_error("Editor Bold property missing");
  }
  void Select(const std::vector<std::string> &ids) {
    auto *table = FixtureTablePanel::Instance();
    table->SelectByUuid(ids, false);
    auto *control = table->GetTableCtrl();
    wxDataViewEvent event(wxEVT_DATAVIEW_SELECTION_CHANGED, control, control->GetSelection());
    event.SetEventObject(control); control->GetEventHandler()->ProcessEvent(event);
  }
  void Button(int id) {
    wxCommandEvent event(wxEVT_BUTTON,id);
    editor_->GetEventHandler()->ProcessEvent(event);
  }
  FixtureLabelPreview *Preview() {
    for(auto *child:editor_->GetChildren())
      if(auto *preview=dynamic_cast<FixtureLabelPreview *>(child))return preview;
    throw std::runtime_error("Label preview missing");
  }
  void Pointer(FixtureLabelPreview &preview,wxEventType type,wxPoint point) {
    wxMouseEvent event(type);event.SetPosition(point);event.m_leftDown=type!=wxEVT_LEFT_UP;
    preview.GetEventHandler()->ProcessEvent(event);
  }
  void Choice(const char *name,int selection) {
    auto *choice=dynamic_cast<wxChoice *>(editor_->FindWindowByName(name,editor_));
    Require(choice,"Named editor choice missing");choice->SetSelection(selection);
    wxCommandEvent event(wxEVT_CHOICE,choice->GetId());event.SetEventObject(choice);
    choice->GetEventHandler()->ProcessEvent(event);
  }
  void CheckPreviewEditing() {
    auto *preview=Preview();preview->Update();
    CheckScreenOutlineRaster(); preview->Refresh(false); preview->Update();
    auto &cfg=ConfigManager::Get();const auto before=cfg.CaptureDirtyState();
    const auto bounds=preview->GetComposition().fields[0]->bounds;
    const auto otherFields=preview->GetComposition().fields;
    Choice("fixtureLabelPreviewBackground",1);preview->Update();
    float clear[4];glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    Require(std::abs(clear[0]-.1f)<1e-5f,"Dark preview did not render");
    Require(preview->GetComposition().fields[0]->bounds==bounds,"Background changed text geometry");
    Choice("fixtureLabelPreviewBackground",0);preview->Update();glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    Require(clear[0]==1 && cfg.CaptureDirtyState().revision==before.revision,"Preview background mutated the project");
    const auto transform=preview->GetPreviewTransform();
    auto *initialX=dynamic_cast<wxSpinCtrlDouble *>(editor_->FindWindowByName("fixtureLabelOffsetX",editor_));
    const double originalX=initialX->GetValue();
    const auto centre=transform.ToScreen((bounds.minX+bounds.maxX)/2,(bounds.minY+bounds.maxY)/2);
    const wxPoint start(std::lround(centre.x),std::lround(centre.y));
    Pointer(*preview,wxEVT_LEFT_DOWN,start);Pointer(*preview,wxEVT_LEFT_UP,start);
    Button(wxID_APPLY);
    Require(cfg.CaptureDirtyState().revision==before.revision,"Click without movement created history");
    // The first unrelated edit must adopt the baseline already shown in preview.
    const auto originalText=preview->GetComposition().text;
    auto *outlineWidth=dynamic_cast<wxSpinCtrlDouble *>(editor_->FindWindowByName("fixtureLabelOutlineWidth",editor_));
    auto *offsetY=dynamic_cast<wxSpinCtrlDouble *>(editor_->FindWindowByName("fixtureLabelOffsetY",editor_));
    Require(outlineWidth && offsetY,"Editor presentation controls missing");
    const double originalWidth=outlineWidth->GetValue(),originalY=offsetY->GetValue();
    for (double width : {originalWidth+.001,originalWidth}) {
      outlineWidth->SetValue(width);
      wxCommandEvent edit(wxEVT_TEXT,outlineWidth->GetId());edit.SetEventObject(outlineWidth);
      outlineWidth->GetEventHandler()->ProcessEvent(edit);
      preview->Update();
      Require(preview->GetPreviewTransform().scale==transform.scale,"Outline edit changed preview framing");
      const auto &updated=preview->GetComposition().text;
      Require(updated.size()==originalText.size(),"Outline edit changed glyph count");
      for(size_t i=0;i<updated.size();++i)
        Require(updated[i].x==originalText[i].x && updated[i].y==originalText[i].y,
                "First outline edit moved a label baseline");
      Require(offsetY->GetValue()==originalY,"Outline edit changed the numeric offset");
    }
    Pointer(*preview,wxEVT_LEFT_DOWN,start);Pointer(*preview,wxEVT_MOTION,start+wxPoint(20,-10));
    Require(preview->IsDragging(),"Preview did not begin a label drag");
    wxKeyEvent escape(wxEVT_CHAR_HOOK); escape.m_keyCode=WXK_ESCAPE;
    preview->GetEventHandler()->ProcessEvent(escape);
    Require(editor_->IsShown(),"Escape closed the editor during a drag");
    Require(!preview->IsDragging() && !wxWindow::GetCapture(),"Cancel left mouse capture active");
    Require(preview->GetComposition().fields[0]->bounds==bounds,"Cancelled drag changed draft geometry");
    Pointer(*preview,wxEVT_LEFT_DOWN,start);Pointer(*preview,wxEVT_MOTION,start+wxPoint(20,-10));
    preview->Update();
    Require(preview->GetPreviewTransform().scale==transform.scale,"Drag changed the preview scale");
    const auto moved=preview->GetComposition().fields[0]->bounds;
    Require(std::abs(moved.minX-bounds.minX-20/transform.scale)<1e-5,"Drag X conversion failed");
    Require(std::abs(moved.minY-bounds.minY-10/transform.scale)<1e-5,"Drag Y conversion failed");
    Pointer(*preview,wxEVT_LEFT_UP,start+wxPoint(20,-10));
    for(int field=1;field<3;++field)
      if(otherFields[field])Require(preview->GetComposition().fields[field]->bounds==otherFields[field]->bounds,
                                  "Dragging one field reflowed another legacy field");
    auto *x=dynamic_cast<wxSpinCtrlDouble *>(editor_->FindWindowByName("fixtureLabelOffsetX",editor_));
    auto *y=dynamic_cast<wxSpinCtrlDouble *>(editor_->FindWindowByName("fixtureLabelOffsetY",editor_));
    Require(x && y,"Offset controls missing");
    Require(std::abs(x->GetValue()-originalX-20/transform.scale)<.0006,"Drag did not synchronize numeric X");
    const double committedX=x->GetValue(),committedY=y->GetValue();
    Pointer(*preview,wxEVT_LEFT_DOWN,start+wxPoint(20,-10));
    Pointer(*preview,wxEVT_MOTION,start+wxPoint(30,-20));
    preview->ReleaseMouse();wxMouseCaptureLostEvent loss(preview->GetId());
    preview->GetEventHandler()->ProcessEvent(loss);
    Require(!preview->IsDragging() && !wxWindow::GetCapture(),"Capture loss left an active drag");
    Require(x->GetValue()==committedX && y->GetValue()==committedY,"Capture loss retained draft movement");
    Require(!wxWindow::GetCapture() && cfg.CaptureDirtyState().revision==before.revision,"Drag committed before Apply");
    Choice("fixtureLabelView",1);Choice("fixtureLabelView",2);Choice("fixtureLabelView",0);
    Require(preview->GetComposition().fields[0]->bounds==moved,"View change lost the dragged draft offset");
    Bold();
  }
  void ApplyAndObservePaint() {
    struct Probe { bool called=false; CommandBuffer commands; };
    const auto probe=std::make_shared<Probe>();
    auto *panel=Viewer2DPanel::Instance();
    const auto before=panel->GetViewState();
    panel->CaptureFrameAsync([probe](CommandBuffer commands,Viewer2DViewState){probe->called=true;probe->commands=std::move(commands);});
    Button(wxID_APPLY);
    Require(probe->called,"Apply did not render while its modal owner was disabled");
    const auto *width=dynamic_cast<wxSpinCtrlDouble *>(editor_->FindWindowByName("fixtureLabelOutlineWidth",editor_));
    Require(width,"Outline width control missing");
    bool outline=false;
    for(const auto &command:probe->commands.commands)
      if(const auto *text=std::get_if<TextCommand>(&command))
        outline|=text->style.fontFamily.starts_with("perastage-sans") &&
            text->style.glyphOutline && std::abs(text->style.outlineWidth-width->GetValue())<1e-6;
    Require(outline,"Natural viewport paint omitted the configured text outline");
    const auto after=panel->GetViewState();
    Require(before.zoom==after.zoom && before.offsetPixelsX==after.offsetPixelsX &&
            before.offsetPixelsY==after.offsetPixelsY && before.view==after.view,"Apply changed the camera");
  }
  void CloseEditor() {
    if(editor_->IsModal())editor_->EndModal(wxID_CANCEL);else editor_->Hide();
    editor_->Destroy();editor_=nullptr;
  }
  void SnapshotEditor(bool wide=false) {
    const char *path=std::getenv("PERASTAGE_SMOKE_SCREENSHOT");
    if(!path)return;
    Preview()->Update();
    const auto size=editor_->GetSize();
    const auto position=editor_->GetScreenPosition();
    wxBitmap bitmap(size);wxMemoryDC target(bitmap);wxScreenDC screen;
    target.Blit(0,0,size.x,size.y,&screen,position.x,position.y);
    target.SelectObject(wxNullBitmap);
    std::string file=std::string(path)+(wide?".wide.png":"");
    if(std::filesystem::exists(file))file+=".reload.png";
    Require(bitmap.SaveFile(wxString::FromUTF8(file),wxBITMAP_TYPE_PNG),"Editor screenshot failed");
  }
public:
  wxAppTraits *CreateTraits() override { return new SmokeTraits; }
  bool OnInit() override {
    scratch = std::filesystem::absolute(argv[1].ToStdString());
    std::filesystem::create_directories(scratch);
    SetAppName("Perastage"); wxInitAllImageHandlers();
    auto &cfg = ConfigManager::Get();
    if (argc < 3) {
      cfg.SetValue("layout_view_mode", "2d_layout_view");
      cfg.SetFloat("view2d_zoom", 8);
      Fixture fixture; fixture.uuid = fixtureUuid; fixture.instanceName = "LITE BEYE 10R4";
      fixture.fixtureId = 1; fixture.address = "1.001"; fixture.typeName = "Smoke";
      cfg.GetScene().fixtures.emplace(fixture.uuid, fixture);
      Require(cfg.SaveProject((scratch/"scene.pstg").string()), "Synthetic PSTG save failed");
    }
    frame_ = new MainWindow("2D viewport regression smoke", nullptr, metrics_);
    SetTopWindow(frame_);
    frame_->LoadStartupProjectFromPath((scratch/"scene.pstg").string());
    timer_.SetOwner(this); Bind(wxEVT_TIMER, &ViewportSmoke::Tick, this); timer_.Start(100);
    return true;
  }
  int OnRun() override { wxApp::OnRun(); return outcome_; }
  void Tick(wxTimerEvent &) {
    try {
      Require(++ticks_ < 600, "GUI smoke timed out");
      auto &cfg = ConfigManager::Get();
      auto *panel = Viewer2DPanel::Instance();
      if (frame_->IsStartupInitializationPending() || !panel || panel->HasPendingResourceLoading()) return;
      auto *manager = wxAuiManager::GetManager(frame_);
      Require(manager && manager->GetPane("2DViewport").IsShown() && panel->IsShownOnScreen(),
              "Actual 2D viewport is not visible");
      if(!editor_ || !editor_->IsModal())
        Require(panel->IsEnabled() && frame_->IsEnabled(), "Viewport remains disabled after loading");
      else Require(panel->IsThisEnabled(),"Modal editor disabled viewport presentation locally");
      if(stage_==5 && (!Preview()->IsShownOnScreen() || Preview()->GetPreviewTransform().scale<=0))return;
      std::cout << "Viewport smoke stage " << stage_ << std::endl;
      switch (stage_++) {
      case 0: {
        Require(metrics_->viewer2DConstructions == 1 && metrics_->viewer3DConstructions == 0,
                "Saved 2D startup broke lazy viewport creation");
        panel->Bind(wxEVT_PAINT, [this](wxPaintEvent &e) { ++paints_; e.Skip(); });
        zoom_ = panel->GetViewState().zoom;
        wxMouseEvent wheel(wxEVT_MOUSEWHEEL); wheel.m_wheelRotation = 120; wheel.m_wheelDelta = 120;
        panel->GetEventHandler()->ProcessEvent(wheel);
        Require(panel->GetViewState().zoom > zoom_, "Wheel zoom failed immediately after startup");
        break;
      }
      case 1:
        // Consume a queued paint while project-loading interaction is locked.
        panel->Disable(); panel->SetView(panel->GetView()); panel->Update();
        resumePaints_ = paints_; panel->Enable(); break;
      case 2:
        Require(paints_ > resumePaints_, "Re-enabling did not present the consumed load-time paint");
        Require(frame_->LoadProjectFromPath((scratch/"scene.pstg").string(), true), "Open while in 2D failed");
        break;
      case 3: {
        wxMouseEvent wheel(wxEVT_MOUSEWHEEL); wheel.m_wheelRotation = 120; wheel.m_wheelDelta = 120;
        const float before = panel->GetViewState().zoom;
        panel->GetEventHandler()->ProcessEvent(wheel);
        Require(panel->GetViewState().zoom > before, "Wheel zoom failed after project open");
        Select({fixtureUuid});
        Require(cfg.GetSelectedFixtures() == std::vector<std::string>{fixtureUuid}, "Table selection failed");
        selectedPixels_ = Pixels();
        cfg.SetSelectedFixtures({}); panel->UpdateScene(false);
        Require(Pixels() != selectedPixels_, "Viewport does not display selection outlines");
        Select({fixtureUuid});
        Require(Pixels() == selectedPixels_, "Table re-selection after scene/history synchronization is stale");
        Select({});
        const wxSize size = panel->GetClientSize();
        wxMouseEvent down(wxEVT_LEFT_DOWN); down.SetPosition({size.x/2,size.y/2}); down.m_leftDown = true;
        panel->GetEventHandler()->ProcessEvent(down);
        wxMouseEvent up(wxEVT_LEFT_UP); up.SetPosition(down.GetPosition());
        panel->GetEventHandler()->ProcessEvent(up);
        Require(!cfg.GetSelectedFixtures().empty(), "Picking a fixture from 2D failed");
        break;
      }
      case 4:
        Menu(ID_View_Layout_Default); Menu(ID_View_Layout_2D);
        Menu(ID_View_Layout_Default); Menu(ID_View_Layout_2D);
        Require(Viewer3DPanel::Instance() && !Viewer3DPanel::Instance()->IsShownOnScreen(), "Mode switching failed");
        editor_ = new FixtureLabelEditor(frame_, cfg, 0);
        CallAfter([dialog=editor_]{dialog->ShowModal();}); break;
      case 5: {
        labels_ = cfg.GetValue("label_fixture_overrides").value_or("");
        legacyPositions_=LegacyPositions();
        Require(!legacyPositions_.empty(),"Synthetic legacy ID label missing");
        CheckPreviewEditing();SnapshotEditor();ApplyAndObservePaint();
        firstApply_=cfg.GetValue("label_fixture_overrides").value_or("");
        Require(firstApply_!=labels_,"First Apply did not commit");
        Require(LegacyPositions()==legacyPositions_,"Editing Name moved the legacy ID baseline");
        auto *width=dynamic_cast<wxSpinCtrlDouble *>(editor_->FindWindowByName("fixtureLabelOutlineWidth",editor_));
        Require(width,"Outline width control missing");
        width->SetValue(std::abs(width->GetValue()-.040)<1e-5?.031:.040);
        wxCommandEvent edit(wxEVT_TEXT,width->GetId());edit.SetEventObject(width);width->GetEventHandler()->ProcessEvent(edit);
        ApplyAndObservePaint();SnapshotEditor(true);secondApply_=cfg.GetValue("label_fixture_overrides").value_or("");
        Require(secondApply_!=firstApply_,"Second Apply did not commit");
        const auto revision=cfg.CaptureDirtyState().revision;Button(wxID_APPLY);
        Require(cfg.CaptureDirtyState().revision==revision,"No-op Apply created history");
        CloseEditor();break;
      }
      case 6: {
        CommandBuffer commands;
        panel->CaptureFrameNow([&](CommandBuffer buffer, Viewer2DViewState) { commands = std::move(buffer); });
        bool composed = false;
        for (const auto &command : commands.commands)
          if (const auto *text = std::get_if<TextCommand>(&command))
            composed |= text->style.fontFamily.starts_with("perastage-sans");
        Require(composed, "Edited labels are absent from real viewport capture");
        const auto applied = cfg.GetValue("label_fixture_overrides");
        Menu(ID_Edit_Undo); Require(cfg.GetValue("label_fixture_overrides").value_or("")==firstApply_,"Undo second Apply failed");
        Menu(ID_Edit_Undo); Require(cfg.GetValue("label_fixture_overrides").value_or("")==labels_,"Undo first Apply failed");
        Menu(ID_Edit_Redo); Require(cfg.GetValue("label_fixture_overrides").value_or("")==firstApply_,"Redo first Apply failed");
        Menu(ID_Edit_Redo); Require(cfg.GetValue("label_fixture_overrides")==applied,"Redo second Apply failed");
        panel->SetSelectedUuids({fixtureUuid}); Pixels();
        labels_ = applied.value_or("");
        editor_ = new FixtureLabelEditor(frame_, cfg, 0); editor_->Show(); break;
      }
      case 7:
        Bold(); Button(wxID_RESET); Button(wxID_CANCEL); CloseEditor();
        Require(cfg.GetValue("label_fixture_overrides").value_or("") == labels_, "Closing committed an unapplied draft");
        cfg.SetValue("layout_perspective", manager->SavePerspective().ToStdString());
        Require(cfg.SaveProject((scratch/"scene.pstg").string()), "Save with a 2D perspective failed");
        Require(frame_->LoadProjectFromPath((scratch/"scene.pstg").string(), false), "Reload with perspective failed");
        break;
      case 8:
        editor_=new FixtureLabelEditor(frame_,cfg,0);editor_->Show();break;
      case 9:
        labels_=cfg.GetValue("label_fixture_overrides").value_or("");Bold();Button(wxID_OK);
        Require(!editor_->IsShown() && cfg.GetValue("label_fixture_overrides").value_or("")!=labels_,"OK did not apply and close");
        CloseEditor();break;
      case 10:
        Require(Pixels().size() == 600*600*4, "Viewport capture failed after reload");
        outcome_ = 0; timer_.Stop(); frame_->Destroy(); ExitMainLoop();
        std::cout << "Viewport regression smoke passed" << std::endl; break;
      }
    } catch (const std::exception &error) {
      std::cerr << "Viewport regression smoke FAILED: " << error.what() << std::endl;
      timer_.Stop(); if (editor_) CloseEditor(); frame_->Destroy(); ExitMainLoop();
    }
  }
};
wxIMPLEMENT_APP(ViewportSmoke);
