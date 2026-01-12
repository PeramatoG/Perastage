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
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <wx/wx.h>
#include "fixturepreviewpanel.h"
#include <cfloat>
#include <array>
#include <algorithm>
#include <cmath>

static constexpr float RENDER_SCALE = 0.001f;

// Helper to transform a point with our Matrix structure
static std::array<float,3> TransformPoint(const Matrix& m, const std::array<float,3>& p)
{
    return {
        m.u[0]*p[0] + m.v[0]*p[1] + m.w[0]*p[2] + m.o[0],
        m.u[1]*p[0] + m.v[1]*p[1] + m.w[1]*p[2] + m.o[1],
        m.u[2]*p[0] + m.v[2]*p[1] + m.w[2]*p[2] + m.o[2]
    };
}

static void MatrixToArray(const Matrix& m, float out[16])
{
    out[0] = m.u[0];  out[1] = m.u[1];  out[2] = m.u[2];  out[3] = 0.0f;
    out[4] = m.v[0];  out[5] = m.v[1];  out[6] = m.v[2];  out[7] = 0.0f;
    out[8] = m.w[0];  out[9] = m.w[1];  out[10] = m.w[2]; out[11] = 0.0f;
    out[12] = m.o[0]; out[13] = m.o[1]; out[14] = m.o[2]; out[15] = 1.0f;
}

// Simple cube rendering when no model is available
static void DrawCube(float size)
{
    float h = size * 0.5f;
    glBegin(GL_QUADS);
    // Front
    glVertex3f(-h,-h, h); glVertex3f( h,-h, h); glVertex3f( h, h, h); glVertex3f(-h, h, h);
    // Back
    glVertex3f(-h,-h,-h); glVertex3f(-h, h,-h); glVertex3f( h, h,-h); glVertex3f( h,-h,-h);
    // Left
    glVertex3f(-h,-h,-h); glVertex3f(-h,-h, h); glVertex3f(-h, h, h); glVertex3f(-h, h,-h);
    // Right
    glVertex3f( h,-h,-h); glVertex3f( h, h,-h); glVertex3f( h, h, h); glVertex3f( h,-h, h);
    // Top
    glVertex3f(-h, h, h); glVertex3f( h, h, h); glVertex3f( h, h,-h); glVertex3f(-h, h,-h);
    // Bottom
    glVertex3f(-h,-h, h); glVertex3f(-h,-h,-h); glVertex3f( h,-h,-h); glVertex3f( h,-h, h);
    glEnd();
}

