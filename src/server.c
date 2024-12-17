/*
 * This file is part of Bialet, which is licensed under the
 * GNU General Public License, version 2 (GPL-2.0).
 *
 * Copyright (c) 2023 Rodrigo Arce
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * For full license text, see LICENSE.md.
 */
#include "server.h"

#include "bialet_wren.h"
#include "favicon.h"
#include "messages.h"
#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int server_fd = -1;

void handle_client(int client_socket);

// @TODO Remove this!
char* get_string(struct String str) {
  char* val = NULL;
  int   method_len = (int)(str.len);
  val = malloc(method_len + 1);
  if(val) {
    strncpy(val, str.str, method_len);
    val[method_len] = '\0';
  }
  return val;
}


int start_server(struct BialetConfig* config) {
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd == -1) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = inet_addr(config->host),
      .sin_port = htons(config->port),
  };

  if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    perror("Failed to bind");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  if(listen(server_fd, 10) == -1) {
    perror("Failed to listen");
    close(server_fd);
    exit(EXIT_FAILURE);
  }
  return 0;
}

void stop_server() {
  if(server_fd != -1) {
    close(server_fd);
    server_fd = -1;
  }
}

struct String create_string(const char* str, int len) {
  struct String s;
  s.len = len;
  s.str = (char*)malloc(len + 1); // Allocate memory for string + null terminator
  if(s.str == NULL) {
    perror("Failed to allocate memory for string");
    exit(EXIT_FAILURE);
  }
  strncpy(s.str, str, len);
  s.str[len] = '\0'; // Ensure null termination
  return s;
}

struct HttpMessage* parse_request(char* request) {
  struct HttpMessage* hm = (struct HttpMessage*)malloc(sizeof(struct HttpMessage));
  if(hm == NULL) {
    perror("Failed to allocate memory for HttpMessage");
    exit(EXIT_FAILURE);
  }
  hm->message = create_string(request, strlen(request));
  // Parse request
  char* method = strtok(request, " ");
  hm->method = create_string(method, strlen(method));
  char* url = strtok(NULL, " ");
  hm->uri = create_string(url, strlen(url));
  hm->routes = create_string("", 0);

  return hm; // Return pointer to the allocated HttpMessage
}

void handle_client(int client_socket) {
  char    buffer[BUFFER_SIZE];
  ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
  if(bytes_read < 0) {
    perror("Error reading request");
    close(client_socket);
    return;
  }

  buffer[bytes_read] = '\0'; // Null-terminate request

  struct HttpMessage* hm;

  hm = parse_request(buffer);

  message(magenta("Request"), get_string(hm->method), get_string(hm->uri));
  // @TODO: If request has files, save them
  // @TODO: If request is a directory, serve index.html or index.wren
  // @TODO: If request is a ignored file, ignore it
  // @TODO: If request is a Wren file, interpret it
  // @TODO: If request is a static file, serve it
  // @TODO: If file is saved in the FILES database, serve it
  // @TODO: If file not found but there is a _route.wren file, interpret it
  // @TODO: If file not found or you can't serve it, return 404
  if(strcmp("/favicon.ico", hm->uri.str) == 0) {
    write(client_socket, FAVICON_RESPONSE, strlen(FAVICON_RESPONSE));
    write(client_socket, favicon_data, FAVICON_SIZE);
    free(hm);
    close(client_socket);
    return;
  }
  struct BialetResponse response;
  response.status = 0;
  response.body = "";
  response.header = "";
  response.length = 0;
  if(response.status == 0) {
    response =
        bialetRun("index",
                  "import \"bialet\" for Response\n"
                  "var message = \"Hello World!\"\n"
                  "Response.out(<!doctype html><body>{{message}}</body></html>)",
                  hm);
    free(hm);
  }

  int   maxResponseSize = response.length + 300;
  char* output = malloc(maxResponseSize);

  if(output == NULL) {
    perror("Failed to allocate memory for HTTP response");
    return;
  }

  // Format the response into the buffer
  int written =
      snprintf(output, maxResponseSize,
               "HTTP/1.1 %d OK\r\n"
               "%s"
               "Content-Length: %lu\r\n"
               "\r\n"
               "%s",
               response.status, response.header ? response.header : "",
               response.length > 0 ? response.length : strlen(response.body),
               response.body ? response.body : "");

  if(written < 0 || written >= maxResponseSize) {
    perror("Failed to format HTTP response or buffer overflow");
    free(output);
    return;
  }
  write(client_socket, output, strlen(output));
  free(output);
  close(client_socket);
}

int server_poll(int delay) {
  struct pollfd fds[1];
  fds[0].fd = server_fd;
  fds[0].events = POLLIN;

  int poll_result = poll(fds, 1, delay);
  if(poll_result < 0) {
    perror("Poll error");
    return -1;
  } else if(poll_result == 0) {
    return 0; // Timeout occurred
  }

  if(fds[0].revents & POLLIN) {
    int client_socket = accept(server_fd, NULL, NULL);
    if(client_socket == -1) {
      perror("Failed to accept connection");
      return -1;
    }
    handle_client(client_socket);
  }

  return 0;
}
