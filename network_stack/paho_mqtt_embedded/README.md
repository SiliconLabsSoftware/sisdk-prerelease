# netstack-paho-mqtt-embedded

Paho MQTT C/C++ client for embedded platforms.

## Silicon Labs maintenance scope

Silicon Labs maintains the platform port and the integration glue required to run
the client on SiWx91x devices. Specifically:

- `MQTTClient-C/src/Si91x/` — the Si91x network transport and client port:
  - `MQTTSi91x.[ch]` — sockets-based transport over the Si91x offload stack
  - `MQTTSi91x_lwip.[ch]` — transport over the LwIP network stack
  - `MQTTSi91x_dual_stack.[ch]` — dual-stack (offload + LwIP) transport
  - `sli_lwip_mqtt_mbedtls_config.h` — mbedTLS configuration for secure MQTT
  - `unit_tests/` and `unit_tests_lwip/` — host-based unit tests for the port

## Documentation

This repository contains the source code for the [Eclipse Paho](http://eclipse.org/paho) MQTT C/C++ client library for Embedded platorms.

There are three sub-projects:

1. MQTTPacket - simple de/serialization of MQTT packets, plus helper functions
2. MQTTClient - high(er) level C++ client, plus
3. MQTTClient-C - high(er) level C client (pretty much a clone of the C++ client)

The *MQTTPacket* directory contains the lowest level C library with the smallest requirements.  This supplies simple serialization
and deserialization routines.  They serve as a base for the higher level libraries, but can also be used on their own
It is mainly up to you to write and read to and from the network.

The *MQTTClient* directory contains the next level C++ library.  This networking code is contained in separate classes so that you can plugin the
network of your choice.  Currently there are implementations for Linux, Arduino and mbed.  ARM mbed was the first platform for which this was written,
where the conventional language choice is C++, which explains the language choice.  I have written a starter [Porting Guide](http://modelbasedtesting.co.uk/2014/08/25/porting-a-paho-embedded-c-client/).

The *MQTTClient-C* directory contains a C equivalent of MQTTClient, for those platforms where C++ is not supported or the convention.  As far
as possible it is a direct translation from *MQTTClient*.

## Build requirements / compilation

CMake builds for the various packages have been introduced, along with Travis-CI configuration for automated build & testing.  The basic
method of building on Linux is:

```
mkdir build.paho
cd build.paho
cmake ..
make
```

The travis-build.sh file has the full build and test sequence for Linux.


## Usage and API

See the samples directories for examples of intended use.  Doxygen config files for each package are available in the doc directory.

## Runtime tracing

The *MQTTClient* API has debug tracing for MQTT packets sent and received - turn this on by setting the MQTT_DEBUG preprocessor definition.


## Reporting bugs

This project uses GitHub Issues here: [github.com/eclipse/paho.mqtt.embedded-c/issues](https://github.com/eclipse/paho.mqtt.embedded-c/issues) to track ongoing development and issues.

## More information

Discussion of the Paho clients takes place on the [Eclipse Mattermost Paho channel](https://mattermost.eclipse.org/eclipse/channels/paho) and the [Eclipse paho-dev mailing list](https://dev.eclipse.org/mailman/listinfo/paho-dev).

General questions about the MQTT protocol are discussed in the [MQTT Google Group](https://groups.google.com/forum/?hl=en-US&fromgroups#!forum/mqtt).

More information is available via the [MQTT community](http://mqtt.org).
