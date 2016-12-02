
#include <cmath>
#include <chrono>
#include <vector>
#include <iostream>

#include <caf/all.hpp>
#include <caf/config.hpp>

#include <caf/io/all.hpp>
#include <caf/io/uri.hpp>
#include <caf/io/middleman.hpp>

using namespace std;
using namespace caf;
using namespace std::chrono;

namespace {

using next_atom = atom_constant<atom("next")>;
using quit_atom = atom_constant<atom("quit")>;

using clk = std::chrono::high_resolution_clock;
using tp = std::chrono::time_point<clk>;

class config : public actor_system_config {
public:
  uint16_t port = 0;
  size_t interval = 1000;
  size_t payload_size = 1024;
  size_t num = 6;
  std::string host = "127.0.0.1";
  std::string protocol = "udp";
  std::string uri_str;
  bool server_mode = false;

  config() {
    opt_group{custom_options_, "global"}
    .add(interval, "interval,i", "set interval between messages in us")
    .add(protocol, "protocol,P", "set transport protocol")
    .add(port, "port,p", "set port")
    .add(host, "host,H", "set host (ignored in server mode)")
    .add(uri_str, "uri,u", "set uri (ignores host, port, protocol if set)")
    .add(server_mode, "server-mode,s", "enable server mode")
    .add(payload_size, "payload-size,S", "set payload size (default 1024 byte)")
    .add(num, "num,n", "set n in 10^n for the number of messages sent");
  }
};

struct tracking {
  tp start;
  tp end;
  std::vector<size_t> received;
  size_t last;
};

behavior server(stateful_actor<tracking>* self) {
  return {
    [=](size_t i, const vector<char>& payload) {
      self->state.start = clk::now();
      self->state.received.emplace_back(payload.size());
      self->state.last = i;
      self->become(
        [=](size_t i, const vector<char>& payload) {
          auto& s = self->state;
          s.received.emplace_back(payload.size());
          if (s.last + 1 != i) {
            std::cerr << "Out of order message, expected " << (s.last + 1)
                      << " but received " << i << "." << std::endl;
          }
          s.last = i;
        },
        [=](quit_atom) {
          self->state.end = clk::now();
          auto time =
            duration_cast<microseconds>(self->state.end - self->state.start);
          cout << "Received " << self->state.received.size() << " messages in "
               << time.count() << " microseconds."
               << "(or " << (float(time.count()) / 1000) << " ms)" << std::endl;
          self->quit();
        },
        after(seconds(5)) >> [=] {
          std::cerr << "[TIMEOUT] Received " << self->state.received.size()
                    << " message." << std::endl;
          self->quit();
        }
      );
    },
    [=](quit_atom) {
      std::cerr << "This should NOT have happend." << std::endl;
      self->quit();
    }
  };
}

behavior client(event_based_actor* self, const actor& other, size_t num,
                size_t interval, const vector<char>& payload) {
  return {
    [=](next_atom, size_t i) {
      if (i >= num) {
        self->send(other, quit_atom::value);
        self->quit();
        aout(self) << "Sent " << i << " messages of size " << payload.size()
                   << "." << endl;
      } else {
        self->send(other, i, payload);
        self->delayed_send(self, microseconds(interval), next_atom::value,
                           1 + i);
      }
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  if (cfg.server_mode) {
    auto addr = cfg.uri_str.empty()
                ? cfg.protocol + "://0.0.0.0:" + to_string(cfg.port)
                : cfg.uri_str;
    auto u = io::uri::make(addr);
    if (!u) {
      std::cerr << "Invalid uri: '" << addr << "'." << endl;
      return;
    }
    cout << "Running in server mode." << endl;
    auto srvr = system.spawn(server);
    auto port = system.middleman().publish(srvr, *u);
    std::cout << "Open on port " << port << "." << std::endl;
    system.await_all_actors_done();
  } else {
    auto addr = cfg.uri_str.empty()
                ? cfg.protocol + "://" + cfg.host + ":" + to_string(cfg.port)
                : cfg.uri_str;
    std:: cout << "Contacting server on '" << addr << "'." << std::endl;
    auto u = io::uri::make(addr);
    if (!u) {
      std::cerr << "Invalid uri: '" << addr << "'." << endl;
      return;
    }
    auto srvr = system.middleman().remote_actor(*u);
    if (!srvr) {
      std::cerr << "Failed to contact server on '" << addr << "'." << std::endl;
      return;
    }
    vector<char> payload(cfg.payload_size);
    std::iota(begin(payload), end(payload), char(0));
    auto clnt = system.spawn(client, *srvr, pow(10, cfg.num),
                             cfg.interval, payload);
    anon_send(clnt, next_atom::value, size_t(0));
    system.await_all_actors_done();
  }
}

} // namespace anonymous

CAF_MAIN(io::middleman)
