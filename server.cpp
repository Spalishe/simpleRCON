#include "include/argparser.h"
#include <algorithm>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <json/forwards.h>
#include <ostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include <json/json.h>

using namespace std;

string random_string(size_t length) {
  auto randchar = []() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  string str(length, 0);
  generate_n(str.begin(), length, randchar);
  return str;
}

void handle_client(int client_socket) {
  char buffer[1024];
  int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
  if (bytes_read > 0) {
    send(client_socket, "Hello from server", 17, 0);
  }
  close(client_socket);
}

int main(int argc, char *argv[]) {
  Argparser::Argparser parser(argc, argv);
  parser.setProgramName("Simple RCON");
  parser.addArgument("--conf", "Configuration file path", false, false,
                     Argparser::ArgumentType::str);
  parser.addArgument("init", "Creates basic configuration file", false, false,
                     Argparser::ArgumentType::def);

  parser.parse();

  if (parser.getDefined(1)) {
    cout << "Creating configuration file..." << endl;
    ofstream conf("rcon.conf");
    if (!conf.is_open()) {
      cerr << "Failed to create configuration file! strerror: "
           << strerror(errno) << endl;
      return 1;
    }
    conf << "{\n  \"port\": 24454,\n  \"password\": \"qwerty\"\n}";
    conf.close();
    return 0;
  }

  if (!parser.getDefined(0)) {
    cerr << "Specify configuration path!" << endl;
    return 1;
  }

  string path = parser.getString(0);
  ifstream conf(path, std::ifstream::binary);

  if (!conf.is_open()) {
    cerr << "Failed to open configuration file! strerror: " << strerror(errno)
         << endl;
    return 1;
  }

  Json::CharReaderBuilder readerBuilder;
  std::string errs;
  Json::Value data;

  if (!Json::parseFromStream(readerBuilder, conf, &data, &errs)) {
    cerr << "Error parsing JSON: " << errs << endl;
    return 1;
  }
  conf.close();

  sockaddr_in addr{AF_INET, htons(data["port"].asInt()), INADDR_ANY};

  int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    cerr << "Socket creation error!" << endl;
    return 1;
  }
  int op = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));
  if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    cerr << "Bind failed!" << endl;
    return 1;
  }
  listen(server, 10);
  while (true) {
    int client_socket = accept(server, nullptr, nullptr);
    std::thread(handle_client, client_socket).detach();
  }

  return 0;
}
