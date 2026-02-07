#include "matrixutils.h"

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

  return 0;
}
