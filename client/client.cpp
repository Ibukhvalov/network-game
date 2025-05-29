#include <arpa/inet.h>
#include <array>
#include <csignal>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <random>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#define PORT "8080"
int client_socket;
sockaddr_in server_addr;
bool isRunning = true;

std::string playerName;
void log_out(std::string msg) {
  std::cout << '[' << playerName << "] " << msg << std::endl;
}

void log_err(std::string msg) {
  std::cerr << '[' << playerName << "] " << msg << ", terminate program"
            << std::endl;
  exit(0);
}

void handle_signal(int sig) {
  log_out("Signal " + std::to_string(sig) + " was gracefully handled");
  close(client_socket);
}

void resolve_name(const char *name, sockaddr_in &addr_out) {
  struct addrinfo hints{}, *res, *p;

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  int status = getaddrinfo(name, PORT, &hints, &res);

  if (status) {
    log_err("Error in getaddrinfo for " + std::string(name));
  }

  bool found = false;
  for (p = res; p != NULL; p = p->ai_next)
    if (p->ai_family == AF_INET) {
      memcpy(&addr_out, p->ai_addr, sizeof(sockaddr_in));
      char ipstr[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, (sockaddr_in *)&p->ai_addr, ipstr, sizeof(ipstr));
      log_out("Server resolved to: " + std::string(ipstr));
      found = true;
      break;
    }

  freeaddrinfo(res);
  if (!found) {
    log_err("IPv4 was not found for hostname");
  }
}

void handle_server_commands() {
  char buffer[1024];
  log_out("Sever listening was started");

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, 1);
  std::array<std::string, 2> answerOption{"SPLIT", "GRAB"};

  while (isRunning) {
    sockaddr_in server_addr_recv;
    socklen_t addr_len = sizeof(server_addr_recv);
    memset(buffer, 0, sizeof(buffer));
    ssize_t msg_len = recvfrom(client_socket, buffer, sizeof(buffer), 0,
                               (sockaddr *)&server_addr_recv, &addr_len);

    if (msg_len > 0) {
      std::string cmd(buffer, msg_len);
      char sender_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &server_addr_recv.sin_addr, sender_ip,
                sizeof(sender_ip));
      if (cmd == "CHOOSE") {
        std::string choice = answerOption[distrib(gen)];
        log_out("Player have chosen " + choice);
        ssize_t bytes_sent =
            sendto(client_socket, choice.c_str(), choice.size(), 0,
                   (sockaddr *)&server_addr, sizeof(server_addr));

        if (bytes_sent <= 0) {
          log_err("Answer wasn`t sent");
        }
      } else {
        log_out("Message has been recieved: " + cmd);
      }
    }
  }

  log_out("Server listening was ended");
}

void register_client() {
  std::string msg = "REGISTER:" + playerName;
  log_out("Try to register with msg: " + msg);
  ssize_t bytes_sent = sendto(client_socket, msg.c_str(), msg.size(), 0,
                              (sockaddr *)&server_addr, sizeof(server_addr));

  if (bytes_sent <= 0) {
    log_err("REGISTER msg wasn`t sent");
  } else {
    log_out("REGISTER msg was sent");
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Player name was not specified, terminate program\n";
    return 1;
  }

  playerName = argv[1];
  log_out("Player was started");

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  client_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (client_socket < 0) {
    log_err("Creating socket wasnt successful");
    return 1;
  }
  log_out("Socket was created");

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);

  if (inet_pton(AF_INET, "172.25.0.10", &server_addr.sin_addr) <= 0) {
    log_err("Invalid server IP address");
  }
  char server_ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &server_addr.sin_addr, server_ip_str,
            sizeof(server_ip_str));
  log_out("Server was set up with address: " + std::string(server_ip_str) +
          ":" + std::to_string(ntohs(server_addr.sin_port)));

  register_client();
  std::thread server_listener_thread(handle_server_commands);

  while (isRunning) {
    sleep(3);

    ssize_t bytes_sent = sendto(client_socket, "PING", 4, 0,
                                (sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_sent <= 0) {
      log_err("PING was not sent to server");
    }

    log_out("PING was sent to server");
  }

  return 0;
}
