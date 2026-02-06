/*
 * This file is part of Perastage.
 */
#include <cassert>

#include "../mvr/mvrexporter.h"

int main() {
  assert(ComputeAbsoluteDmx(1, 1) == 1);
  assert(ComputeAbsoluteDmx(3, 1) == 1025);
  assert(ComputeAbsoluteDmx(3, 7) == 1031);
  assert(ComputeAbsoluteDmx(6, 121) == 2681);
  return 0;
}
