
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
using tcp_atom = atom_constant<atom("tcp")>;
using udp_atom = atom_constant<atom("udp")>;

using clk = std::chrono::high_resolution_clock;
using tp = std::chrono::time_point<clk>;

class config : public actor_system_config {
public:
  // client
  std::string host = "127.0.0.1";
  std::string protocol = "udp";
  uint16_t port = 4321;
  std::string uri_str;
  size_t num = 6;
  // server
  size_t interval = 100;
  bool server_mode = false;
  uint16_t tcp = 1234;
  uint16_t udp = 4321;

  config() {
    opt_group{custom_options_, "client"}
    .add(protocol, "protocol,P", "set transport protocol (udp)")
    .add(port, "port,p", "set port (4321)")
    .add(host, "host,H", "set host (127.0.0.1)")
    .add(uri_str, "uri,U", "set uri (ignores host, port, protocol if set)")
    .add(interval, "interval,i", "set interval between messages in ms (100)")
    .add(num, "num,n", "sends 10^n messages (10^6)");
    opt_group{custom_options_, "server"}
    .add(server_mode, "server-mode,s", "enable server mode")
    .add(tcp, "tcp,t", "set TCP port (1234)")
    .add(udp, "udp,u", "set UDP port (4321)");
  }
};

size_t handle(event_based_actor* self, atom_value atm, size_t val) {
  if (val == 0) {
    aout(self) << "[" << to_string(atm) << "] From zero ..." << std::endl;
  } else if (val % 1000 == 0) {
    aout(self) << "[" << to_string(atm) << "] Incrementing " << val
               << "." << std::endl;
  }
  return val + 1;
}

behavior server(event_based_actor* self) {
  return {
    [=](tcp_atom atm, size_t i) -> size_t {
      return handle(self, atm, i);
    },
    [=](udp_atom atm, size_t i) -> size_t {
      return handle(self, atm, i);
    }
  };
}

behavior client(event_based_actor* self, const actor& other,
                size_t limit, size_t interval, atom_value atm) {
  return {
    [=](next_atom, size_t i) {
      self->send(other, atm, i);
    },
    [=](size_t i) {
      if (i >= limit) {
        std::cerr << "DONE" << std::endl;
        self->quit();
      } else {
        self->delayed_send(self, milliseconds(interval), next_atom::value, i);
      }
    },
    after(chrono::milliseconds(5 * interval)) >> [=] {
      std::cerr << "RESTARTING" << std::endl;
      self->send(self, next_atom::value, size_t(0));
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  if (cfg.server_mode) {
    cout << "Running in server mode." << endl;
    auto tcp_addr = "tcp://0.0.0.0:" + to_string(cfg.tcp);
    auto udp_addr = "udp://0.0.0.0:" + to_string(cfg.udp);
    auto tcp_uri = io::uri::make(tcp_addr);
    auto udp_uri = io::uri::make(udp_addr);
    if (!tcp_uri) {
      std::cerr << "Invalid uri: '" << tcp_addr << "'." << endl;
      return;
    }
    if (!udp_uri) {
      std::cerr << "Invalid uri: '" << udp_addr << "'." << endl;
      return;
    }
    auto srvr = system.spawn(server);
    auto port = system.middleman().publish(srvr, *tcp_uri);
    std::cout << "Open on TCP port " << port << "." << std::endl;
    port = system.middleman().publish(srvr, *udp_uri);
    std::cout << "Open on UDP port " << port << "." << std::endl;
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
    auto atm = atom_from_string(caf::io::to_string(u->scheme()));
    auto clnt = system.spawn(client, *srvr, pow(10, cfg.num), cfg.interval, atm);
    anon_send(clnt, next_atom::value, size_t(0));
    system.await_all_actors_done();
  }
}

} // namespace anonymous

CAF_MAIN(io::middleman)
