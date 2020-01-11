/***********************************************
HAYESDUINO PROJECT - COPYRIGHT 2013, PAYTON BYRD

Project homepage: http://hayesduino.codeplex.com
License: http://hayesduino.codeplex.com/license
***********************************************/
#include "utility/w5100.h"
#include "utility/socket.h"
extern "C" {
#include "string.h"
}

#include "EthernetClient.h"
#include "EthernetServer.h"
#include "Ethernet.h"

EthernetServer::EthernetServer(uint16_t port)
{
  _port = port;
}

void EthernetServer::begin()
{
  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);
    if (client.status() == SnSR::CLOSED) {
      socket(sock, SnMR::TCP, _port, 0);
      listen(sock);
      EthernetClass::_server_port[sock] = _port;
      break;
    }
  }  
}

EthernetClient EthernetServer::connected(uint8_t sock)
{
    accept();

    EthernetClient client(sock);
    if (EthernetClass::_server_port[sock] == _port &&
         (client.status() == SnSR::ESTABLISHED ||
         (client.status() == SnSR::CLOSE_WAIT && client.available()))) {
      return client;
    }

    return EthernetClient(MAX_SOCK_NUM);
}

EthernetClient EthernetServer::connected()
{
  accept();
  int rand = micros() % MAX_SOCK_NUM;

  for (int s = 0; s < MAX_SOCK_NUM; s++) {
    int sock = (s + rand) % MAX_SOCK_NUM;
    EthernetClient client(sock);
    if (EthernetClass::_server_port[sock] == _port &&
         (client.status() == SnSR::ESTABLISHED ||
         (client.status() == SnSR::CLOSE_WAIT && client.available()))) {
      return client;
    }
  }

  return EthernetClient(MAX_SOCK_NUM);
}

void EthernetServer::accept()
{
  int listening = 0;

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);

    if (EthernetClass::_server_port[sock] == _port) {
      if (client.status() == SnSR::LISTEN) {
        listening = 1;
      } 
      else if (client.status() == SnSR::CLOSE_WAIT && !client.available()) {
        client.stop();
      }
    } 
  }

  if (!listening) {
    begin();
  }
}

EthernetClient EthernetServer::available()
{
  accept();

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);
    if (EthernetClass::_server_port[sock] == _port &&
        (client.status() == SnSR::ESTABLISHED ||
         client.status() == SnSR::CLOSE_WAIT)) {
      if (client.available()) {
        // XXX: don't always pick the lowest numbered socket.
        return client;
      }
    }
  }

  return EthernetClient(MAX_SOCK_NUM);
}

size_t EthernetServer::write(uint8_t b) 
{
  return write(&b, 1);
}

size_t EthernetServer::write(const uint8_t *buffer, size_t size) 
{
  size_t n = 0;
  
  accept();

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);

    if (EthernetClass::_server_port[sock] == _port &&
      client.status() == SnSR::ESTABLISHED) {
      n += client.write(buffer, size);
    }
  }
  
  return n;
}
