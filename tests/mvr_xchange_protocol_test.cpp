#include "xchange/mvr_xchange_commit.h"
#include "xchange/mvr_xchange_dns_names.h"
#include "../core/uuidutils.h"
#include "xchange/mvr_xchange_message.h"
#include "xchange/mvr_xchange_mdns_cache.h"
#include "xchange/mvr_xchange_network_interfaces.h"
#include "xchange/mvr_xchange_packet.h"
#include "xchange/mvr_xchange_station_registry.h"
#include "xchange/mvr_xchange_tcp_client.h"
#include "xchange/mvr_xchange_tcp_server.h"
#include "json.hpp"
#include <cassert>
#include <chrono>
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

// Verifies framing rejects invalid packages and reassembles bounded multipart payloads.
static void TestPacketRejection() {
  std::vector<uint8_t> payload{'x'};
  auto multipart = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  multipart[15] = 2;
  mvr::xchange::Packet packet;
  std::string error;
  assert(mvr::xchange::DecodePacket(multipart, packet, error) == mvr::xchange::DecodeStatus::Complete);
  auto invalidType = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, payload);
  invalidType[19] = 9;
  assert(mvr::xchange::DecodePacket(invalidType, packet, error) == mvr::xchange::DecodeStatus::Invalid);

  mvr::xchange::PacketReassembler reassembler;
  mvr::xchange::Packet complete;
  mvr::xchange::Packet first{mvr::xchange::PacketType::Json, 0, 2, {'a'}};
  mvr::xchange::Packet second{mvr::xchange::PacketType::Json, 1, 2, {'b'}};
  assert(reassembler.Add(first, complete, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  assert(reassembler.Add(second, complete, error) == mvr::xchange::DecodeStatus::Complete);
  assert(std::string(complete.payload.begin(), complete.payload.end()) == "ab");
  assert(reassembler.Add(second, complete, error) == mvr::xchange::DecodeStatus::Invalid);
  auto firstWire = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {'a'});
  auto secondWire = mvr::xchange::EncodePacket(mvr::xchange::PacketType::Json, {'b'});
  firstWire[15] = 2;
  secondWire[11] = 1;
  secondWire[15] = 2;
  firstWire.insert(firstWire.end(), secondWire.begin(), secondWire.end());
  reassembler.Reset();
  assert(mvr::xchange::DecodePacket(firstWire, packet, error) == mvr::xchange::DecodeStatus::Complete);
  assert(reassembler.Add(packet, complete, error) == mvr::xchange::DecodeStatus::NeedMoreData);
  assert(mvr::xchange::DecodePacket(firstWire, packet, error) == mvr::xchange::DecodeStatus::Complete);
  assert(reassembler.Add(packet, complete, error) == mvr::xchange::DecodeStatus::Complete);
}

