#include "command/command_transform.h"
#include "matrixutils.h"
#include "osc/osc_command_adapter.h"
#include "osc/osc_udp_transport.h"

#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
using perastage::osc::Argument;
using perastage::osc::Message;

class RecordingHost final : public perastage::command::ProjectMutationHost {
public:
  // Records semantic mutation publication.
  perastage::command::MutationPublication
  CommitMutation(const MvrScene &, const scene_grouping::ObjectSelection &,
                 const std::string &) override {
    ++publications;
    return {true, true};
  }
  int publications = 0;
};

struct Harness {
  Harness() : context{scene, selection, host} {
    Fixture fixture;
    fixture.uuid = "fixture-a";
    fixture.transform = MatrixUtils::Identity();
    scene.fixtures.emplace(fixture.uuid, fixture);
    selection.fixtures = {fixture.uuid};
  }

  MvrScene scene;
  scene_grouping::ObjectSelection selection;
  RecordingHost host;
  perastage::command::ExecutionContext context;
  scene_grouping::InteractiveTransformPolicy policy;
};

enum class BooleanWire {
  FalseTag,
  TrueTag,
  IntegerZero,
  IntegerOne,
  IntegerTwo
};
enum class ValueWire { Float, Integer };

// Appends an OSC string with its required zero padding.
void AppendString(std::vector<std::uint8_t> &packet, const std::string &value) {
  packet.insert(packet.end(), value.begin(), value.end());
  packet.push_back(0);
  while (packet.size() % 4 != 0)
    packet.push_back(0);
}

// Appends one big-endian 32-bit OSC word.
void AppendWord(std::vector<std::uint8_t> &packet, std::uint32_t word) {
  packet.push_back(static_cast<std::uint8_t>(word >> 24));
  packet.push_back(static_cast<std::uint8_t>(word >> 16));
  packet.push_back(static_cast<std::uint8_t>(word >> 8));
  packet.push_back(static_cast<std::uint8_t>(word));
}

// Returns the OSC type tag for one supported boolean encoding.
char BooleanTag(BooleanWire encoding) {
  if (encoding == BooleanWire::FalseTag)
    return 'F';
  if (encoding == BooleanWire::TrueTag)
    return 'T';
  return 'i';
}

// Appends an integer payload when the boolean uses an int32 tag.
void AppendBooleanValue(std::vector<std::uint8_t> &packet,
                        BooleanWire encoding) {
  if (encoding == BooleanWire::IntegerZero)
    AppendWord(packet, 0);
  else if (encoding == BooleanWire::IntegerOne)
    AppendWord(packet, 1);
  else if (encoding == BooleanWire::IntegerTwo)
    AppendWord(packet, 2);
}

