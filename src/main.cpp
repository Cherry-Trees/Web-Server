/**
 *  Date:   2/4/2025
 */

#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#define MAX_WAITING 25
#define RBUF_SIZE 81

typedef enum Error {
  ERROR_OK,
  ERROR_USAGE,
  ERROR_SOCKET,
  ERROR_BIND,
  ERROR_LISTEN,
  ERROR_FILE
} Error;

static int dfa[5][256];
static bool dfa_inited = 0;

Error run_server(const int port);
std::string get_resource(const std::string &request);

int main(int argc, char *argv[]) {

  // Check if a port was entered.
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
    return ERROR_USAGE;
  }

  // Run the server.
  return run_server(atoi(argv[1]));
}

Error run_server(const int port) {

  // Create listening socket.
  int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_socket < 0) {
    std::cerr << "Could not create listening socket." << std::endl;
    return ERROR_SOCKET;
  }

  // Give server address socket info.
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  // Bind the listening socket to the server.
  if (bind(listen_socket, (struct sockaddr *) &server_addr, sizeof(server_addr))) {
    std::cerr << "Binding failed." << std::endl;
    return ERROR_BIND;
  }

  // Listen for a client connection.
  if(listen(listen_socket, MAX_WAITING)) {
    std::cerr << "Listen error." << std::endl;
    return ERROR_LISTEN;
  }

  // Give client address socket info.
  struct sockaddr_in client_addr;
  socklen_t client_addr_size = sizeof(client_addr);
  int connected_socket = accept(listen_socket, (struct sockaddr *) &client_addr, &client_addr_size);

  // Read the client request.
  int rbuf_len = RBUF_SIZE - 1;
  std::string request;
  while (!request.ends_with("\r\n\r\n")) {
    char rbuf[RBUF_SIZE];
    rbuf_len = read(connected_socket, rbuf, RBUF_SIZE - 1);
    rbuf[rbuf_len] = '\0';
    request += rbuf;    
  }

  // Filter the resource from the client request.
  request.erase(request.size() - 4); // Remove \r\n\r\n
  std::string resource = get_resource(request);

  // '/' -> index.html
  if (resource == "/")
    resource = "index.html";

  // Remove leading '.'s and '/'s.
  else while (resource.front() == '.' || resource.front() == '/')
    resource.erase(0, 1);

  // Read input file's contents.
  Error err = ERROR_OK;
  std::ifstream ifs(resource);
  std::string wbuf;
  if (ifs) {
    wbuf = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
    std::stringstream stream;
    stream << ifs.rdbuf();
    wbuf += stream.str();
    ifs.close();
  }
  else {
    wbuf = "HTTP/1.1 404 Not Found\n\n<b>404 Error - resource not found on this server</b>\n";
    err = ERROR_FILE;
  }

  // Write the file's contents to the server.
  int wbuf_len = wbuf.length();
  char *wbuf_cstr = (char *) wbuf.c_str();
  while (wbuf_len) {
      int wbuf_out = write(connected_socket, wbuf_cstr, wbuf_len);
      wbuf_len -= wbuf_out;
      wbuf_cstr += wbuf_out;
  }
  
  // Disconnect the client socket.
  close(connected_socket);
  return err;
}

std::string get_resource(const std::string &request) {

  // Construct a simple DFA that scans for 'GET'.
  if (!dfa_inited)
    init_dfa();

  int state = 0;
  std::string resource;
  unsigned int request_index = 0;
  while (state != -1 && request_index < request.length()) {

    // Get the next char.
    char ch = request.at(request_index++);

    // Only append to the resource string if 'GET' has been read (state = 4).
    if (state == 4 && !std::isspace(ch))
      resource += ch;

    // Move to the next state.
    state = dfa[state][(int) ch];
  }
  return resource;
}

void init_dfa() {
    for (int i = 0; i < 256; i++) {
        dfa[0][i] = 0;
        dfa[1][i] = 0;
        dfa[2][i] = 0;
        dfa[3][i] = 0;
        dfa[4][i] = 4;
    }

    dfa[0][(int)'G'] = 1;
    dfa[0][(int)'g'] = 1;
    dfa[1][(int)'E'] = 2;
    dfa[1][(int)'e'] = 2;
    dfa[2][(int)'T'] = 3;
    dfa[2][(int)'t'] = 3;
    dfa[3][(int)' '] = 4;

    dfa[4][(int)'\n'] = -1; // Delimiters
    dfa[4][(int)'\0'] = -1;
    dfa[4][(int)' '] = -1;

    dfa_inited = 1;
}
