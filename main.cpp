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
#include <json/json.h>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

using namespace std;

std::vector<char> byte_history;
std::vector<int> clients;

std::mutex data_mutex;

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
  while (true) {
    char buffer[1024];
    int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_read > 0) {
      send(client_socket, "Hello from server", 17, 0);
    } else if (bytes_read < 0) {
      cout << "Client " << client_socket << " disconnected" << endl;
      break;
    }
  }
  clients.erase(std::remove(clients.begin(), clients.end(), client_socket),
                clients.end());
  close(client_socket);
}

void broadcast_to_clients(void *buffer, ssize_t len) {
  std::lock_guard<std::mutex> lock(data_mutex);
  for (auto it = clients.begin(); it != clients.end();) {
    int fd = *it;
    ssize_t res = send(fd, buffer, len, 0);

    if (res < 0) {
      std::cout << "Client " << fd << " disconnected" << std::endl;
      close(fd);
      it = clients.erase(it);
    } else {
      ++it;
    }
  }
}
void send_to_client(int fd, void *buffer, ssize_t len) {
  ssize_t res = send(fd, buffer, len, 0);

  if (res < 0) {
    std::cout << "Client " << fd << " disconnected" << std::endl;
    close(fd);
    clients.erase(std::remove(clients.begin(), clients.end(), fd),
                  clients.end());
  }
}

void send_in_chunks(int fd, void *data, size_t total_size) {
  const size_t CHUNK_SIZE = 1024;
  unsigned char *ptr = (unsigned char *)data;
  size_t bytes_sent = 0;

  while (bytes_sent < total_size) {
    size_t current_chunk = total_size - bytes_sent;
    if (current_chunk > CHUNK_SIZE) {
      current_chunk = CHUNK_SIZE;
    }

    send_to_client(fd, ptr + bytes_sent, current_chunk);

    bytes_sent += current_chunk;
  }
}

void process_piping() {
  char b;
  while (read(STDIN_FILENO, &b, 1) > 0) {
    write(STDOUT_FILENO, &b, 1);
    byte_history.push_back(b);
    broadcast_to_clients(&b, 1);
  }
  cout << "[RCON] Process closed, closing all connections and leaving..."
       << endl;
  for (auto &fd : clients) {
    close(fd);
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(NULL);
  setvbuf(stdin, NULL, _IONBF, 0);

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
    cerr << "[RCON] Error parsing JSON: " << errs << endl;
    return 1;
  }
  conf.close();

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(data["port"].asInt());
  addr.sin_addr.s_addr = INADDR_ANY;

  int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    cerr << "[RCON] Socket creation error! " << strerror(errno) << endl;
    return 1;
  }
  int op = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));
  if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    cerr << "[RCON] Bind failed! " << strerror(errno) << endl;
    return 1;
  }
  listen(server, 10);
  std::thread reader_thread(process_piping);
  while (true) {
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_socket = accept(server, (sockaddr *)&client_addr, &addr_len);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    int client_port = ntohs(client_addr.sin_port);

    cout << "[RCON] Got new client at " << client_ip << ":" << client_port
         << " with client FD: " << client_socket << endl;

    send_in_chunks(client_socket, byte_history.data(), byte_history.size());

    clients.push_back(client_socket);
    std::thread(handle_client, client_socket).detach();
  }
  if (reader_thread.joinable())
    reader_thread.join();
  return 0;
}
