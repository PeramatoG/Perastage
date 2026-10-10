#pragma once
#include "mesh.h"
#include "model_detail_policy.h"
namespace viewer3d::resources {
Mesh BuildRuntimeRepresentation(const Mesh &source,
                                model_detail::SimplificationPolicy policy);
}
