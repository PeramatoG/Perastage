#include "peraviz_loader.h"

#include "mvr_scene_loader.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

void PeravizLoader::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_mvr", "path"), &PeravizLoader::load_mvr);
}

Array PeravizLoader::load_mvr(const String &path) const {
    const peraviz::SceneModel model = peraviz::load_mvr(std::string(path.utf8().get_data()));

    UtilityFunctions::print("[PeravizNative] load_mvr nodes=", model.nodes.size(),
                            " fixtures=", model.fixture_count,
                            " trusses=", model.truss_count,
                            " objects=", model.object_count,
                            " supports=", model.support_count,
                            " extracted_assets=", model.extracted_asset_count,
                            " cache=", String(model.cache_path.c_str()));

    Array out;
    out.resize(static_cast<int64_t>(model.nodes.size()));
    int index = 0;
    for (const auto &node : model.nodes) {
        Dictionary d;
        d["node_id"] = String(node.node_id.c_str());
        d["parent_id"] = String(node.parent_id.c_str());
        d["name"] = String(node.name.c_str());
        d["type"] = String(node.type.c_str());
        d["asset_path"] = String(node.asset_path.c_str());
        d["is_fixture"] = node.is_fixture;
        d["is_axis"] = node.is_axis;
        d["is_emitter"] = node.is_emitter;
        d["pos"] = Vector3(node.local_transform.position.x, node.local_transform.position.y,
                            node.local_transform.position.z);
        d["rot"] = Vector3(node.local_transform.rotation_degrees.x,
                            node.local_transform.rotation_degrees.y,
                            node.local_transform.rotation_degrees.z);
        d["scale"] = Vector3(node.local_transform.scale.x, node.local_transform.scale.y,
                              node.local_transform.scale.z);
        out[index++] = d;
    }
    return out;
}

} // namespace godot
