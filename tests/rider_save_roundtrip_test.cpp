#include <cassert>
#include <filesystem>
#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"

int main(int argc, char **argv) {
    wxInitializer initializer;
    assert(initializer.IsOk());
    assert(argc >= 2);

    auto &cfg = ConfigManager::Get();
    cfg.Reset();
    assert(RiderImporter::Import(argv[1]));

    std::size_t count = cfg.GetScene().fixtures.size();
    assert(count > 0);

    std::filesystem::path temp = std::filesystem::temp_directory_path() / "rider_roundtrip.pera";
    assert(cfg.SaveProject(temp.string()));

    cfg.Reset();
    assert(cfg.LoadProject(temp.string()));
    assert(cfg.GetScene().fixtures.size() == count);

    std::filesystem::remove(temp);
    return 0;
}
