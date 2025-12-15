#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string host{"127.0.0.1"};
    int port{8080};
};

void printUsage() {
    std::cout << "Usage:\n"
              << "  commandlineclient list-cues [--host HOST] [--port PORT]\n"
              << "  commandlineclient play-cue <cueId> [--host HOST] [--port PORT]\n"
              << "  commandlineclient help\n";
}

Options parseOptions(int& argc, char* argv[]) {
    Options opts{};
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--host" && i + 1 < argc) {
            opts.host = argv[++i];
        } else if (arg.rfind("--host=", 0) == 0) {
            opts.host = arg.substr(7);
        } else if (arg == "--port" && i + 1 < argc) {
            opts.port = std::stoi(argv[++i]);
        } else if (arg.rfind("--port=", 0) == 0) {
            opts.port = std::stoi(arg.substr(7));
        } else {
            // Keep positional args intact
            continue;
        }
        // Remove processed args from argc to simplify command parsing
        argv[i] = nullptr;
    }
    return opts;
}

httplib::Client makeClient(const Options& opts) {
    httplib::Client cli(opts.host, opts.port);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);
    return cli;
}

int listCues(const Options& opts) {
    auto cli = makeClient(opts);
    if (auto res = cli.Get("/cues")) {
        if (res->status == 200) {
            auto body = nlohmann::json::parse(res->body);
            std::cout << "Cues:\n";
            for (const auto& cue : body) {
                std::cout << "- cueId=" << cue.value("id", "") << " name=\"" << cue.value("name", "")
                          << "\" sceneId=" << cue.value("sceneId", "") << "\n";
            }
            return 0;
        }
        std::cerr << "Server responded " << res->status << " for /cues\n";
    } else {
        std::cerr << "Failed to reach server at " << opts.host << ":" << opts.port << ", error=" << res.error() << "\n";
        return 1;
    }
    return 1;
}

int playCue(const Options& opts, const std::string& cueId) {
    auto cli = makeClient(opts);

    nlohmann::json payload{{"cueId", cueId}};
    if (auto res = cli.Post("/renderer/playCue", "application/json", payload.dump())) {
        if (res->status == 200) {
            std::cout << "Requested playCue for cueId=" << cueId << "\n";
            return 0;
        }
        std::cerr << "playCue responded " << res->status << ", response: " << res->body << "\n";
    }

    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    Options opts = parseOptions(argc, argv);
    std::string command = argv[1] ? argv[1] : "";

    if (command == "help") {
        printUsage();
        return 0;
    }

    if (command == "list-cues") {
        return listCues(opts);
    }

    if (command == "play-cue") {
        if (argc < 3 || argv[2] == nullptr) {
            std::cerr << "play-cue requires <cueId>\n";
            printUsage();
            return 1;
        }
        return playCue(opts, argv[2]);
    }

    printUsage();
    return 1;
}
