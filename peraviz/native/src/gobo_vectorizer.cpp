#include "gobo_vectorizer.h"

#include <godot_cpp/classes/bit_map.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kOuterBorderPixels = 1;
constexpr float kApertureBorderRatio = 0.985F;

void prepare_binary_mask_image(const godot::Ref<godot::Image> &image,
                               float luma_alpha_threshold,
                               bool apply_edge_mask_correction) {
    if (image.is_null()) {
        return;
    }

    const int width = image->get_width();
    const int height = image->get_height();
    if (width <= 0 || height <= 0) {
        return;
    }

    const godot::Vector2 center((static_cast<float>(width) - 1.0F) * 0.5F,
                                (static_cast<float>(height) - 1.0F) * 0.5F);
    const float aperture_radius =
        (static_cast<float>(std::min(width, height)) * 0.5F) * kApertureBorderRatio;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (apply_edge_mask_correction) {
                if (x < kOuterBorderPixels || y < kOuterBorderPixels ||
                    x >= (width - kOuterBorderPixels) ||
                    y >= (height - kOuterBorderPixels)) {
                    image->set_pixel(x, y, godot::Color(0.0, 0.0, 0.0, 0.0));
                    continue;
                }
                const godot::Vector2 pos(static_cast<float>(x), static_cast<float>(y));
                if (pos.distance_to(center) >= aperture_radius) {
                    image->set_pixel(x, y, godot::Color(0.0, 0.0, 0.0, 0.0));
                    continue;
                }
            }

            const godot::Color sample = image->get_pixel(x, y);
            const float luma = sample.r * 0.299F + sample.g * 0.587F + sample.b * 0.114F;
            const float binary = (luma * sample.a) >= luma_alpha_threshold ? 1.0F : 0.0F;
            image->set_pixel(x, y, godot::Color(binary, binary, binary, binary));
        }
    }
}

} // namespace

namespace godot {

void PeravizGoboVectorizer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("vectorize_image", "image_path", "max_size",
                                   "luma_alpha_threshold", "apply_edge_mask_correction"),
                         &PeravizGoboVectorizer::vectorize_image,
                         DEFVAL(192), DEFVAL(0.5), DEFVAL(true));
}

Dictionary PeravizGoboVectorizer::vectorize_image(const String &image_path,
                                                  int max_size,
                                                  float luma_alpha_threshold,
                                                  bool apply_edge_mask_correction) const {
    Dictionary result;
    result["ok"] = false;
    result["width"] = 0;
    result["height"] = 0;
    result["polygons"] = Array();

    if (image_path.is_empty()) {
        result["error"] = String("image_path is empty");
        return result;
    }

    Ref<Image> image;
    image.instantiate();
    const Error load_error = image->load(image_path);
    if (load_error != OK) {
        result["error"] = String("failed to load gobo image");
        return result;
    }

    if (image->get_format() != Image::FORMAT_RGBA8) {
        image->convert(Image::FORMAT_RGBA8);
    }

    const int longest_size = std::max(image->get_width(), image->get_height());
    const int safe_max_size = std::max(max_size, 8);
    if (longest_size > safe_max_size) {
        const int target_width =
            std::max(8, static_cast<int>(Math::round(static_cast<float>(image->get_width()) *
                                                static_cast<float>(safe_max_size) /
                                                static_cast<float>(longest_size))));
        const int target_height =
            std::max(8, static_cast<int>(Math::round(static_cast<float>(image->get_height()) *
                                                static_cast<float>(safe_max_size) /
                                                static_cast<float>(longest_size))));
        image->resize(target_width, target_height, Image::INTERPOLATE_BILINEAR);
    }

    prepare_binary_mask_image(image, luma_alpha_threshold, apply_edge_mask_correction);

    Ref<BitMap> bitmap;
    bitmap.instantiate();
    bitmap->create_from_image_alpha(image, 0.5);

    const Rect2i rect(0, 0, image->get_width(), image->get_height());
    Array polygons = bitmap->opaque_to_polygons(rect, 1.6);

    result["ok"] = true;
    result["width"] = image->get_width();
    result["height"] = image->get_height();
    result["polygons"] = polygons;
    return result;
}

} // namespace godot