// Builds one OSC transform packet in the focused adapter wire format.
std::vector<std::uint8_t>
TransformPacket(const std::string &address, ValueWire valueWire, double value,
                BooleanWire relative, const std::string &space,
                BooleanWire group, const std::string &axis = "x") {
  std::vector<std::uint8_t> packet;
  AppendString(packet, address);
  std::string tags = ",s";
  tags.push_back(valueWire == ValueWire::Float ? 'f' : 'i');
  tags.push_back(BooleanTag(relative));
  tags.push_back('s');
  tags.push_back(BooleanTag(group));
  AppendString(packet, tags);
  AppendString(packet, axis);
  if (valueWire == ValueWire::Float)
    AppendWord(packet, std::bit_cast<std::uint32_t>(static_cast<float>(value)));
  else
    AppendWord(packet,
               static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
  AppendBooleanValue(packet, relative);
  AppendString(packet, space);
  AppendBooleanValue(packet, group);
  return packet;
}

// Parses a packet and requires the focused OSC message contract.
Message Parse(const std::vector<std::uint8_t> &packet) {
  Message message;
  std::string error;
  assert(perastage::osc::ParseMessage(packet, message, error));
  return message;
}

// Compares the scene and selection state relevant to adapter validation.
void AssertUnchanged(const Harness &harness, const Matrix &before,
                     int publicationsBefore) {
  const Matrix &after = harness.scene.fixtures.at("fixture-a").transform;
  assert(after.u == before.u && after.v == before.v && after.w == before.w &&
         after.o == before.o);
  assert(harness.selection.fixtures == std::vector<std::string>{"fixture-a"});
  assert(harness.selection.trusses.empty());
  assert(harness.selection.supports.empty());
  assert(harness.selection.sceneObjects.empty());
  assert(harness.host.publications == publicationsBefore);
}

// Requires adapter validation to reject without mutation or publication.
void AssertRejected(Harness &harness, const Message &message) {
  const Matrix before = harness.scene.fixtures.at("fixture-a").transform;
  const int publicationsBefore = harness.host.publications;
  const auto result =
      perastage::osc::ExecuteMessage(message, harness.context, harness.policy);
  assert(result.outcome == perastage::command::Outcome::ValidationError);
  assert(result.diagnostics.size() == 1);
  assert(result.diagnostics.front().code == "osc.invalid_arguments");
  assert(!result.mutation.HasSemanticChanges());
  AssertUnchanged(harness, before, publicationsBefore);
}

// Sends one UDP datagram to the loopback-only test listener.
void SendLoopback(std::uint16_t port, const std::vector<std::uint8_t> &packet) {
  const auto socketFd = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  sendto(socketFd, reinterpret_cast<const char *>(packet.data()),
         static_cast<int>(packet.size()), 0,
         reinterpret_cast<sockaddr *>(&address), sizeof(address));
#if defined(_WIN32)
  closesocket(socketFd);
#else
  close(socketFd);
#endif
}

// Verifies malformed and unsupported OSC packets are rejected
// deterministically.
void CheckPacketRejection() {
  std::string error;
  Message message;
  const auto valid =
      TransformPacket("/perastage/transform/position", ValueWire::Float, 10.0,
                      BooleanWire::TrueTag, "world", BooleanWire::FalseTag);
  auto malformed = valid;
  malformed.pop_back();
  assert(!perastage::osc::ParseMessage(malformed, message, error));
  assert(error == "invalid_packet_size");
  auto invalidPadding = valid;
  invalidPadding.back() = 1;
  assert(!perastage::osc::ParseMessage(invalidPadding, message, error));
  std::vector<std::uint8_t> bundle;
  AppendString(bundle, "#bundle");
  AppendString(bundle, ",");
  assert(!perastage::osc::ParseMessage(bundle, message, error));
  assert(error == "bundles_unsupported");
  std::vector<std::uint8_t> unsupported;
  AppendString(unsupported, "/perastage/test");
  AppendString(unsupported, ",b");
  assert(!perastage::osc::ParseMessage(unsupported, message, error));
  assert(error == "unsupported_type_tag");
  std::vector<std::uint8_t> oversized(perastage::osc::kMaximumPacketBytes + 4);
  assert(!perastage::osc::ParseMessage(oversized, message, error));
  assert(error == "invalid_packet_size");
}

// Verifies native and integer OSC booleans with float and integer values.
void CheckTransformWireForms() {
  Harness native;
  const auto nativeMessage = Parse(
      TransformPacket("/perastage/transform/position", ValueWire::Float, 1250.0,
                      BooleanWire::TrueTag, "world", BooleanWire::FalseTag));
  auto result = perastage::osc::ExecuteMessage(nativeMessage, native.context,
                                               native.policy);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(native.host.publications == 1);
  assert(native.selection.fixtures == std::vector<std::string>{"fixture-a"});
  assert(std::fabs(native.scene.fixtures.at("fixture-a").transform.o[0] -
                   1250.0f) < 0.01f);

  Harness integer;
  integer.scene.fixtures.at("fixture-a").transform =
      MatrixUtils::EulerToMatrix(90.0f, 0.0f, 0.0f);
  const auto integerMessage = Parse(TransformPacket(
      "/perastage/transform/position", ValueWire::Integer, 400,
      BooleanWire::IntegerOne, "local", BooleanWire::IntegerZero));
  result = perastage::osc::ExecuteMessage(integerMessage, integer.context,
                                          integer.policy);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(integer.host.publications == 1);
  assert(std::get<bool>(result.request->arguments[1].value));
  assert(std::get<std::string>(result.request->arguments[2].value) == "local");
  assert(!std::get<bool>(result.request->arguments[3].value));
  assert(std::fabs(integer.scene.fixtures.at("fixture-a").transform.o[0]) <
         0.01f);
  assert(std::fabs(integer.scene.fixtures.at("fixture-a").transform.o[1]) >
         399.0f);
}

// Verifies grouped rotation remains one semantic transaction and publication.
void CheckGroupedRotation() {
  Harness harness;
  harness.scene.fixtures.at("fixture-a").transform.o[0] = -100.0f;
  Fixture second;
  second.uuid = "fixture-b";
  second.transform = MatrixUtils::Identity();
  second.transform.o[0] = 100.0f;
  harness.scene.fixtures.emplace(second.uuid, second);
  harness.selection.fixtures.push_back(second.uuid);
  const auto message = Parse(TransformPacket(
      "/perastage/transform/rotation", ValueWire::Float, 90.0,
      BooleanWire::FalseTag, "world", BooleanWire::TrueTag, "z"));
  const auto result =
      perastage::osc::ExecuteMessage(message, harness.context, harness.policy);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(harness.host.publications == 1);
  assert(std::get<bool>(result.request->arguments[3].value));
  assert(std::fabs(harness.scene.fixtures.at("fixture-a").transform.o[1]) >
         99.0f);
  assert(std::fabs(harness.scene.fixtures.at("fixture-b").transform.o[1]) >
         99.0f);
}

// Verifies invalid mapped arguments cannot mutate or publish Undo state.
void CheckMappingValidation() {
  Harness harness;
  AssertRejected(harness,
                 Parse(TransformPacket(
                     "/perastage/transform/position", ValueWire::Float, 10.0,
                     BooleanWire::IntegerTwo, "world", BooleanWire::FalseTag)));
  AssertRejected(harness, Message{"/perastage/transform/position",
                                  {std::string("q"), 10.0f, true,
                                   std::string("world"), false}});
  AssertRejected(harness, Message{"/perastage/transform/position",
                                  {std::string("x"), 10.0f, true,
                                   std::string("stage"), false}});
  AssertRejected(harness, Message{"/perastage/transform/position",
                                  {std::string("x"), 10.0f, true,
                                   std::string("world")}});
  AssertRejected(harness, Message{"/perastage/transform/position",
                                  {std::string("x"), std::string("10"), true,
                                   std::string("world"), false}});
  AssertRejected(harness, Message{"/perastage/transform/position",
                                  {std::string("x"), 10.0f, std::string("true"),
                                   std::string("world"), false}});
  AssertRejected(harness,
                 Message{"/perastage/selection/clear", {std::int32_t{1}}});
}

// Verifies selection clear remains one compatible semantic publication.
void CheckSelectionClear() {
  Harness harness;
  const auto result = perastage::osc::ExecuteMessage(
      Parse([] {
        std::vector<std::uint8_t> packet;
        AppendString(packet, "/perastage/selection/clear");
        AppendString(packet, ",");
        return packet;
      }()),
      harness.context, harness.policy);
  assert(result.Success() && result.mutation.selectionChanged);
  assert(harness.selection.fixtures.empty());
  assert(harness.host.publications == 1);
}

// Verifies UDP loopback start, receive, and stop lifecycle behavior.
void CheckLoopbackLifecycle() {
  std::vector<std::uint8_t> clear;
  AppendString(clear, "/perastage/selection/clear");
  AppendString(clear, ",");
  std::atomic<int> received = 0;
  std::string error;
  perastage::osc::UdpServer server;
  assert(server.Start(
      0,
      [&](std::span<const std::uint8_t> packet) {
        Message incoming;
        std::string parseError;
        if (perastage::osc::ParseMessage(packet, incoming, parseError))
          ++received;
      },
      error));
  assert(server.Port() != 0);
  SendLoopback(server.Port(), clear);
  for (int wait = 0; wait < 100 && received == 0; ++wait)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  assert(received == 1);
  server.Stop();
  assert(server.Port() == 0);
}
} // namespace

// Characterizes OSC parsing, mapping, validation, execution, and lifecycle.
int main() {
  CheckPacketRejection();
  CheckTransformWireForms();
  CheckGroupedRotation();
  CheckMappingValidation();
  CheckSelectionClear();
  CheckLoopbackLifecycle();
  return 0;
}
