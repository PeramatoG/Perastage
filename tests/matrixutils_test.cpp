#include "matrixutils.h"
#include "transform_space.h"

#include <cmath>
#include <iostream>

namespace {
bool Near(float a, float b, float eps = 1e-6f) { return std::fabs(a - b) <= eps; }
}

int main() {
  {
    Matrix m;
    const std::string text =
        "{0.035,0,8.53590478e-08}{0,0.035,0}{-8.53590478e-08,0,0.035}{1000,-2000,3000}";
    if (!MatrixUtils::ParseMatrix(text, m)) {
      std::cerr << "ParseMatrix rejected scientific notation input\n";
      return 1;
    }

    if (!Near(m.u[0], 0.035f) || !Near(m.u[2], 8.53590478e-08f) ||
        !Near(m.w[0], -8.53590478e-08f) || !Near(m.o[0], 1000.0f) ||
        !Near(m.o[2], 3000.0f)) {
      std::cerr << "ParseMatrix values mismatch\n";
      return 1;
    }
  }

  {
    Matrix parent;
    MatrixUtils::ParseMatrix("{1,0,0}{0,1,0}{0,0,1}{10,20,30}", parent);
    Matrix geo;
    MatrixUtils::ParseMatrix("{0.0254,0,0}{0,0.0254,0}{0,0,0.0254}{1,2,3}", geo);

    Matrix composed = MatrixUtils::Multiply(parent, geo);
    if (!Near(composed.u[0], 0.0254f) || !Near(composed.v[1], 0.0254f) ||
        !Near(composed.w[2], 0.0254f) || !Near(composed.o[0], 11.0f) ||
        !Near(composed.o[1], 22.0f) || !Near(composed.o[2], 33.0f)) {
      std::cerr << "Matrix multiply failed to preserve geometry scale\n";
      return 1;
    }
  }


  {
    Matrix source;
    MatrixUtils::ParseMatrix("{0.0254,0,0}{0,0.0508,0}{0,0,0.1016}{100,200,300}", source);

    Matrix rotation = MatrixUtils::EulerToMatrix(90.0f, 0.0f, 0.0f);
    Matrix updated = MatrixUtils::ApplyRotationPreservingScale(source, rotation,
                                                               {400.0f, 500.0f, 600.0f});

    auto scales = MatrixUtils::ExtractScale(updated);
    if (!Near(scales[0], 0.0254f) || !Near(scales[1], 0.0508f) ||
        !Near(scales[2], 0.1016f) || !Near(updated.o[0], 400.0f) ||
        !Near(updated.o[1], 500.0f) || !Near(updated.o[2], 600.0f)) {
      std::cerr << "ApplyRotationPreservingScale did not keep source scale\n";
      return 1;
    }
  }



  {
    Matrix rotated = MatrixUtils::EulerToMatrix(90.0f, 0.0f, 0.0f);
    Matrix worldMoved = transform_space::ApplyIncrementalTranslation(
        rotated, {1000.0f, 0.0f, 0.0f}, transform_space::TransformSpace::World);
    Matrix localMoved = transform_space::ApplyIncrementalTranslation(
        rotated, {1000.0f, 0.0f, 0.0f}, transform_space::TransformSpace::Local);
    if (!Near(worldMoved.o[0], 1000.0f) || !Near(worldMoved.o[1], 0.0f) ||
        !Near(localMoved.o[0], 0.0f, 1e-3f) || !Near(localMoved.o[1], -1000.0f, 1e-3f)) {
      std::cerr << "Transform-space translation mismatch\n";
      return 1;
    }
  }

  {
    Matrix source = MatrixUtils::EulerToMatrix(30.0f, 20.0f, 10.0f);
    source.o = {10.0f, 20.0f, 30.0f};
    for (int i = 0; i < 3; ++i) {
      source.u[i] *= 2.0f;
      source.v[i] *= 3.0f;
      source.w[i] *= 4.0f;
    }
    Matrix delta = MatrixUtils::EulerToMatrix(0.0f, 0.0f, 45.0f);
    Matrix worldRotated = transform_space::ApplyIncrementalRotation(
        source, delta, transform_space::TransformSpace::World);
    Matrix localRotated = transform_space::ApplyIncrementalRotation(
        source, delta, transform_space::TransformSpace::Local);
    auto worldScale = MatrixUtils::ExtractScale(worldRotated);
    auto localScale = MatrixUtils::ExtractScale(localRotated);
    if (Near(worldRotated.v[0], localRotated.v[0], 1e-4f) ||
        !Near(worldRotated.o[0], 10.0f) || !Near(worldRotated.o[1], 20.0f) ||
        !Near(worldScale[0], 2.0f, 1e-4f) || !Near(worldScale[1], 3.0f, 1e-4f) ||
        !Near(localScale[2], 4.0f, 1e-4f)) {
      std::cerr << "Transform-space rotation did not preserve expected state\n";
      return 1;
    }
  }

  {
    Matrix degenerate;
    degenerate.u = {0.0f, 0.0f, 0.0f};
    Matrix rotated = transform_space::ApplyIncrementalRotation(
        degenerate, MatrixUtils::EulerToMatrix(0.0f, 45.0f, 0.0f),
        transform_space::TransformSpace::Local);
    for (float value : {rotated.u[0], rotated.u[1], rotated.u[2], rotated.v[0],
                        rotated.v[1], rotated.v[2], rotated.w[0], rotated.w[1], rotated.w[2]}) {
      if (!std::isfinite(value)) {
        std::cerr << "Degenerate transform produced non-finite output\n";
        return 1;
      }
    }
  }

  return 0;
}
