# caf-poke
Test some datagram functionality in CAF

```
Application Options:
  -i [--interval] arg                                : set interval between messages in us
  -P [--protocol] arg                                : set transport protocol
  -p [--port] arg                                    : set port
  -H [--host] arg                                    : set host (ignored in server mode)
  -u [--uri] arg                                     : set uri (ignores host, port, protocol if set)
  -s [--server-mode]                                 : enable server mode
  -S [--payload-size] arg                            : set payload size (default 1024 byte)
  -n [--num] arg                                     : set n in 10^n for the number of messages sent
  -h [-?,--help]                                     : print this text
```

## Build

```
./configure --with-caf=/PATH/TO/actor-framework/build/
make
```

## Message loss (local link)

server

```
./build/bin/poke -s -p 4242 -P udp
```

client

```
./build/bin/poke -i 1 -p 4242 -P udp -n 5 -S 100
```

Starting the client with `100` sometime leads to message loss, while `400` consistently looses messages.
