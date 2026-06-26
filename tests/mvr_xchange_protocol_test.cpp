#include "mvr/xchange/mvr_xchange_commit.h"
#include "mvr/xchange/mvr_xchange_message.h"
#include "mvr/xchange/mvr_xchange_packet.h"
#include <cassert>
#include <string>

// Verifies bounded commit history and latest commit lookup behavior.
static void TestCommitStore() {
  MvrXchangeCommitStore store(2);
  MvrXchangeCommit first{"one", "station", "one.mvr", {}, {}, {1}};
  MvrXchangeCommit second{"two", "station", "two.mvr", {}, {}, {2, 3}};
  MvrXchangeCommit third{"three", "station", "three.mvr", {}, {}, {4}};
  store.Add(first);
  store.Add(second);
  store.Add(third);
  assert(!store.FindByFileUuid("one"));
  assert(store.FindByFileUuid("two"));
  assert(store.Latest()->fileUuid == "three");
}

// Verifies official MVR-xchange TCP packet framing.
static void TestPackets() {
  const std::string json = "{\"Type\":\"MVR_JOIN\"}";
  const std::vector<uint8_t> payload(json.begin(), json.end());
  auto encoded = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  auto decoded = mvr::xchange::TryDecodePacket(encoded);
  assert(decoded);
  assert(decoded->type == mvr::xchange::PacketType::Json);
  assert(std::string(decoded->payload.begin(), decoded->payload.end()) == json);
}

// Verifies minimal MVR-xchange JSON parsing and response serialization.
static void TestMessages() {
  auto msg = mvr::xchange::ParseMessage("{\"MessageType\":\"MVR_REQUEST\",\"FileUUID\":\"abc\"}");
  assert(msg.type == "MVR_REQUEST");
  assert(msg.fileUuid == "abc");
  MvrXchangeCommit commit{"abc", "station", "scene.mvr", "Manual", {}, {1, 2, 3}};
  const std::string response = mvr::xchange::BuildRequestRet(commit);
  assert(response.find("MVR_REQUEST_RET") != std::string::npos);
  assert(response.find("\"FileSize\":3") != std::string::npos);
  const std::string error = mvr::xchange::BuildError("MVR_REQUEST_RET", "Requested MVR file is not available.");
  assert(error.find("\"OK\":false") != std::string::npos);
}

// Runs focused non-GUI MVR-xchange protocol coverage.
int main() {
  TestCommitStore();
  TestPackets();
  TestMessages();
  return 0;
}
