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

// Builds the supported explicit transform message.
std::vector<std::uint8_t> TransformPacket(const std::string &address,
                                          float value) {
  std::vector<std::uint8_t> packet;
  AppendString(packet, address);
  AppendString(packet, ",sfTsF");
  AppendString(packet, "x");
  AppendWord(packet, std::bit_cast<std::uint32_t>(value));
  AppendString(packet, "world");
  return packet;
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
} // namespace

// Verifies parsing, rejection, semantic mapping, and loopback lifecycle.
int main() {
  using namespace perastage;
  osc::Message message;
  std::string error;
  const auto position =
      TransformPacket("/perastage/transform/position", 1250.0f);
  assert(osc::ParseMessage(position, message, error));
  assert(message.address == "/perastage/transform/position");
  assert(message.arguments.size() == 5);

  auto malformed = position;
  malformed.pop_back();
  assert(!osc::ParseMessage(malformed, message, error));
  assert(error == "invalid_packet_size");
  auto invalidPadding = position;
  invalidPadding[invalidPadding.size() - 1] = 1;
  assert(!osc::ParseMessage(invalidPadding, message, error));
  std::vector<std::uint8_t> bundle;
  AppendString(bundle, "#bundle");
  AppendString(bundle, ",");
  assert(!osc::ParseMessage(bundle, message, error));
  assert(error == "bundles_unsupported");
  std::vector<std::uint8_t> unsupported;
  AppendString(unsupported, "/perastage/test");
  AppendString(unsupported, ",b");
  assert(!osc::ParseMessage(unsupported, message, error));
  assert(error == "unsupported_type_tag");
  std::vector<std::uint8_t> oversized(osc::kMaximumPacketBytes + 4);
  assert(!osc::ParseMessage(oversized, message, error));
  assert(error == "invalid_packet_size");

  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.transform = MatrixUtils::Identity();
  scene.fixtures.emplace(fixture.uuid, fixture);
  scene_grouping::ObjectSelection selection;
  selection.fixtures = {fixture.uuid};
  RecordingHost host;
  command::ExecutionContext context{scene, selection, host};
  scene_grouping::InteractiveTransformPolicy policy;
  assert(osc::ParseMessage(position, message, error));
  auto result = osc::ExecuteMessage(message, context, policy);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(host.publications == 1);
  assert(std::fabs(scene.fixtures.at(fixture.uuid).transform.o[0] - 1250.0f) <
         0.01f);

  const auto rotation = TransformPacket("/perastage/transform/rotation", 45.0f);
  assert(osc::ParseMessage(rotation, message, error));
  result = osc::ExecuteMessage(message, context, policy);
  assert(result.Success() &&
         result.request->commandId == command::transform::kRotationCommandId);
  assert(host.publications == 2);

  std::vector<std::uint8_t> clear;
  AppendString(clear, "/perastage/selection/clear");
  AppendString(clear, ",");
  assert(osc::ParseMessage(clear, message, error));
  result = osc::ExecuteMessage(message, context, policy);
  assert(result.Success() && selection.fixtures.empty());
  assert(host.publications == 3);
  message.address = "/perastage/unknown";
  result = osc::ExecuteMessage(message, context, policy);
  assert(!result.Success());
  assert(result.diagnostics.front().code == "osc.unsupported_address");

  std::atomic<int> received = 0;
  osc::UdpServer server;
  assert(server.Start(
      0,
      [&](std::span<const std::uint8_t> packet) {
        osc::Message incoming;
        std::string parseError;
        if (osc::ParseMessage(packet, incoming, parseError))
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
  return 0;
}
