/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2025 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#include "server.h"

#include "bialet.h"
#include "bialet_wren.h"
#include "favicon.h"
#include "messages.h"
#include <arpa/inet.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE (BUFSIZ * 4)
#define PATH_SIZE (1024 * 2)
#define REQUEST_MESSAGE_SIZE 300

int                        server_fd = -1;
static struct BialetConfig bialet_config;

static ssize_t send_all(int fd, const void* buf, size_t count) {
  size_t      sent = 0;
  const char* p = (const char*)buf;
  while(sent < count) {
    ssize_t n = send(fd, p + sent, count - sent, 0);
    if(n < 0)
      return n; // error
    if(n == 0)
      break; // peer closed
    sent += (size_t)n;
  }
  return (ssize_t)sent;
}

void handle_client(int client_socket);

int start_server(struct BialetConfig* config) {
  bialet_config = *config;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd == -1) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }
  // Enable SO_REUSEADDR option
  int opt = 1;
  if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    perror("Failed to set SO_REUSEADDR");
    close(server_fd);
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
    message(magenta("Server stopped"));
  }
}

struct String create_string(const char* str, int len) {
  struct String s;
  s.len = len;
  s.str = (char*)malloc((size_t)len + 1);
  if(s.str == NULL) {
    perror("Failed to allocate memory for string");
    exit(EXIT_FAILURE);
  }
  memcpy(s.str, str, (size_t)len);
  s.str[len] = '\0';
  return s;
}

void clean_http_message(struct HttpMessage* hm) {
  if(!hm)
    return;
  free(hm->message.str);
  free(hm->method.str);
  free(hm->uri.str);
  free(hm->routes.str);
  free(hm);
  hm = NULL;
}

const char* get_http_status_description(int status_code) {
  switch(status_code) {
    case 100:
      return "Continue";
    case 101:
      return "Switching Protocols";
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 203:
      return "Non-Authoritative Information";
    case 204:
      return "No Content";
    case 205:
      return "Reset Content";
    case 206:
      return "Partial Content";
    case 300:
      return "Multiple Choices";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 303:
      return "See Other";
    case 304:
      return "Not Modified";
    case 307:
      return "Temporary Redirect";
    case 308:
      return "Permanent Redirect";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 406:
      return "Not Acceptable";
    case 407:
      return "Proxy Authentication Required";
    case 408:
      return "Request Timeout";
    case 409:
      return "Conflict";
    case 410:
      return "Gone";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    case 505:
      return "HTTP Version Not Supported";
    default:
      return "Unknown Status";
  }
}

char* get_content_type(const char* path) {
  const char* ext = strrchr(path, '.');
  if(!ext) {
    return (char*)"Content-Type: application/octet-stream\r\n";
  }
  if(strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
    return BIALET_HEADERS;
  } else if(strcmp(ext, ".css") == 0) {
    return (char*)"Content-Type: text/css\r\n";
  } else if(strcmp(ext, ".js") == 0) {
    return (char*)"Content-Type: application/javascript\r\n";
  } else if(strcmp(ext, ".json") == 0) {
    return (char*)"Content-Type: application/json\r\n";
  } else if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
    return (char*)"Content-Type: image/jpeg\r\n";
  } else if(strcmp(ext, ".png") == 0) {
    return (char*)"Content-Type: image/png\r\n";
  } else if(strcmp(ext, ".gif") == 0) {
    return (char*)"Content-Type: image/gif\r\n";
  } else if(strcmp(ext, ".svg") == 0) {
    return (char*)"Content-Type: image/svg+xml\r\n";
  } else if(strcmp(ext, ".txt") == 0) {
    return (char*)"Content-Type: text/plain\r\n";
  } else if(strcmp(ext, ".xml") == 0) {
    return (char*)"Content-Type: application/xml\r\n";
  } else if(strcmp(ext, ".pdf") == 0) {
    return (char*)"Content-Type: application/pdf\r\n";
  }
  return (char*)"Content-Type: application/octet-stream\r\n";
}

struct HttpMessage* parse_request(char* request) {
  struct HttpMessage* hm = (struct HttpMessage*)malloc(sizeof(struct HttpMessage));
  if(hm == NULL) {
    perror("Failed to allocate memory for HttpMessage");
    exit(EXIT_FAILURE);
  }

