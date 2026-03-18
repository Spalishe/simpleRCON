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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

using namespace std;

std::vector<char> byte_history;
std::vector<int> clients;

std::mutex data_mutex;

struct IPState {
  int attempts_left = 5;
  bool authorized = false;
  std::chrono::steady_clock::time_point auth_expires;
};

std::map<std::string, IPState> ip_registry;
std::map<std::string, std::chrono::steady_clock::time_point> ban_list;
std::string get_ban_remaining(std::string ip) {
  if (ban_list.count(ip)) {
    auto now = std::chrono::steady_clock::now();
    auto expire = ban_list[ip];

    if (expire > now) {
      auto diff = std::chrono::duration_cast<std::chrono::seconds>(expire - now)
                      .count();
      int mins = diff / 60;
      int secs = diff % 60;
      return std::to_string(mins) + "m " + std::to_string(secs) + "s";
    }
  }
  return "0m 0s";
}
bool is_banned(std::string ip) {
  if (ban_list.count(ip)) {
    if (std::chrono::steady_clock::now() < ban_list[ip]) {
      return true;
    } else {
      ban_list.erase(ip);
    }
  }
  return false;
}

void ban_ip(std::string ip, int minutes) {
  ban_list[ip] =
      std::chrono::steady_clock::now() + std::chrono::minutes(minutes);
  std::cout << "[BANNED] IP " << ip << " for " << minutes << " min."
            << std::endl;
}

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

void handle_client(int client_socket, string pipepath, string client_ip,
                   Json::Value data) {
  bool can_proceed = false;
  auto &state = ip_registry[client_ip];

  if (data["password"].asString().size() == 0)
    can_proceed = true;

  if (!can_proceed) {
    if (state.authorized &&
        std::chrono::steady_clock::now() < state.auth_expires) {
      send(client_socket, "Session restored. Welcome!\n", 27, MSG_NOSIGNAL);
      can_proceed = true;
    } else {
      state.authorized = false;

      while (state.attempts_left > 0) {
        send(client_socket, "Enter Password: ", 16, MSG_NOSIGNAL);

        char pass_buf[256];
        int bytes = recv(client_socket, pass_buf, sizeof(pass_buf) - 1, 0);
        if (bytes <= 0) {
          close(client_socket);
          return;
        }

        pass_buf[bytes] = '\0';
        std::string input(pass_buf);
        input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());
        input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());

        if (input == data["password"].asString()) {
          state.attempts_left = 5;
          state.authorized = true;
          state.auth_expires =
              std::chrono::steady_clock::now() + std::chrono::minutes(15);
          send(client_socket, "Access Granted!\n", 16, MSG_NOSIGNAL);
          can_proceed = true;
          break;
        } else {
          state.attempts_left--;
          std::string msg =
              "Wrong! Remaining: " + std::to_string(state.attempts_left) + "\n";
          send(client_socket, msg.c_str(), msg.length(), MSG_NOSIGNAL);
        }
      }

      if (!can_proceed) {
        ban_list[client_ip] =
            std::chrono::steady_clock::now() + std::chrono::minutes(15);
        state.attempts_left = 5; // Сброс для будущего входа
        send(client_socket, "Too many attempts. Banned for 15 min.\n", 38,
             MSG_NOSIGNAL);
        close(client_socket);
        return;
      }
    }
  }

  if (can_proceed) {
    send_in_chunks(client_socket, byte_history.data(), byte_history.size());

    clients.push_back(client_socket);
    while (true) {
      char buffer[1024];
      int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
      if (bytes_read > 0) {
        ofstream pipe(pipepath);
        if (pipe.is_open()) {
          pipe << buffer;
          pipe.flush();
          pipe.close();
        }
      } else if (bytes_read < 0) {
        cout << "Client " << client_socket << " disconnected" << endl;
        break;
      }
    }
    clients.erase(std::remove(clients.begin(), clients.end(), client_socket),
                  clients.end());
  }
  close(client_socket);
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
  parser.addArgument("init", "Creates basic configuration file", false, false,
                     Argparser::ArgumentType::def);

  parser.addArgument("--conf", "Configuration file path", false, false,
                     Argparser::ArgumentType::str);
  parser.addArgument("--inpipe", "STDIN Pipe path", false, false,
                     Argparser::ArgumentType::str);
  parser.parse();

  if (parser.getDefined(0)) {
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

  if (!parser.getDefined(1)) {
    cerr << "Specify configuration path!" << endl;
    return 1;
  }

  string path = parser.getString(1);
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
    if (is_banned(client_ip)) {
      std::cout << "[RCON] Banned IP tried to connect: " << client_ip
                << std::endl;
      string str = "Your ban expires in " + get_ban_remaining(client_ip);
      send(client_socket, str.data(), str.size(), 0);
      close(client_socket);
      continue;
    }

    std::thread(handle_client, client_socket,
                (parser.getDefined(2) ? parser.getString(2) : ""), client_ip,
                data)
        .detach();
  }
  if (reader_thread.joinable())
    reader_thread.join();
  return 0;
}
