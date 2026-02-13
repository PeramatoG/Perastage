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

    UtilityFunctions::print("[PeravizNative] load_mvr instances=", model.instances.size(),
                            " fixtures=", model.fixture_count,
                            " trusses=", model.truss_count,
                            " objects=", model.object_count,
                            " supports=", model.support_count);

    for (int i = 0; i < 3 && i < static_cast<int>(model.instances.size()); ++i) {
        const auto &inst = model.instances[static_cast<size_t>(i)];
        UtilityFunctions::print("[PeravizNative] sample[", i, "] ", String(inst.type.c_str()),
                                " ", String(inst.id.c_str()),
                                " pos=", inst.transform.position.x, ",",
                                inst.transform.position.y, ",", inst.transform.position.z);
    }

    Array out;
    out.resize(static_cast<int64_t>(model.instances.size()));
    int index = 0;
    for (const auto &inst : model.instances) {
        Dictionary d;
        d["id"] = String(inst.id.c_str());
        d["type"] = String(inst.type.c_str());
        d["is_fixture"] = inst.is_fixture;
        d["pos"] = Vector3(inst.transform.position.x, inst.transform.position.y,
                            inst.transform.position.z);
        d["rot"] = Vector3(inst.transform.rotation_degrees.x,
                            inst.transform.rotation_degrees.y,
                            inst.transform.rotation_degrees.z);
        d["scale"] = Vector3(inst.transform.scale.x, inst.transform.scale.y,
                              inst.transform.scale.z);
        out[index++] = d;
    }
    return out;
}

} // namespace godot