// Verifies cached DNS-SD records resolve across layouts, order, TTL, and goodbye.
static void TestMdnsRecordCache() {
  using namespace mvr::xchange;
  MdnsRecordCache cache;
  const std::string group = "Default._mvrxchange._tcp.local.";
  cache.Apply({DnsRecordType::A, "HOST.local.", {}, "192.0.2.10", {}, 0, 120, 4, 1000});
  cache.Apply({DnsRecordType::Txt, "Peer.Default._mvrxchange._tcp.local.", {}, {}, {{"stationname", "Peer"}, {"stationuuid", "BBBBBBBB-CCCC-DDDD-EEEE-FFFFFFFFFFFF"}}, 0, 120, 4, 1000});
  cache.Apply({DnsRecordType::Srv, "peer.default._mvrxchange._tcp.local", "host.LOCAL", {}, {}, 42424, 120, 4, 1000});
  cache.Apply({DnsRecordType::Ptr, group, "PEER.Default._mvrxchange._tcp.local.", {}, {}, 0, 120, 4, 1000});
  auto stations = cache.Resolve(group, 1000);
  assert(stations.size() == 1 && stations[0].ipAddress == "192.0.2.10" && stations[0].stationName == "Peer");
  cache.Apply({DnsRecordType::Ptr, group, "PEER.Default._mvrxchange._tcp.local.", {}, {}, 0, 0, 4, 2000});
  assert(cache.Resolve(group, 2500).size() == 1);
  cache.Expire(3000);
  assert(cache.Resolve(group, 3000).empty());

  MdnsRecordCache official;
  official.Apply({DnsRecordType::Ptr, group, "Member.Default._mvrxchange._tcp.local.", {}, {}, 0, 60, 1, 5000});
  official.Apply({DnsRecordType::Srv, group, "group-host.local.", {}, {}, 43000, 60, 1, 5000});
  official.Apply({DnsRecordType::Txt, group, {}, {}, {{"stationname", "Group member"}, {"stationuuid", "11111111-2222-3333-4444-555555555555"}}, 0, 60, 1, 5000});
  official.Apply({DnsRecordType::Aaaa, "group-host.local", {}, "2001:db8::1", {}, 0, 60, 1, 5000});
  stations = official.Resolve(group, 5000);
  assert(stations.size() == 1 && stations[0].ipAddress == "2001:db8::1" && stations[0].port == 43000);
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
  MvrXchangeRemoteStation outgoing = discovered;
  outgoing.outgoingJoined = true;
  outgoing.inventorySpecified = true;
  outgoing.commits.clear();
  assert(registry.UpsertOutgoingJoin(outgoing));
  assert(registry.List()[0].commits.empty());
  MvrXchangeCommit update{"dddddddd-eeee-ffff-0000-111111111111", "bbbbbbbb-cccc-dddd-eeee-ffffffffffff", "new.mvr", {}, {}, {}};
  update.declaredFileSize = 42;
  update.declaredFileSizeSpecified = true;
  assert(registry.ApplyCommit(update));
  assert(registry.List()[0].commits.size() == 1 && registry.List()[0].commits[0].payload.empty());
  assert(registry.ApplyCommit(update));
  assert(registry.List()[0].commits.size() == 1);
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

// Verifies JOIN, COMMIT, and REQUEST use independent matching TCP transactions.
static void TestTcpTransactions() {
  const std::string localUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  const std::string remoteUuid = "bbbbbbbb-cccc-dddd-eeee-ffffffffffff";
  MvrXchangeCommit available{"11111111-2222-3333-4444-555555555555", localUuid, "scene.mvr", {}, {}, {'m', 'v', 'r'}};
  MvrXchangeSettings settings;
  settings.stationName = "Server";
  settings.stationUuid = localUuid;
  settings.port = 0;
  int joins = 0;
  int commits = 0;
  std::string leftStation;
  MvrXchangeTcpServer server;
  assert(server.Start(settings,
                      [&](const std::string &fileUuid) { return fileUuid.empty() || fileUuid == available.fileUuid ? std::optional<MvrXchangeCommit>(available) : std::nullopt; },
                      [&] { return std::vector<MvrXchangeCommit>{available}; }, {},
                      [&](const MvrXchangeRemoteStation &) { ++joins; }, [&](const std::string &uuid) { leftStation = uuid; return std::string{}; },
                      [&](const MvrXchangeCommit &) { ++commits; return std::string{}; }));
  MvrXchangeRemoteStation endpoint;
  endpoint.stationName = "Server";
  endpoint.stationUuid = localUuid;
  endpoint.ipAddress = "127.0.0.1";
  endpoint.port = server.Port();
  MvrXchangeTcpClient client;
  MvrXchangeSettings clientSettings;
  clientSettings.stationName = "Client";
  clientSettings.stationUuid = remoteUuid;
  MvrXchangeRemoteStation joined;
  assert(client.SendJoin(endpoint, clientSettings, {}, joined, {}));
  assert(joins == 1 && joined.stationUuid == localUuid && joined.commits.size() == 1);
  MvrXchangeCommit announcement{"22222222-3333-4444-5555-666666666666", remoteUuid, "remote.mvr", {}, {}, {}};
  announcement.declaredFileSize = 10;
  announcement.declaredFileSizeSpecified = true;
  assert(client.SendCommit(endpoint, announcement, {}));
  assert(commits == 1);
  for (int i = 0; i < 32; ++i) assert(client.SendCommit(endpoint, announcement, {}));
  assert(commits == 33);
  endpoint.commits = {available};
  endpoint.commits[0].declaredFileSize = 3;
  endpoint.commits[0].declaredFileSizeSpecified = true;
  const auto requested = client.RequestCommit(endpoint, available.fileUuid, remoteUuid, {});
  assert(requested && requested->payload == available.payload);
  assert(client.SendLeave(endpoint, remoteUuid, {}));
  assert(leftStation == remoteUuid);
  const auto stopStarted = std::chrono::steady_clock::now();
  server.Stop();
  assert(std::chrono::steady_clock::now() - stopStarted < std::chrono::seconds(2));
}

// Verifies an idle listener stops promptly without relying on accept interruption.
static void TestTcpServerIdleStop() {
  MvrXchangeSettings settings;
  settings.stationName = "Idle server";
  settings.stationUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  for (int iteration = 0; iteration < 10; ++iteration) {
    MvrXchangeTcpServer server;
    assert(server.Start(settings, {}, {}, {}, {}, {}, {}));
    const auto stopStarted = std::chrono::steady_clock::now();
    server.Stop();
    assert(std::chrono::steady_clock::now() - stopStarted < std::chrono::seconds(2));
  }
}

// Runs focused non-GUI MVR-xchange protocol coverage.
int main() {
  TestCommitStore();
  TestPackets();
  TestMessages();
  TestMalformedMessages();
  TestInventoryCompatibility();
  TestPacketRejection();
  TestMdnsRecordCache();
  TestDnsNames();
  TestCanonicalUuidUse();
  TestStationRegistry();
  TestNetworkInterfaces();
  TestTcpTransactions();
  TestTcpServerIdleStop();
  return 0;
}