  // Copiar solo la primera línea (método y path) sin modificar el buffer original
  char   first_line[BUFFER_SIZE];
  char*  line_end = strstr(request, "\r\n");
  size_t line_len = line_end ? (size_t)(line_end - request) : strlen(request);
  if(line_len >= sizeof(first_line))
    line_len = sizeof(first_line) - 1;
  memcpy(first_line, request, line_len);
  first_line[line_len] = '\0';

  hm->message = create_string(request, (int)strlen(request));

  // Tokenizar la primera línea segura
  char* saveptr = NULL;
  char* method = strtok_r(first_line, " ", &saveptr);
  if(!method)
    method = (char*)"GET";
  hm->method = create_string(method, (int)strlen(method));

  char* url = strtok_r(NULL, " ", &saveptr);
  if(!url)
    url = (char*)"/";
  hm->uri = create_string(url, (int)strlen(url));

  hm->routes = create_string("", 0);

  return hm;
}

void write_response(int client_socket, struct BialetResponse* response) {
  if(!response->status) {
    custom_error(404, response);
  }

  size_t body_len = response->length;
  if(body_len == 0 && response->body) {
    body_len = strlen(response->body);
  }

  char* message = (char*)malloc(REQUEST_MESSAGE_SIZE);
  if(message == NULL) {
    perror("Failed to allocate memory for HTTP response");
    return;
  }
  int ok =
      snprintf(message, REQUEST_MESSAGE_SIZE,
               "HTTP/1.1 %d %s\r\n"
               "%s"
               "Content-Length: %lu\r\n\r\n",
               response->status, get_http_status_description(response->status),
               response->header ? response->header : "", (unsigned long)body_len);
  if(ok < 0) {
    perror("Failed to format HTTP response or buffer overflow");
    free(message);
    return;
  }

  (void)send_all(client_socket, message, strlen(message));
  free(message);

  if(response->body && body_len > 0) {
    (void)send_all(client_socket, response->body, body_len);
  }

  close(client_socket);
}

