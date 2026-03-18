#include "include/argparser.h"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[]) {
  Argparser::Argparser parser(argc, argv);
  parser.addArgument("--ip", "Host IPv4 address");
  parser.addArgument("--port", "Host port", true, false,
                     Argparser::ArgumentType::num);

  parser.parse();

  int client = socket(AF_INET, SOCK_STREAM, 0);
  if (client < 0) {
    cerr << "Socket creation error" << endl;
    return -1;
  }
  sockaddr_in address{AF_INET, htons(parser.getDefined(1)), INADDR_ANY};

  if (inet_pton(AF_INET, parser.getString(0).c_str(), &address.sin_addr) <= 0) {
    cerr << "Invalid host IPv4" << endl;
    return -1;
  }

  return 0;
}