static void DrawMesh(const Mesh& mesh, float scale)
{
    glBegin(GL_TRIANGLES);
    bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        unsigned short i0 = mesh.indices[i];
        unsigned short i1 = mesh.indices[i+1];
        unsigned short i2 = mesh.indices[i+2];
        float v0x = mesh.vertices[i0*3] * scale;
        float v0y = mesh.vertices[i0*3+1] * scale;
        float v0z = mesh.vertices[i0*3+2] * scale;
        float v1x = mesh.vertices[i1*3] * scale;
        float v1y = mesh.vertices[i1*3+1] * scale;
        float v1z = mesh.vertices[i1*3+2] * scale;
        float v2x = mesh.vertices[i2*3] * scale;
        float v2y = mesh.vertices[i2*3+1] * scale;
        float v2z = mesh.vertices[i2*3+2] * scale;
        if(hasNormals){
            glNormal3f(mesh.normals[i0*3], mesh.normals[i0*3+1], mesh.normals[i0*3+2]);
            glVertex3f(v0x,v0y,v0z);
            glNormal3f(mesh.normals[i1*3], mesh.normals[i1*3+1], mesh.normals[i1*3+2]);
            glVertex3f(v1x,v1y,v1z);
            glNormal3f(mesh.normals[i2*3], mesh.normals[i2*3+1], mesh.normals[i2*3+2]);
            glVertex3f(v2x,v2y,v2z);
        }else{
            float ux=v1x-v0x, uy=v1y-v0y, uz=v1z-v0z;
            float vx=v2x-v0x, vy=v2y-v0y, vz=v2z-v0z;
            float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
            float len = std::sqrt(nx*nx+ny*ny+nz*nz);
            if(len>0){ nx/=len; ny/=len; nz/=len; }
            glNormal3f(nx,ny,nz);
            glVertex3f(v0x,v0y,v0z);
            glVertex3f(v1x,v1y,v1z);
            glVertex3f(v2x,v2y,v2z);
        }
    }
    glEnd();

    // draw edges in a slightly darker color
    glDisable(GL_LIGHTING);
    glColor3f(0.3f,0.3f,0.3f);
    glBegin(GL_LINES);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        unsigned short i0 = mesh.indices[i];
        unsigned short i1 = mesh.indices[i+1];
        unsigned short i2 = mesh.indices[i+2];
        float v0x = mesh.vertices[i0*3] * scale;
        float v0y = mesh.vertices[i0*3+1] * scale;
        float v0z = mesh.vertices[i0*3+2] * scale;
        float v1x = mesh.vertices[i1*3] * scale;
        float v1y = mesh.vertices[i1*3+1] * scale;
        float v1z = mesh.vertices[i1*3+2] * scale;
        float v2x = mesh.vertices[i2*3] * scale;
        float v2y = mesh.vertices[i2*3+1] * scale;
        float v2z = mesh.vertices[i2*3+2] * scale;

        glVertex3f(v0x, v0y, v0z); glVertex3f(v1x, v1y, v1z);
        glVertex3f(v1x, v1y, v1z); glVertex3f(v2x, v2y, v2z);
        glVertex3f(v2x, v2y, v2z); glVertex3f(v0x, v0y, v0z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
    glColor3f(1.0f,1.0f,1.0f);
}

wxBEGIN_EVENT_TABLE(FixturePreviewPanel, wxGLCanvas)
    EVT_PAINT(FixturePreviewPanel::OnPaint)
    EVT_SIZE(FixturePreviewPanel::OnResize)
    EVT_LEFT_DOWN(FixturePreviewPanel::OnMouseDown)
    EVT_LEFT_UP(FixturePreviewPanel::OnMouseUp)
    EVT_MOTION(FixturePreviewPanel::OnMouseMove)
    EVT_MOUSEWHEEL(FixturePreviewPanel::OnMouseWheel)
    EVT_MOUSE_CAPTURE_LOST(FixturePreviewPanel::OnCaptureLost)
wxEND_EVENT_TABLE()

FixturePreviewPanel::FixturePreviewPanel(wxWindow* parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition, wxSize(200,200), wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    m_glContext = new wxGLContext(this);
    // default bounding box for cube
    m_bbMin[0]=m_bbMin[1]=m_bbMin[2]=-0.1f;
    m_bbMax[0]=m_bbMax[1]=m_bbMax[2]=0.1f;
    m_camera.SetOrientation(45.f,30.f);
    m_camera.SetDistance(0.6f);
}

FixturePreviewPanel::~FixturePreviewPanel()
{
    delete m_glContext;
}

