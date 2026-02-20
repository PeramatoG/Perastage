#include "symbols/Skeletonize.h"

#include <array>

namespace symbols {

BinaryMask SkeletonizeMask(const BinaryMask &mask, int width, int height) {
  BinaryMask out = mask;
  bool changed = true;

  auto idx = [width](int x, int y) { return y * width + x; };
  auto val = [&](int x, int y) { return out[idx(x, y)] != 0; };

  while (changed) {
    changed = false;
    for (int sub = 0; sub < 2; ++sub) {
      BinaryMask toRemove(out.size(), 0);
      for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
          if (!val(x, y))
            continue;
          const int p2 = val(x, y - 1);
          const int p3 = val(x + 1, y - 1);
          const int p4 = val(x + 1, y);
          const int p5 = val(x + 1, y + 1);
          const int p6 = val(x, y + 1);
          const int p7 = val(x - 1, y + 1);
          const int p8 = val(x - 1, y);
          const int p9 = val(x - 1, y - 1);
          const int b = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
          if (b < 2 || b > 6)
            continue;
          const int a = (!p2 && p3) + (!p3 && p4) + (!p4 && p5) + (!p5 && p6) +
                        (!p6 && p7) + (!p7 && p8) + (!p8 && p9) + (!p9 && p2);
          if (a != 1)
            continue;
          if (sub == 0) {
            if (p2 * p4 * p6 != 0)
              continue;
            if (p4 * p6 * p8 != 0)
              continue;
          } else {
            if (p2 * p4 * p8 != 0)
              continue;
            if (p2 * p6 * p8 != 0)
              continue;
          }
          toRemove[idx(x, y)] = 1;
        }
      }
      for (size_t i = 0; i < out.size(); ++i) {
        if (toRemove[i]) {
          out[i] = 0;
          changed = true;
        }
      }
    }
  }

  return out;
}

} // namespace symbols
