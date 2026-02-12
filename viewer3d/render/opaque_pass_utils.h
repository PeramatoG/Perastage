#pragma once

#include "canvas2d.h"
#include "viewer3d_types.h"
#include "models/types.h"

#include <array>
#include <string>

std::string NormalizeModelKey(const std::string &p);
std::string ResolveCacheKey(const std::string &pathRef);

SymbolBounds ComputeSymbolBounds(const CommandBuffer &buffer);
void MatrixToArray(const Matrix &m, float out[16]);
std::array<float, 3> TransformPoint(const Matrix &m,
                                    const std::array<float, 3> &p);
Transform2D BuildInstanceTransform2D(const Matrix &m, Viewer2DView view);