void handle_client(int client_socket) {
  char    buffer[BUFFER_SIZE];
  ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
  if(bytes_read <= 0) {
    perror("Error reading request");
    close(client_socket);
    return;
  }

  struct HttpMessage* hm = parse_request(buffer);
  message(magenta("Request"), hm->method.str, hm->uri.str);

  if(strcmp("/favicon.ico", hm->uri.str) == 0) {
    (void)send_all(client_socket, FAVICON_RESPONSE, strlen(FAVICON_RESPONSE));
    (void)send_all(client_socket, favicon_data, FAVICON_SIZE);
    clean_http_message(hm);
    close(client_socket);
    return;
  }

  struct BialetResponse response = {0, "", "", 0};
  char                  path[PATH_SIZE];
  char                  wren_path[PATH_SIZE + 5];
  struct stat           file_stat;

  snprintf(path, PATH_SIZE, "%s%s", bialet_config.root_dir, hm->uri.str);
  // Remove query parameters if present
  char* query_start = strchr(path, '?');
  if(query_start) {
    *query_start = '\0'; // Truncate at '?'
  }

  // Handle routes ending with "/" or without
  size_t pathlen = strlen(path);
  if(pathlen > 0 && path[pathlen - 1] == '/') {
    path[pathlen - 1] = '\0';
  }

  if(strlen(path) + 5 < PATH_SIZE) { // 5 accounts for ".wren" and null terminator
    snprintf(wren_path, PATH_SIZE + 5, "%s.wren", path);
    if(stat(wren_path, &file_stat) == 0) {
      strncpy(path, wren_path, PATH_SIZE - 1);
      path[PATH_SIZE - 1] = '\0'; // Ensure null termination
    }
  } else {
    perror("Path too long to append .wren suffix");
  }

  if(stat(path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
    // Serve index.html or index.wren
    strncat(path, "/index.wren", PATH_SIZE - strlen(path) - 1);
    if(stat(path, &file_stat) != 0) {
      // Reemplazar sufijo ".wren" por ".html"
      size_t L = strlen(path);
      if(L >= 5) {
        strncpy(path + L - 5, ".html", 6);
      }
    }
  }

  if(strncmp(hm->uri.str, "/_", 2) == 0 || strstr(hm->uri.str, "/.") != NULL) {
    // Ignore files starting with _ or .
    clean_http_message(hm);
    response.status = 403;
    response.body = BIALET_FORBIDDEN_PAGE;
    response.length = strlen(BIALET_FORBIDDEN_PAGE);
    response.header = BIALET_HEADERS;
    write_response(client_socket, &response);
    return;
  }

  if(stat(path, &file_stat) != 0) {
    // Search for _route.wren
    char* url_copy = strdup(hm->uri.str);
    if(!url_copy) {
      perror("strdup");
      clean_http_message(hm);
      close(client_socket);
      return;
    }
    while(1) {
      snprintf(path, PATH_SIZE, "%s%s/_route.wren", bialet_config.root_dir,
               url_copy);
      if(stat(path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        hm->routes = create_string(url_copy, (int)strlen(url_copy));
        break;
      }
      char* last_slash = strrchr(url_copy, '/');
      if(!last_slash) { // Stop if root is reached
        free(url_copy);
        // If no index.wren or index.html or _route.wren in root
        // serve welcome page
        if(strncmp(hm->uri.str, "/", 2) == 0) {
          response.status = 200;
          response.body = BIALET_WELCOME_PAGE;
          response.length = strlen(BIALET_WELCOME_PAGE);
          response.header = BIALET_HEADERS;
        }
        clean_http_message(hm);
        write_response(client_socket, &response);
        return;
      }
      *last_slash = '\0'; // Truncate to parent directory
    }
    free(url_copy);
  }

  // Open file and read content
  FILE* file = fopen(path, "rb");
  if(file == NULL) {
    perror("Error opening file");
    clean_http_message(hm);
    close(client_socket);
    return;
  }
  if(fseek(file, 0, SEEK_END) != 0) {
    perror("fseek");
    fclose(file);
    clean_http_message(hm);
    close(client_socket);
    return;
  }
  size_t file_size = ftell(file);
  if(file_size < 0) {
    perror("ftell");
    fclose(file);
    clean_http_message(hm);
    close(client_socket);
    return;
  }
  rewind(file);

  unsigned char is_wren_file = 0;
  if(strstr(path, ".wren") != NULL) {
    is_wren_file = 1;
  }

  size_t alloc_size = file_size + is_wren_file;
  char*  file_content = malloc(alloc_size);
  if(file_content == NULL) {
    perror("Error allocating memory for file content");
    fclose(file);
    clean_http_message(hm);
    close(client_socket);
    return;
  }
  size_t read_bytes = fread(file_content, 1, file_size, file);
  if(read_bytes != file_size) {
    perror("Error reading file");
    free(file_content);
    fclose(file);
    clean_http_message(hm);
    close(client_socket);
    return;
  }
  fclose(file);

  if(is_wren_file) {
    file_content[read_bytes] = '\0';
    response = bialetRun(path, file_content, hm);
    if(response.length == 0 && response.body) {
      response.length = strlen(response.body);
    }
  } else {
    response.status = 200;
    response.body = file_content;
    response.length = read_bytes;
    response.header = get_content_type(path);
  }

  clean_http_message(hm);
  write_response(client_socket, &response);
  free(file_content);
}

int server_poll(int delay) {
  struct pollfd fds[1];
  fds[0].fd = server_fd;
  fds[0].events = POLLIN;

  int poll_result = poll(fds, 1, delay);
  if(poll_result < 0) {
    if(server_fd != -1) {
      perror("Poll error");
    }
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

void custom_error(int status, struct BialetResponse* response) {
  response->header = BIALET_HEADERS;
  response->status = status;
  char        path[PATH_SIZE];
  struct stat file_stat;
  snprintf(path, PATH_SIZE, "%s/%d.html", bialet_config.root_dir, status);
  if(stat(path, &file_stat) == 0) {
    FILE* file = fopen(path, "rb");
    if(file != NULL) {
      if(fseek(file, 0, SEEK_END) == 0) {
        long file_size = ftell(file);
        if(file_size >= 0) {
          rewind(file);
          char* file_content = (char*)malloc((size_t)file_size);
          if(file_content != NULL) {
            size_t read_bytes = fread(file_content, 1, (size_t)file_size, file);
            fclose(file);
            response->body = file_content;
            response->length = read_bytes;
            return;
          }
        }
      }
      fclose(file);
    }
  }
  if(status == 404) {
    response->body = BIALET_NOT_FOUND_PAGE;
    response->length = strlen(BIALET_NOT_FOUND_PAGE);
  } else if(status == 500) {
    response->body = BIALET_ERROR_PAGE;
    response->length = strlen(BIALET_ERROR_PAGE);
  } else {
    response->body = (char*)"";
    response->length = 0;
  }
}
