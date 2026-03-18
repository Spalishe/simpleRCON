#include "include/argparser.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include <netinet/tcp.h>

using namespace std;

void receive_thread(int sock) {
  while (true) {
    char buffer[1024] = {0};
    int valread = recv(sock, buffer, 1024, 0);
    if (valread > 0) {
      cout << buffer;
    } else if (valread < 0) {
      cerr << "Connection reset" << endl;
      fflush(stderr);
      fflush(stdout);
      exit(-1);
    }
  }
}

int main(int argc, char *argv[]) {
  Argparser::Argparser parser(argc, argv);
  parser.addArgument("--ip", "Host IPv4 address", true, false,
                     Argparser::ArgumentType::str);
  parser.addArgument("--port", "Host port", true, false,
                     Argparser::ArgumentType::num);

  parser.parse();

  int client = socket(AF_INET, SOCK_STREAM, 0);
  if (client < 0) {
    cerr << "Socket creation error. " << strerror(errno) << endl;
    return -1;
  }
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(parser.getNumber(1));

  int keepalive = 1;
  int keepidle = 10;
  int keepintvl = 3;
  int keepcnt = 2;
  setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
  setsockopt(client, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
  setsockopt(client, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
  setsockopt(client, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

  struct timeval tv;
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

  if (inet_pton(AF_INET, parser.getString(0).c_str(), &address.sin_addr) <= 0) {
    cerr << "Invalid host IPv4" << endl;
    return -1;
  }

  if (connect(client, (struct sockaddr *)&address, sizeof(address)) < 0) {
    cerr << "Connection error. " << strerror(errno) << endl;
    return -1;
  }

  bool canRead = true;

  thread(receive_thread, client).detach();

  cout << "Connected." << endl;
  while (true) {
    std::string message;
    std::getline(std::cin, message);
    if (message == "exit")
      break;

    ssize_t result = send(client, message.c_str(), message.length(), 0);
    if (result < 0) {
      cerr << "Connection reset" << endl;
      fflush(stderr);
      fflush(stdout);
      exit(-1);
    }
  }
  return 0;
}
