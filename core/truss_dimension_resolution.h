#pragma once

#include "truss.h"

// Reports whether all three stored truss dimensions are finite and positive.
bool HasValidTrussDimensions(const Truss &truss);

// Resolves recoverable truss dimensions from measured geometry bounds.
bool ResolveTrussDimensionsFromGeometry(Truss &truss,
                                        bool legacyMetadataContext);
