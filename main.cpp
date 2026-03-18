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
      cout << "Failed to create configuration file! strerror: "
           << strerror(errno) << endl;
      return 1;
    }
    conf << "{\n  \"port\": 24454,\n  \"password\": \"qwerty\"\n}";
    conf.close();
    return 0;
  }

  if (!parser.getDefined(0)) {
    cout << "Specify configuration path!" << endl;
    return 1;
  }

  string path = parser.getString(0);
  ifstream conf(path, std::ifstream::binary);

  if (!conf.is_open()) {
    cout << "Failed to open configuration file! strerror: " << strerror(errno)
         << endl;
    return 1;
  }

  Json::CharReaderBuilder readerBuilder;
  std::string errs;
  Json::Value data;

  if (!Json::parseFromStream(readerBuilder, conf, &data, &errs)) {
    std::cerr << "Error parsing JSON: " << errs << std::endl;
    return 1;
  }
  conf.close();
  cout << data["password"] << endl;

  return 0;
}