void FixturePreviewPanel::InitGL()
{
    if(!IsShownOnScreen()){
        return;
    }
    SetCurrent(*m_glContext);
    if(!m_glInitialized){
        glewExperimental = GL_TRUE;
        glewInit();
        m_glInitialized = true;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glClearColor(0.08f,0.08f,0.08f,1.0f);
}

void FixturePreviewPanel::LoadFixture(const std::string& gdtfPath)
{
    m_objects.clear();
    m_hasModel = false;
    if(!gdtfPath.empty()){
        m_hasModel = LoadGdtf(gdtfPath, m_objects);
    }
    if(m_hasModel){
        m_bbMin[0]=m_bbMin[1]=m_bbMin[2]=FLT_MAX;
        m_bbMax[0]=m_bbMax[1]=m_bbMax[2]=-FLT_MAX;
        for(const auto& obj : m_objects){
            for(size_t vi=0; vi+2<obj.mesh.vertices.size(); vi+=3){
                std::array<float,3> p = {
                    obj.mesh.vertices[vi]*RENDER_SCALE,
                    obj.mesh.vertices[vi+1]*RENDER_SCALE,
                    obj.mesh.vertices[vi+2]*RENDER_SCALE
                };
                p = TransformPoint(obj.transform, p);
                for(int j=0;j<3;++j){
                    m_bbMin[j] = std::min(m_bbMin[j], p[j]);
                    m_bbMax[j] = std::max(m_bbMax[j], p[j]);
                }
            }
        }
    } else {
        m_bbMin[0]=m_bbMin[1]=m_bbMin[2]=-0.1f;
        m_bbMax[0]=m_bbMax[1]=m_bbMax[2]=0.1f;
    }
    float cx = (m_bbMin[0]+m_bbMax[0])*0.5f;
    float cy = (m_bbMin[1]+m_bbMax[1])*0.5f;
    float cz = (m_bbMin[2]+m_bbMax[2])*0.5f;
    m_camera.SetTarget(cx,cy,cz);
    float sizeX = m_bbMax[0]-m_bbMin[0];
    float sizeY = m_bbMax[1]-m_bbMin[1];
    float sizeZ = m_bbMax[2]-m_bbMin[2];
    float radius = std::max({sizeX,sizeY,sizeZ})*0.5f;
    if(radius < 0.1f) radius = 0.1f;
    m_camera.SetDistance(radius*3.0f);
    Refresh();
}

void FixturePreviewPanel::Render()
{
    int w,h; GetClientSize(&w,&h);
    glViewport(0,0,w,h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0,(double)w/h,0.1,100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    m_camera.Apply();
    float lightPos[4] = {0.f, 0.f, 1.f, 0.f};
    float lightDiffuse[4] = {1.f,1.f,1.f,1.f};
    float lightAmbient[4] = {0.2f,0.2f,0.2f,1.f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glColor3f(1.0f,1.0f,1.0f);
    if(m_hasModel){
        for(const auto& obj : m_objects){
            glPushMatrix();
            float m[16];
            MatrixToArray(obj.transform,m);
            glMultMatrixf(m);
            DrawMesh(obj.mesh, RENDER_SCALE);
            glPopMatrix();
        }
    } else {
        DrawCube(0.2f);
    }
    glFlush();
}

void FixturePreviewPanel::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(this);
    if(!IsShownOnScreen()){
        return;
    }
    InitGL();
    Render();
    SwapBuffers();
}

void FixturePreviewPanel::OnResize(wxSizeEvent&)
{
    Refresh();
}

void FixturePreviewPanel::OnMouseDown(wxMouseEvent& evt)
{
    m_dragging = true;
    m_lastMousePos = evt.GetPosition();
    CaptureMouse();
}

void FixturePreviewPanel::OnMouseUp(wxMouseEvent&)
{
    if(m_dragging){
        m_dragging = false;
        if(HasCapture()) ReleaseMouse();
    }
}

void FixturePreviewPanel::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(evt))
{
    m_dragging = false;
}

void FixturePreviewPanel::OnMouseMove(wxMouseEvent& evt)
{
    if(m_dragging){
        wxPoint pos = evt.GetPosition();
        int dx = pos.x - m_lastMousePos.x;
        int dy = pos.y - m_lastMousePos.y;
        m_camera.Orbit(dx * 0.5f, dy * 0.5f);
        m_lastMousePos = pos;
        Refresh();
    }
}

void FixturePreviewPanel::OnMouseWheel(wxMouseEvent& evt)
{
    int rot = evt.GetWheelRotation();
    int delta = evt.GetWheelDelta();
    m_camera.Zoom(-(float)rot/delta);
    Refresh();
}
