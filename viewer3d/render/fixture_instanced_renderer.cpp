#include "fixture_instanced_renderer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <array>
#include <string>

namespace {

struct InstanceGpuData {
  float model[16] = {0.0f};
  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

void MultiplyColumnMajor4x4(const double a[16], const double b[16],
                            float out[16]) {
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      out[col * 4 + row] = static_cast<float>(
          a[0 * 4 + row] * b[col * 4 + 0] +
          a[1 * 4 + row] * b[col * 4 + 1] +
          a[2 * 4 + row] * b[col * 4 + 2] +
          a[3 * 4 + row] * b[col * 4 + 3]);
    }
  }
}

bool CompileShader(GLuint shader, const char *source) {
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  return ok == GL_TRUE;
}

GLuint GetInstancingProgram() {
  static GLuint program = 0;
  static bool attempted = false;
  if (attempted)
    return program;
  attempted = true;

  if (!(GLEW_VERSION_3_3 ||
        (GLEW_ARB_instanced_arrays && GLEW_ARB_vertex_shader &&
         GLEW_ARB_fragment_shader))) {
    return 0;
  }

  const char *vs = R"(
    #version 330 core
    layout(location = 0) in vec3 aPosition;
    layout(location = 2) in vec4 aModelCol0;
    layout(location = 3) in vec4 aModelCol1;
    layout(location = 4) in vec4 aModelCol2;
    layout(location = 5) in vec4 aModelCol3;
    layout(location = 6) in vec4 aColor;

    uniform mat4 uViewProj;
    uniform float uMeshScale;

    out vec4 vColor;

    void main() {
      mat4 model = mat4(aModelCol0, aModelCol1, aModelCol2, aModelCol3);
      vec4 world = model * vec4(aPosition * uMeshScale, 1.0);
      gl_Position = uViewProj * world;
      vColor = aColor;
    }
  )";

  const char *fs = R"(
    #version 330 core
    in vec4 vColor;
    out vec4 fragColor;

    void main() {
      fragColor = vColor;
    }
  )";

  GLuint vert = glCreateShader(GL_VERTEX_SHADER);
  GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
  if (!CompileShader(vert, vs) || !CompileShader(frag, fs)) {
    glDeleteShader(vert);
    glDeleteShader(frag);
    return 0;
  }

  program = glCreateProgram();
  glAttachShader(program, vert);
  glAttachShader(program, frag);
  glLinkProgram(program);

  glDeleteShader(vert);
  glDeleteShader(frag);

  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    glDeleteProgram(program);
    program = 0;
  }

  return program;
}

} // namespace

bool RenderFixtureInstancedBatches(const FixtureInstancedBatches &batches) {
  if (batches.empty())
    return false;

  const GLuint program = GetInstancingProgram();
  if (!program)
    return false;

  std::array<double, 16> model = {};
  std::array<double, 16> projection = {};
  glGetDoublev(GL_MODELVIEW_MATRIX, model.data());
  glGetDoublev(GL_PROJECTION_MATRIX, projection.data());

  float viewProj[16] = {0.0f};
  MultiplyColumnMajor4x4(projection.data(), model.data(), viewProj);

  GLuint instanceVbo = 0;
  glGenBuffers(1, &instanceVbo);

  const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
  if (lightingWasEnabled)
    glDisable(GL_LIGHTING);

  GLint currentProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

  glUseProgram(program);
  glUniformMatrix4fv(glGetUniformLocation(program, "uViewProj"), 1, GL_FALSE,
                     viewProj);
  glUniform1f(glGetUniformLocation(program, "uMeshScale"), 0.001f);

  bool renderedAny = false;
  for (const auto &[mesh, instances] : batches) {
    if (!mesh || !mesh->buffersReady || mesh->triangleIndexCount <= 0 ||
        instances.empty()) {
      continue;
    }

    std::vector<InstanceGpuData> gpuInstances(instances.size());
    for (size_t i = 0; i < instances.size(); ++i) {
      for (int j = 0; j < 16; ++j)
        gpuInstances[i].model[j] = instances[i].modelMatrix[j];
      gpuInstances[i].color[0] = instances[i].color[0];
      gpuInstances[i].color[1] = instances[i].color[1];
      gpuInstances[i].color[2] = instances[i].color[2];
      gpuInstances[i].color[3] = 1.0f;
    }

    glBindVertexArray(mesh->vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vboVertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 gpuInstances.size() * sizeof(InstanceGpuData),
                 gpuInstances.data(), GL_STREAM_DRAW);

    constexpr GLsizei stride = sizeof(InstanceGpuData);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(0));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(sizeof(float) * 4));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(sizeof(float) * 8));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(sizeof(float) * 12));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(sizeof(float) * 16));

    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->eboTriangles);
    glDrawElementsInstanced(GL_TRIANGLES, mesh->triangleIndexCount,
                            GL_UNSIGNED_SHORT, nullptr,
                            static_cast<GLsizei>(instances.size()));

    glVertexAttribDivisor(2, 0);
    glVertexAttribDivisor(3, 0);
    glVertexAttribDivisor(4, 0);
    glVertexAttribDivisor(5, 0);
    glVertexAttribDivisor(6, 0);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glDisableVertexAttribArray(5);
    glDisableVertexAttribArray(6);
    glDisableVertexAttribArray(0);

    renderedAny = true;
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glUseProgram(static_cast<GLuint>(currentProgram));

  if (lightingWasEnabled)
    glEnable(GL_LIGHTING);

  glDeleteBuffers(1, &instanceVbo);
  return renderedAny;
}
