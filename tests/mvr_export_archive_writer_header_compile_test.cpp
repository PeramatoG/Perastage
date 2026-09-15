#include "mvr_export_archive_writer.h"
#include "mvr_export_transport.h"

// Verifies that the archive and transport headers are self-contained.
int main() {
  mvr_export_archive::Request request;
  mvr_export_transport::Result result;
  return request.resources.empty() && !result.success ? 0 : 1;
}
