#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <netinet/in.h>
#include <optional>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <string>
#include <iostream>

#include "net/RendererServer.h"

using projection::core::RendererMessage;
using projection::core::RendererMessageType;
using projection::renderer::makeAckMessage;
using projection::renderer::makeErrorMessage;
using projection::renderer::parseRendererMessageLine;
using projection::renderer::renderRendererMessageLine;

namespace {
RendererMessage sampleHelloMessage() {
  RendererMessage message{};
  message.type = RendererMessageType::Hello;
  message.commandId = "cmd-1";
  message.hello = projection::core::HelloMessage{"1.0.0", "renderer", "stage-left"};
  return message;
}

class RecordingHandler : public projection::renderer::RendererCommandHandler {
 public:
  void handle(const RendererMessage& message) override { lastMessage = message; }

  std::optional<RendererMessage> lastMessage;
};
}

TEST_CASE("parseRendererMessageLine parses valid JSON", "[renderer]") {
  auto message = sampleHelloMessage();
  auto line = renderRendererMessageLine(message);
  auto parsed = parseRendererMessageLine(line);

  REQUIRE(parsed.type == RendererMessageType::Hello);
  REQUIRE(parsed.commandId == "cmd-1");
  REQUIRE(parsed.hello);
  REQUIRE(parsed.hello->version == "1.0.0");
  REQUIRE(parsed.hello->role == "renderer");
  REQUIRE(parsed.hello->name == "stage-left");
}

TEST_CASE("parseRendererMessageLine throws on invalid JSON", "[renderer]") {
  bool threw = false;
  try {
    parseRendererMessageLine("not-json");
  } catch (...) {
    threw = true;
  }
  REQUIRE(threw);
}

TEST_CASE("makeAckMessage populates payload", "[renderer]") {
  auto ack = makeAckMessage("abc");
  REQUIRE(ack.type == RendererMessageType::Ack);
  REQUIRE(ack.ack);
  REQUIRE(ack.ack->commandId == "abc");
}

TEST_CASE("makeErrorMessage populates payload", "[renderer]") {
  auto error = makeErrorMessage("command-x", "boom");
  REQUIRE(error.type == RendererMessageType::Error);
  REQUIRE(error.error);
  REQUIRE(error.error->commandId == "command-x");
  REQUIRE(error.error->message == "boom");
}

TEST_CASE("RendererServer accepts a message and responds with ack", "[renderer]") {
  RecordingHandler handler;
  projection::renderer::RendererServer server(handler);
  server.start(0);

  int port = 0;
  for (int i = 0; i < 50; ++i) {
    port = server.port();
    if (port != 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (port == 0) {
    const auto error = server.lastError();
    if (error.find("Operation not permitted") != std::string::npos) {
      std::cerr << "Skipping socket test: " << error << std::endl;
      server.stop();
      return;
    }
  }
  REQUIRE(port != 0);

  int client = socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(client >= 0);

  sockaddr_in addr{};
#ifdef __APPLE__
  addr.sin_len = sizeof(addr);
#endif
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  REQUIRE(connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

  auto message = sampleHelloMessage();
  std::string payload = renderRendererMessageLine(message) + "\n";
  REQUIRE(send(client, payload.c_str(), payload.size(), 0) == static_cast<ssize_t>(payload.size()));

  char buffer[512];
  ssize_t received = recv(client, buffer, sizeof(buffer) - 1, 0);
  REQUIRE(received > 0);
  buffer[received] = '\0';

  std::string response(buffer);
  auto newlinePos = response.find('\n');
  if (newlinePos != std::string::npos) {
    response = response.substr(0, newlinePos);
  }
  auto ack = parseRendererMessageLine(response);
  REQUIRE(ack.type == RendererMessageType::Ack);
  REQUIRE(ack.ack);
  REQUIRE(ack.ack->commandId == "cmd-1");

  server.stop();
  close(client);
}
