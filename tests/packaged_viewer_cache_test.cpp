#include "mvr_import_package.h"
#include "packaged_resource_identity.h"
#include "resources/mesh_processing.h"
#include "cache_service.h"
#include "apppaths.h"
#include "performance_metrics.h"
#include "support/zip_test_utils.h"
#include <cassert>
#include <fstream>
#include <wx/init.h>
#include <wx/mstream.h>

namespace fs = std::filesystem;
const auto root = fs::temp_directory_path() / ("packaged-viewer-cache-" +
    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
namespace AppPaths {
fs::path GetUserDataDir() { return root / "data"; }
}
std::vector<uint8_t> Package(const std::string &texture, bool reverse = false) {
  std::vector<tests::zip::Entry> entries = {
    {"GeneralSceneDescription.xml", "<GeneralSceneDescription/>"},
    {"models/object.3ds", "model bytes"}, {"texture.png", texture}};
  if (reverse) std::reverse(entries.begin(), entries.end());
  wxMemoryOutputStream output;
  {
    wxZipOutputStream zip(output);
    for (const auto &entry : entries) {
      assert(zip.PutNextEntry(wxString::FromUTF8(entry.name)));
      zip.Write(entry.payload.data(), entry.payload.size());
    }
    assert(zip.Close());
  }
  std::vector<uint8_t> bytes(output.GetSize());
  output.CopyTo(bytes.data(), bytes.size());
  return bytes;
}
int main() {
  wxInitializer wx;
  assert(wx.IsOk());
  runtime_storage::SetRuntimeRootOverrideForTests(root / "runtime");
  std::vector<MvrImportDiagnostic> diagnostics;
  auto first = mvr::AcquireImportPackage(Package("red"), diagnostics, false);
  auto second = mvr::AcquireImportPackage(Package("red", true), diagnostics, false);
  auto different = mvr::AcquireImportPackage(Package("blue"), diagnostics, false);
  assert(first && second && different);
  const auto a = first->rootPath / "models/object.3ds";
  const auto b = second->rootPath / "models/object.3ds";
  const auto c = different->rootPath / "models/object.3ds";
  assert(a != b);
  const auto identity = packaged_resources::Lookup(a);
  assert(identity && identity == packaged_resources::Lookup(b));
  assert(identity != packaged_resources::Lookup(c)); // Companion textures affect identity.
  const auto cacheA = CacheService::ViewerCachePath(a.string(), "", false);
  assert(cacheA == CacheService::ViewerCachePath(b.string(), "", false));
  assert(cacheA != CacheService::ViewerCachePath(c.string(), "", false));
  assert(CacheService::ViewerCachePath(a.string(), "Mode A", true) !=
         CacheService::ViewerCachePath(a.string(), "Mode B", true));
  Mesh mesh;
  mesh.vertices = {0,0,0,1,0,0,0,1,0}; mesh.indices = {0,1,2};
  Mesh loaded;
  performance::Reset();
  {
    CacheService::ViewerCacheBatch batch;
    assert(batch.prepared);
    for (size_t i = 0; i < 100; ++i) {
      assert(viewer3d::resources::TrySaveMeshCache(a.string(), mesh));
      assert(viewer3d::resources::TryLoadMeshCache(b.string(), loaded));
    }
    assert(performance::samples[static_cast<size_t>(performance::Work::CacheRoot)].count == 1);
    // Prepared batches retain live ancestor and cache-leaf symlink validation.
    const auto bucket = AppPaths::GetViewer3DMeshCacheDir();
    fs::rename(bucket, bucket.string() + ".saved");
    std::error_code ec;
    fs::create_directory_symlink(a.parent_path(), bucket, ec);
    if (!ec) {
      assert(!viewer3d::resources::TrySaveMeshCache(a.string(), mesh));
      fs::remove(bucket);
    }
    fs::rename(bucket.string() + ".saved", bucket);
  }
  assert(loaded.vertices == mesh.vertices);
  // Workspace identity survives transfer to a scene lease and shared history.
  auto lease = first->workspace.TransferToSceneLease();
  first.reset();
  assert(packaged_resources::Lookup(a) == identity);
  auto undoLease = lease;
  lease.reset();
  assert(packaged_resources::Lookup(a) == identity);
  undoLease.reset();
  assert(!packaged_resources::Lookup(a));
  // If application code replaces an extracted resource, stale content identity
  // is rejected, even when its byte length stays the same.
  fs::last_write_time(b, fs::last_write_time(b) + std::chrono::seconds(1));
  assert(!packaged_resources::Lookup(b));
  assert(!viewer3d::resources::TryLoadMeshCache(b.string(), loaded));
  second.reset(); different.reset();
  fs::remove_all(root);
}
