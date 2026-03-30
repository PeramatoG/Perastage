#include "lighting_profile.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace Viewer3DLightingProfile {

namespace {
float Clamp01(float value) {
  if (value < 0.0f)
    return 0.0f;
  if (value > 1.0f)
    return 1.0f;
  return value;
}

float Lerp(float from, float to, float t) {
  return from + ((to - from) * t);
}
} // namespace

void ApplyEnhancedBasicLighting(const LightingOptions &options) {
  glEnable(GL_LIGHTING);
  // Keep normal lengths stable after model transforms with scaling.
  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  // Slightly lower ambient contribution and add multiple directional lights.
  // This improves depth cues for similar gray materials at low runtime cost.
  const float aoStrength =
      options.ambientOcclusionEnabled ? Clamp01(options.ambientOcclusionStrength)
                                      : 0.0f;
  const bool whiteModelStyle = options.whiteModelStyle;
  const float globalAmbientIntensity =
      whiteModelStyle ? Lerp(0.12f, 0.02f, aoStrength)
                      : Lerp(0.14f, 0.03f, aoStrength);
  const GLfloat globalAmbient[] = {globalAmbientIntensity,
                                   globalAmbientIntensity,
                                   globalAmbientIntensity, 1.0f};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

  const GLfloat keyAmbient[] = {whiteModelStyle ? 0.05f : 0.08f,
                                whiteModelStyle ? 0.05f : 0.08f,
                                whiteModelStyle ? 0.05f : 0.08f, 1.0f};
  const GLfloat keyDiffuse[] = {whiteModelStyle ? 0.90f : 0.78f,
                                whiteModelStyle ? 0.90f : 0.78f,
                                whiteModelStyle ? 0.90f : 0.78f, 1.0f};
  const GLfloat keySpecular[] = {whiteModelStyle ? 0.28f : 0.35f,
                                 whiteModelStyle ? 0.28f : 0.35f,
                                 whiteModelStyle ? 0.28f : 0.35f, 1.0f};
  const GLfloat keyPosition[] = {2.0f, -4.0f, 5.0f, 0.0f};

  glEnable(GL_LIGHT0);
  glLightfv(GL_LIGHT0, GL_AMBIENT, keyAmbient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, keyDiffuse);
  glLightfv(GL_LIGHT0, GL_SPECULAR, keySpecular);
  glLightfv(GL_LIGHT0, GL_POSITION, keyPosition);

  const GLfloat fillAmbient[] = {0.0f, 0.0f, 0.0f, 1.0f};
  const float fillDiffuseIntensity =
      whiteModelStyle ? Lerp(0.22f, 0.06f, aoStrength)
                      : Lerp(0.32f, 0.12f, aoStrength);
  const GLfloat fillDiffuse[] = {fillDiffuseIntensity, fillDiffuseIntensity,
                                 fillDiffuseIntensity + 0.04f, 1.0f};
  const GLfloat fillSpecular[] = {0.0f, 0.0f, 0.0f, 1.0f};
  const GLfloat fillPosition[] = {-1.5f, 2.0f, 1.0f, 0.0f};

  glEnable(GL_LIGHT1);
  glLightfv(GL_LIGHT1, GL_AMBIENT, fillAmbient);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDiffuse);
  glLightfv(GL_LIGHT1, GL_SPECULAR, fillSpecular);
  glLightfv(GL_LIGHT1, GL_POSITION, fillPosition);

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

  const GLfloat materialSpecular[] = {whiteModelStyle ? 0.10f : 0.18f,
                                      whiteModelStyle ? 0.10f : 0.18f,
                                      whiteModelStyle ? 0.10f : 0.18f, 1.0f};
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, whiteModelStyle ? 12.0f : 24.0f);

  glShadeModel(GL_SMOOTH);
}

} // namespace Viewer3DLightingProfile
