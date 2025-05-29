#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#define PORT 8080
#define ROUND_TIMEOUT 5.0
#define CLIENT_TIMEOUT_SEC 10.0
#define ROUNDS_NB 20

int server_socket;
bool isRunning = true;
bool isGameRunning = false;
std::mutex choices_mtx;

enum GameChoice { SPLIT, GRAB };

void log_out_main(std::string msg) {
  std::cout << "[Server Main] " << msg << std::endl;
}

void log_out_cmd(std::string msg) {
  std::cout << "[Server Admin Commands] " << msg << std::endl;
}

void log_err(std::string msg) {
  std::cerr << "[Server] " << msg << std::endl;
  exit(0);
}

void log_out_game(std::string msg) {
  std::cout << "[Server Game] " << msg << std::endl;
}

void handle_signal(int sig) {
  log_out_main("Gracefully handle signal " + std::to_string(sig));
  close(server_socket);
  isRunning = false;
}

struct Client {
  std::string name;
  sockaddr_in addr;
  time_t lastSeen;
  bool isActive;

  static std::string format_addr(sockaddr_in &addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    std::string formatted_addr =
        std::string(client_ip) + ':' + std::to_string(htons(addr.sin_port));

    return formatted_addr;
  }

  std::string format() {
    return "[Name: " + name + ", at address: " + format_addr(addr) + "]";
  }

  bool send(std::string msg) {
    return sendto(server_socket, msg.c_str(), msg.size(), 0, (sockaddr *)&addr,
                  sizeof(addr)) >= 0;
  }

  bool ask_for_game_choice() { return send("CHOOSE"); }
};
class ClientManager {
  struct Player {
    std::string client;
    std::string name;
    int points;
    std::optional<GameChoice> lastChoice;

    std::string format() {
      return std::string(name + " with " + std::to_string(points) + " points");
    }
  };

public:
  void send_to_all_clients(std::string msg) {
    for (auto &[addr, client] : clients) {
      if (client.isActive) {
        client.send(msg);
      }
    }
  }
  std::string get_name(std::string addr) { return clients[addr].name; }
  void print_info() {
    std::vector<Client> activeClients;
    std::vector<Client> inactiveClients;

    for (auto &[addr, client] : clients) {
      if (client.isActive) {
        activeClients.push_back(client);
      } else {
        inactiveClients.push_back(client);
      }
    }

    std::cout << "[Info]\nActive clients (" << activeClients.size() << ") :\n";
    for (auto client : activeClients) {
      std::cout << client.format() << std::endl;
      ;
    }

    std::cout << "Inactive clients (" << inactiveClients.size() << ") :\n";
    for (auto client : inactiveClients) {
      std::cout << client.format() << std::endl;
    }
  }
  void add(Client &client) {
    log_out_main("New client " + client.format() + " has been added");
    clients.insert({client.format_addr(client.addr), client});
  }

  void handle_ping(std::string addr) {
    Client &client = clients[addr];
    client.lastSeen = time(nullptr);

    if (!client.isActive) {
      log_out_main("Client " + client.format() + " was reconnected");
      client.isActive = true;
    }
  }

  void update_clients(time_t &cur_time) {
    for (auto it : clients) {
      bool wasActive = it.second.isActive;
      it.second.isActive = difftime(cur_time, it.second.lastSeen);
      if (!(it.second.isActive =
                difftime(cur_time, it.second.lastSeen) < timeout_sec) &&
          wasActive) {
        log_out_main("Client " + it.second.format() + " was disconnected");
      }
    }
  }

  void start_game() {
    if (isGameRunning)
      return;

    isGameRunning = true;
    int nbPlayers = clients.size();
    players.clear();
    players.reserve(nbPlayers);

    std::transform(clients.begin(), clients.end(), std::back_inserter(players),
                   [](const std::pair<std::string, Client> &p) {
                     return Player{p.first, p.second.name, 0, std::nullopt};
                   });

    for (int i = 0; i < nbPlayers; ++i) {
      for (int j = i + 1; j < nbPlayers; ++j) {
        std::pair<Player &, Player &> players_pair{players[i], players[j]};
        run_game_between(players_pair);
      }
    }

    log_out_main("Game has ended:");
    std::sort(players.begin(), players.end(),
              [](const Player &left, const Player &right) {
                return left.points > right.points;
              });
    for (auto player : players) {
      log_out_main(player.format());
    }
  }

