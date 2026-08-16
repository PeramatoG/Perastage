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
  assert(mvr::xchange::ValidateMessage(*msg).empty());
  assert(!mvr::xchange::ParseMessage("not json"));
  assert(!mvr::xchange::ParseMessage("[]"));

  MvrXchangeCommit commit{"ABCDEFAB-CDEF-ABCD-EFAB-CDEFABCDEFAB", "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "scene.mvr", "Manual", {}, {1, 2, 3}};
  const auto commitJson = nlohmann::json::parse(mvr::xchange::BuildCommit(commit));
  assert(commitJson["Type"] == "MVR_COMMIT");
  assert(commitJson["verMajor"] == 1);
  assert(commitJson["verMinor"] == 6);
  assert(commitJson["FileSize"] == 3);
  assert(commitJson["FileUUID"] == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(commitJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(commitJson["ForStationsUUID"].is_array());
  auto parsedCommit = mvr::xchange::ParseMessage(commitJson.dump());
  assert(parsedCommit);
  assert(parsedCommit->type == "MVR_COMMIT");
  assert(parsedCommit->fileUuid == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(parsedCommit->stationUuid == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(mvr::xchange::ValidateMessage(*parsedCommit).empty());

  const auto outgoingJoinJson = nlohmann::json::parse(mvr::xchange::BuildJoin("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage", {commit}));
  assert(outgoingJoinJson["Type"] == "MVR_JOIN");
  assert(outgoingJoinJson["Provider"] == "Perastage");
  assert(outgoingJoinJson["verMajor"] == 1);
  assert(outgoingJoinJson["verMinor"] == 6);
  assert(outgoingJoinJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(outgoingJoinJson["Commits"].size() == 1);
  assert(!outgoingJoinJson.contains("Files"));

  auto parsedJoin = mvr::xchange::ParseMessage(outgoingJoinJson.dump());
  assert(parsedJoin);
  assert(parsedJoin->type == "MVR_JOIN");
  assert(parsedJoin->provider == "Perastage");
  assert(parsedJoin->stationName == "Perastage");
  assert(parsedJoin->verMajor == 1);
  assert(parsedJoin->verMinor == 6);
  assert(parsedJoin->commits.size() == 1);
  assert(parsedJoin->commits[0].payload.empty());
  assert(parsedJoin->commits[0].declaredFileSize == 3);
  assert(mvr::xchange::ValidateMessage(*parsedJoin).empty());

  const auto joinJson = nlohmann::json::parse(mvr::xchange::BuildJoinRet("AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", "Perastage", {commit}));
  assert(joinJson["Type"] == "MVR_JOIN_RET");
  assert(joinJson["OK"] == true);
  assert(joinJson["Provider"] == "Perastage");
  assert(joinJson["StationUUID"] == "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  assert(joinJson["Commits"].size() == 1);
  assert(!joinJson.contains("Files"));
  auto parsedJoinRet = mvr::xchange::ParseMessage(joinJson.dump());
  assert(parsedJoinRet);
  assert(parsedJoinRet->type == "MVR_JOIN_RET");
  assert(parsedJoinRet->ok);
  assert(parsedJoinRet->commits.size() == 1);
  assert(mvr::xchange::ValidateMessage(*parsedJoinRet).empty());

  const auto errorJson = nlohmann::json::parse(mvr::xchange::BuildRequestError("The MVR is not available on this client"));
  assert(errorJson["Type"] == "MVR_REQUEST_RET");
  assert(errorJson["OK"] == false);
  assert(errorJson["Message"] == "The MVR is not available on this client");
}

// Verifies canonical inventory presence and isolated legacy Files compatibility.
static void TestInventoryCompatibility() {
  const std::string entry = R"({"Type":"MVR_COMMIT","verMajor":1,"verMinor":6,"FileSize":999999999999,"FileUUID":"11111111-1111-1111-1111-111111111111","StationUUID":"22222222-2222-2222-2222-222222222222","ForStationsUUID":[],"FileName":"scene.mvr","Comment":"remote"})";
  auto absent = mvr::xchange::ParseMessage(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222"})");
  assert(absent && absent->inventoryPresence == mvr::xchange::InventoryPresence::Absent);
  auto empty = mvr::xchange::ParseMessage(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","Commits":[]})");
  assert(empty && empty->inventoryPresence == mvr::xchange::InventoryPresence::PresentEmpty);
  auto files = mvr::xchange::ParseMessage(std::string(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","Files":[)") + entry + "]}");
  assert(files && files->commits.size() == 1 && files->commits[0].payload.empty());
  auto both = mvr::xchange::ParseMessage(std::string(R"({"Type":"MVR_JOIN","Provider":"Peer","StationName":"Peer","StationUUID":"22222222-2222-2222-2222-222222222222","Commits":[)") + entry + R"(],"Files":[)" + entry + "]}");
  assert(both && both->commits.size() == 1);
  assert(both->commits[0].fileName == "scene.mvr" && both->commits[0].comment == "remote");
}

// Verifies framing rejects invalid, oversized, and multipart packages safely.
static void TestPacketRejection() {
  std::vector<uint8_t> payload{'x'};
  auto multipart = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  multipart[15] = 2;
  mvr::xchange::Packet packet;
  std::string error;
  assert(mvr::xchange::DecodePacket(multipart, packet, error) == mvr::xchange::DecodeStatus::Invalid);
  auto invalidType = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  invalidType[19] = 9;
  assert(mvr::xchange::DecodePacket(invalidType, packet, error) == mvr::xchange::DecodeStatus::Invalid);
}

// Verifies malformed official messages are parsed safely and rejected with clear errors.
static void TestMalformedMessages() {
  auto invalidJoinUuid = mvr::xchange::ParseMessage("{\"Type\":\"MVR_JOIN\",\"Provider\":\"Other\",\"StationName\":\"Desk\",\"StationUUID\":\"not-a-uuid\"}");
  assert(invalidJoinUuid);
  assert(!mvr::xchange::ValidateMessage(*invalidJoinUuid).empty());

  auto missingProvider = mvr::xchange::ParseMessage("{\"Type\":\"MVR_JOIN\",\"StationName\":\"Desk\",\"StationUUID\":\"BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF\"}");
  assert(missingProvider);
  assert(mvr::xchange::ValidateMessage(*missingProvider) == "MVR_JOIN is missing Provider.");

  auto invalidCommitUuid = mvr::xchange::ParseMessage("{\"Type\":\"MVR_COMMIT\",\"FileUUID\":\"bad\",\"StationUUID\":\"BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF\"}");
  assert(invalidCommitUuid);
  assert(mvr::xchange::ValidateMessage(*invalidCommitUuid) == "MVR_COMMIT is missing a valid FileUUID.");

  auto invalidRequestUuid = mvr::xchange::ParseMessage("{\"Type\":\"MVR_REQUEST\",\"FileUUID\":\"latest\"}");
  assert(invalidRequestUuid);
  assert(mvr::xchange::ValidateMessage(*invalidRequestUuid) == "MVR_REQUEST contains an invalid FileUUID.");

  auto unknown = mvr::xchange::ParseMessage("{\"Type\":\"PRIVATE_SYNC\",\"StationUUID\":\"BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF\"}");
  assert(unknown);
  assert(mvr::xchange::ValidateMessage(*unknown) == "Unsupported MVR-xchange message type.");
}

// Verifies MVR-xchange DNS-SD naming helpers.
static void TestDnsNames() {
  assert(mvr::xchange::BuildMvrXchangeGroupServiceName("Default") == "Default._mvrxchange._tcp.local.");
  assert(mvr::xchange::BuildMvrXchangeServiceInstanceName("Perastage", "Default") == "Perastage.Default._mvrxchange._tcp.local.");
  assert(mvr::xchange::NormalizeDnsName("Peer.DEFAULT.Local.") == "peer.default.local");
  assert(mvr::xchange::DnsNamesEqual("Peer.Default.local", "peer.default.LOCAL."));
}

// Verifies MVR-xchange UUID helpers use canonical lowercase UUIDs.
static void TestCanonicalUuidUse() {
  const auto generated = GenerateUuid();
  assert(CanonicalizeUuid(generated) == generated);
  assert(CanonicalizeUuid("ABCDEFABCDEFABCDEFABCDEFABCDEFAB") == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(CanonicalizeUuid("ABCDEFAB-CDEF-ABCD-EFAB-CDEFABCDEFAB") == "abcdefab-cdef-abcd-efab-cdefabcdefab");
  assert(CanonicalizeUuid("not-a-uuid").empty());
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
  assert(registry.MarkLeft("bbbbbbbb-cccc-dddd-eeee-ffffffffffff"));
  assert(registry.JoinedStations().empty());
  assert(registry.UpsertIncomingJoin(incoming));
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
  TestMalformedMessages();
  TestInventoryCompatibility();
  TestPacketRejection();
  TestDnsNames();
  TestCanonicalUuidUse();
  TestStationRegistry();
  TestNetworkInterfaces();
  return 0;
}
