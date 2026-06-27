#include "xchange/mvr_xchange_commit.h"
#include "xchange/mvr_xchange_dns_names.h"
#include "../core/uuidutils.h"
#include "xchange/mvr_xchange_message.h"
#include "xchange/mvr_xchange_network_interfaces.h"
#include "xchange/mvr_xchange_packet.h"
#include "xchange/mvr_xchange_station_registry.h"
#include "json.hpp"
#include <cassert>
#include <string>

// Verifies bounded commit history and latest commit lookup behavior.
static void TestCommitStore() {
  MvrXchangeCommitStore store(2);
  MvrXchangeCommit first{"11111111-1111-1111-1111-111111111111", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", "one.mvr", {}, {}, {1}};
  MvrXchangeCommit second{"22222222-2222-2222-2222-222222222222", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", "two.mvr", {}, {}, {2, 3}};
  MvrXchangeCommit third{"33333333-3333-3333-3333-333333333333", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", "three.mvr", {}, {}, {4}};
  store.Add(first);
  store.Add(second);
  store.Add(third);
  assert(!store.FindByFileUuid("11111111-1111-1111-1111-111111111111"));
  assert(store.FindByFileUuid("22222222-2222-2222-2222-222222222222"));
  assert(store.Latest()->fileUuid == "33333333-3333-3333-3333-333333333333");
  assert(store.Latest()->FileSize() == 1);
}

// Verifies official MVR-xchange TCP packet framing constants and roundtrip behavior.
static void TestPackets() {
  const std::string json = "{\"Type\":\"MVR_JOIN\"}";
  const std::vector<uint8_t> payload(json.begin(), json.end());
  auto encoded = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  assert(encoded.size() == 28 + payload.size());
  assert(encoded[0] == 0x00 && encoded[1] == 0x0b && encoded[2] == 0xe1 && encoded[3] == 0xba);
  assert(encoded[7] == 0x01);
  assert(encoded[15] == 0x01);
  auto decoded = mvr::xchange::TryDecodePacket(encoded);
  assert(decoded);
  assert(decoded->type == mvr::xchange::PacketType::Json);
  assert(std::string(decoded->payload.begin(), decoded->payload.end()) == json);
}

// Verifies official MVR-xchange JSON parsing and response serialization.
static void TestMessages() {
  auto msg = mvr::xchange::ParseMessage("{\"Type\":\"MVR_REQUEST\",\"FileUUID\":\"abcdefab-cdef-abcd-efab-cdefabcdefab\"}");
  assert(msg);
  assert(msg->type == "MVR_REQUEST");
  assert(msg->fileUuid == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(!mvr::xchange::ParseMessage("not json"));

  MvrXchangeCommit commit{"ABCDEFAB-CDEF-ABCD-EFAB-CDEFABCDEFAB", "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "scene.mvr", "Manual", {}, {1, 2, 3}};
  const auto commitJson = nlohmann::json::parse(mvr::xchange::BuildCommit(commit));
  assert(commitJson["Type"] == "MVR_COMMIT");
  assert(commitJson["verMajor"] == 1);
  assert(commitJson["verMinor"] == 6);
  assert(commitJson["FileSize"] == 3);
  assert(commitJson["FileUUID"] == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(commitJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(commitJson["ForStationsUUID"].is_array());

  const auto outgoingJoinJson = nlohmann::json::parse(mvr::xchange::BuildJoin("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage", {commit}));
  assert(outgoingJoinJson["Type"] == "MVR_JOIN");
  assert(outgoingJoinJson["Provider"] == "Perastage");
  assert(outgoingJoinJson["verMajor"] == 1);
  assert(outgoingJoinJson["verMinor"] == 6);
  assert(outgoingJoinJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(outgoingJoinJson["Commits"].size() == 1);
  assert(outgoingJoinJson["Files"].size() == 1);

  auto parsedJoin = mvr::xchange::ParseMessage(outgoingJoinJson.dump());
  assert(parsedJoin);
  assert(parsedJoin->type == "MVR_JOIN");
  assert(parsedJoin->provider == "Perastage");
  assert(parsedJoin->stationName == "Perastage");
  assert(parsedJoin->verMajor == 1);
  assert(parsedJoin->verMinor == 6);
  assert(parsedJoin->commits.size() == 1);

  const auto joinJson = nlohmann::json::parse(mvr::xchange::BuildJoinRet("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage", {commit}));
  assert(joinJson["Type"] == "MVR_JOIN_RET");
  assert(joinJson["OK"] == true);
  assert(joinJson["Provider"] == "Perastage");
  assert(joinJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(joinJson["Commits"].size() == 1);
  assert(joinJson["Files"].size() == 1);
  auto parsedJoinRet = mvr::xchange::ParseMessage(joinJson.dump());
  assert(parsedJoinRet);
  assert(parsedJoinRet->type == "MVR_JOIN_RET");
  assert(parsedJoinRet->ok);
  assert(parsedJoinRet->commits.size() == 1);

  const auto errorJson = nlohmann::json::parse(mvr::xchange::BuildRequestError("The MVR is not available on this client"));
  assert(errorJson["Type"] == "MVR_REQUEST_RET");
  assert(errorJson["OK"] == false);
  assert(errorJson["Message"] == "The MVR is not available on this client");
}

// Verifies MVR-xchange DNS-SD naming helpers.
static void TestDnsNames() {
  assert(mvr::xchange::BuildMvrXchangeGroupServiceName("Default") == "Default._mvrxchange._tcp.local.");
  assert(mvr::xchange::BuildMvrXchangeServiceInstanceName("Perastage", "Default") == "Perastage.Default._mvrxchange._tcp.local.");
}

// Verifies MVR-xchange UUID helpers use canonical lowercase UUIDs.
static void TestCanonicalUuidUse() {
  const auto generated = GenerateUuid();
  assert(CanonicalizeUuid(generated) == generated);
  assert(CanonicalizeUuid("ABCDEFABCDEFABCDEFABCDEFABCDEFAB") == "abcdefab-cdef-abcd-efab-cdefabcdefab");
}

// Verifies remote station registry self-filtering, deduplication, and join states.
static void TestStationRegistry() {
  MvrXchangeStationRegistry registry;
  registry.SetLocalIdentity("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage.Default._mvrxchange._tcp.local.", 42424);
  MvrXchangeRemoteStation own;
  own.stationUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  own.ipAddress = "127.0.0.1";
  own.port = 42424;
  assert(!registry.UpsertDiscovered(own));

  MvrXchangeRemoteStation discovered;
  discovered.stationUuid = "BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF";
  discovered.stationName = "grandMA3";
  discovered.serviceInstanceName = "grandMA3.Default._mvrxchange._tcp.local.";
  discovered.ipAddress = "127.0.0.1";
  discovered.port = 50000;
  assert(registry.UpsertDiscovered(discovered));
  assert(registry.List().size() == 1);
  assert(registry.List()[0].discovered);

  MvrXchangeRemoteStation incoming = discovered;
  incoming.provider = "grandMA3";
  incoming.verMajor = 1;
  incoming.verMinor = 6;
  incoming.commits.push_back({"cccccccc-dddd-eeee-ffff-000000000000", "bbbbbbbb-cccc-dddd-eeee-ffffffffffff", "remote.mvr", {}, {}, {1}});
  assert(registry.UpsertIncomingJoin(incoming));
  assert(registry.List().size() == 1);
  assert(registry.List()[0].incomingJoined);
  assert(registry.List()[0].commits.size() == 1);
  assert(registry.MarkOutgoingJoined("bbbbbbbb-cccc-dddd-eeee-ffffffffffff", "127.0.0.1", 50000));
  assert(registry.List()[0].outgoingJoined);
  assert(registry.JoinedStations().size() == 1);
}

// Verifies that loopback is always available for same-machine MVR-xchange tests.
static void TestNetworkInterfaces() {
  const auto interfaces = ListMvrXchangeNetworkInterfaces();
  bool foundLoopback = false;
  for (const auto &iface : interfaces) foundLoopback = foundLoopback || iface.ipv4Address == "127.0.0.1";
  assert(foundLoopback);
  const auto loopback = SelectMvrXchangeNetworkInterface("127.0.0.1");
  assert(loopback.ipv4Address == "127.0.0.1");
}

// Runs focused non-GUI MVR-xchange protocol coverage.
int main() {
  TestCommitStore();
  TestPackets();
  TestMessages();
  TestDnsNames();
  TestCanonicalUuidUse();
  TestStationRegistry();
  TestNetworkInterfaces();
  return 0;
}