  bool answer(std::string playerKey, GameChoice answer) {
    std::lock_guard<std::mutex> lock(choices_mtx);
    for (auto &player : players) {
      if (player.client == playerKey) {
        if (player.lastChoice != std::nullopt)
          return false;
        player.lastChoice = std::optional<GameChoice>(answer);
        return true;
      }
    }
    return false;
  }

private:
  void run_game_between(std::pair<Player &, Player &> &players) {
    std::pair<int, int> startPoints{players.first.points,
                                    players.second.points};
    for (int i = 0; i < ROUNDS_NB; ++i) {
      clients[players.first.client].ask_for_game_choice();
      clients[players.second.client].ask_for_game_choice();
      time_t start_round_time = time(nullptr);
      bool roundSuccess = false;

      while (!roundSuccess &&
             difftime(time(nullptr), start_round_time) < ROUND_TIMEOUT) {
        if (players.first.lastChoice == std::nullopt ||
            players.second.lastChoice == std::nullopt)
          continue;

        std::lock_guard<std::mutex> lock(choices_mtx);
        std::pair<GameChoice, GameChoice> choices(*players.first.lastChoice,
                                                  *players.second.lastChoice);
        if (choices.first == GRAB && choices.second == GRAB) {
          players.first.points += 1;
          players.second.points += 1;
        } else if (choices.first == SPLIT && choices.second == SPLIT) {
          players.first.points += 3;
          players.second.points += 3;
        } else if (choices.first == SPLIT && choices.second == GRAB) {
          players.first.points += 0;
          players.second.points += 5;
        } else if (choices.first == GRAB && choices.second == SPLIT) {
          players.first.points += 5;
          players.second.points += 0;
        }
        players.first.lastChoice = std::nullopt;
        players.second.lastChoice = std::nullopt;
        roundSuccess = true;
      }

      if (!roundSuccess) {
        log_out_game("Timeout exceed");
        return;
      }
    }

    log_out_game("Game between " + players.first.client + " (" +
                 std::to_string(players.first.points - startPoints.first) +
                 ") and " + players.second.client +
                 std::to_string(players.second.points - startPoints.second) +
                 " has been ended");
  }

  std::unordered_map<std::string, Client> clients;
  std::vector<Player> players;
  double timeout_sec = CLIENT_TIMEOUT_SEC;
};

ClientManager clientManager;

void handle_commands() {
  log_out_cmd("Thread has been started. Enter command");
  std::string cmd;
  while (isRunning) {
    log_out_cmd(
        "Enter a command:\n0. Exit\n1. Info\n2. Start a game\nSEND:{msg}");

    if (!std::getline(std::cin, cmd) && isRunning) {
      log_err("Error while reading command");
    }

    if (cmd == "0") {
      log_out_cmd("Handle exiting");
      isRunning = false;
    } else if (cmd == "1") {
      log_out_cmd("Handle info");
      clientManager.print_info();
    } else if (cmd == "2") {
      log_out_cmd("Handle game");
      clientManager.start_game();
    } else if (cmd.rfind("SEND:") == 0) {
      std::string msg = cmd.substr(5);
      clientManager.send_to_all_clients(msg);
    }
  }
}

void update_active_clients() {
  while (isRunning) {
    time_t t = time(nullptr);
    clientManager.update_clients(t);
  }
}

int main() {
  log_out_main("Server has started at port: " + std::to_string(PORT));

  signal(SIGINT, handle_signal);
  signal(SIGINT, handle_signal);

  server_socket = socket(AF_INET, SOCK_DGRAM, 0);

  if (server_socket < 0) {
    log_err("Server socket was not created successfully");
  }

  log_out_main("Server socket was created");

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  int reuse = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse,
                 sizeof(reuse)) < 0)
    log_err("Setsockopt failed");

  if (bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    log_err("Binding failed");

  log_out_main("Server socket has been binded to port " + std::to_string(PORT));

  std::thread commands_thread(handle_commands);

  char buffer[1024];
  sockaddr_in client_addr{};
  socklen_t addr_len = sizeof(client_addr);

  while (isRunning) {
    memset(&client_addr, 0, sizeof(client_addr));
    addr_len = sizeof(client_addr);

    ssize_t len = recvfrom(server_socket, buffer, sizeof(buffer), 0,
                           (sockaddr *)&client_addr, &addr_len);

    std::string clientKey = Client::format_addr(client_addr);

    if (len > 0 && isRunning) {
      std::string msg(buffer, len);

      if (msg.rfind("REGISTER:") == 0) {
        std::string playerName = msg.substr(9);
        Client newClient{playerName, client_addr, time(nullptr), true};
        clientManager.add(newClient);
      } else if (msg == "PING") {
        //log_out_main("Ping has been recieved from " +
          //           clientManager.get_name(clientKey));
        clientManager.handle_ping(clientKey);
      } else if (msg == "SPLIT") {
        if (!clientManager.answer(clientKey, GameChoice::SPLIT)) {
          log_out_game("Answer recieved but cant be reecognized");
        }
      } else if (msg == "GRAB") {
        if (!clientManager.answer(clientKey, GameChoice::GRAB)) {
          log_out_game("Answer recieved but cant be reecognized");
        }
      } else {
        log_out_main("Command from client wasn`t recognized: " + msg);
      }
    }
  }

  return 0;
}
