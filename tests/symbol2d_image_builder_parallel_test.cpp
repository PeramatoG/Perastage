#include "symbols/Symbol2DImageBuilder.h"

#include <cassert>
#include <cstddef>
#include <vector>

namespace {

// Creates a transparent RGBA test render for one symbol view.
symbols::RenderedSymbolImage MakeImage(symbols::SymbolView view, int width,
                                       int height) {
  symbols::RenderedSymbolImage image;
  image.view = view;
  image.width = width;
  image.height = height;
  image.rgba.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4,
                    0);
  return image;
}

// Sets one image pixel to an opaque black source sample.
void SetBlackPixel(symbols::RenderedSymbolImage &image, int x, int y) {
  const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
                         static_cast<size_t>(x)) *
                        4;
  image.rgba[offset + 0] = 0;
  image.rgba[offset + 1] = 0;
  image.rgba[offset + 2] = 0;
  image.rgba[offset + 3] = 255;
}

// Draws a filled black rectangle into the source image.
void FillRect(symbols::RenderedSymbolImage &image, int x0, int y0, int x1,
              int y1) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x)
      SetBlackPixel(image, x, y);
}

// Builds deterministic synthetic images that exercise independent conversion paths.
std::vector<symbols::RenderedSymbolImage> BuildInputs() {
  std::vector<symbols::RenderedSymbolImage> renders;
  renders.push_back(MakeImage(symbols::SymbolView::Front, 32, 32));
  FillRect(renders.back(), 6, 7, 24, 22);

  renders.push_back(MakeImage(symbols::SymbolView::Top, 32, 32));
  FillRect(renders.back(), 4, 4, 27, 27);
  FillRect(renders.back(), 11, 11, 20, 20);
  for (int y = 11; y <= 20; ++y)
    for (int x = 11; x <= 20; ++x)
      renders.back().rgba[(static_cast<size_t>(y) * 32 + static_cast<size_t>(x)) * 4 + 3] = 0;

  renders.push_back(MakeImage(symbols::SymbolView::Left, 32, 32));
  for (int i = 3; i < 29; ++i)
    SetBlackPixel(renders.back(), i, i);

  renders.push_back(MakeImage(symbols::SymbolView::Bottom, 32, 32));
  FillRect(renders.back(), 2, 2, 6, 6);
  FillRect(renders.back(), 23, 23, 29, 29);
  return renders;
}

// Compares all symbol fields through exact value equality.
void AssertEqualSymbols(const std::vector<symbols::Symbol2D> &expected,
                        const std::vector<symbols::Symbol2D> &actual) {
  assert(expected.size() == actual.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    assert(expected[i].view == actual[i].view);
    assert(expected[i].strokeWidthPx == actual[i].strokeWidthPx);
    assert(expected[i].bounds.valid == actual[i].bounds.valid);
    assert(expected[i].bounds.min.x == actual[i].bounds.min.x);
    assert(expected[i].bounds.min.y == actual[i].bounds.min.y);
    assert(expected[i].bounds.max.x == actual[i].bounds.max.x);
    assert(expected[i].bounds.max.y == actual[i].bounds.max.y);
    assert(expected[i].fill.size() == actual[i].fill.size());
    for (size_t polygon = 0; polygon < expected[i].fill.size(); ++polygon) {
      assert(expected[i].fill[polygon].outer.size() == actual[i].fill[polygon].outer.size());
      for (size_t point = 0; point < expected[i].fill[polygon].outer.size(); ++point) {
        assert(expected[i].fill[polygon].outer[point].x == actual[i].fill[polygon].outer[point].x);
        assert(expected[i].fill[polygon].outer[point].y == actual[i].fill[polygon].outer[point].y);
      }
      assert(expected[i].fill[polygon].holes.size() == actual[i].fill[polygon].holes.size());
      for (size_t hole = 0; hole < expected[i].fill[polygon].holes.size(); ++hole) {
        assert(expected[i].fill[polygon].holes[hole].size() == actual[i].fill[polygon].holes[hole].size());
        for (size_t point = 0; point < expected[i].fill[polygon].holes[hole].size(); ++point) {
          assert(expected[i].fill[polygon].holes[hole][point].x == actual[i].fill[polygon].holes[hole][point].x);
          assert(expected[i].fill[polygon].holes[hole][point].y == actual[i].fill[polygon].holes[hole][point].y);
        }
      }
    }
    assert(expected[i].strokes.size() == actual[i].strokes.size());
    for (size_t stroke = 0; stroke < expected[i].strokes.size(); ++stroke) {
      assert(expected[i].strokes[stroke].size() == actual[i].strokes[stroke].size());
      for (size_t point = 0; point < expected[i].strokes[stroke].size(); ++point) {
        assert(expected[i].strokes[stroke][point].x == actual[i].strokes[stroke][point].x);
        assert(expected[i].strokes[stroke][point].y == actual[i].strokes[stroke][point].y);
      }
    }
  }
}

} // namespace

// Verifies parallel image vectorization preserves sequential output and order exactly.
int main() {
  const auto renders = BuildInputs();
  const auto sequential =
      symbols::Symbol2DImageBuilder::BuildFromRenderedImagesSequential(renders);
  const auto parallel = symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
  AssertEqualSymbols(sequential, parallel);
  assert(parallel.size() == renders.size());
  assert(parallel[0].view == symbols::SymbolView::Front);
  assert(parallel[1].view == symbols::SymbolView::Top);
  assert(parallel[2].view == symbols::SymbolView::Left);
  assert(parallel[3].view == symbols::SymbolView::Bottom);
  return 0;
}
