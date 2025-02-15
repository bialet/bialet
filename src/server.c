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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE (1024 * 1024 * 2) // 2 MB
#define PATH_SIZE (1024 * 2)
#define REQUEST_MESSAGE_SIZE 300

int                        server_fd = -1;
static struct BialetConfig bialet_config;

void handle_client(int client_socket);
void handle_file_upload(int client_socket, struct HttpMessage* hm,
                        struct BialetResponse* response);

int start_server(struct BialetConfig* config) {
  bialet_config = *config;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd == -1) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }
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
  s.str = (char*)malloc(len + 1); // Allocate memory for string + null terminator
  if(s.str == NULL) {
    perror("Failed to allocate memory for string");
    exit(EXIT_FAILURE);
  }
  strncpy(s.str, str, len);
  s.str[len] = '\0'; // Ensure null termination
  return s;
}

void clean_http_message(struct HttpMessage* hm) {
  free(hm->message.str);
  free(hm->method.str);
  free(hm->uri.str);
  free(hm->routes.str);
  free(hm->uploaded_files_ids.str);
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
    return "Content-Type: application/octet-stream\r\n";
  }
  if(strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
    return BIALET_HEADERS;
  } else if(strcmp(ext, ".css") == 0) {
    return "Content-Type: text/css\r\n";
  } else if(strcmp(ext, ".js") == 0) {
    return "Content-Type: application/javascript\r\n";
  } else if(strcmp(ext, ".json") == 0) {
    return "Content-Type: application/json\r\n";
  } else if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
    return "Content-Type: image/jpeg\r\n";
  } else if(strcmp(ext, ".png") == 0) {
    return "Content-Type: image/png\r\n";
  } else if(strcmp(ext, ".gif") == 0) {
    return "Content-Type: image/gif\r\n";
  } else if(strcmp(ext, ".svg") == 0) {
    return "Content-Type: image/svg+xml\r\n";
  } else if(strcmp(ext, ".txt") == 0) {
    return "Content-Type: text/plain\r\n";
  } else if(strcmp(ext, ".xml") == 0) {
    return "Content-Type: application/xml\r\n";
  } else if(strcmp(ext, ".pdf") == 0) {
    return "Content-Type: application/pdf\r\n";
  }
  return "Content-Type: application/octet-stream";
}

struct HttpMessage* parse_request(char* request) {
  struct HttpMessage* hm = (struct HttpMessage*)malloc(sizeof(struct HttpMessage));
  if(hm == NULL) {
    perror("Failed to allocate memory for HttpMessage");
    exit(EXIT_FAILURE);
  }
  hm->message = create_string(request, strlen(request));
  char* method = strtok(request, " ");
  hm->method = create_string(method, strlen(method));
  char* url = strtok(NULL, " ");
  hm->uri = create_string(url, strlen(url));
  hm->routes = create_string("", 0);
  hm->uploaded_files_ids = create_string("", 0);

  return hm;
}

size_t count_utf8_code_points(const char* s) {
  size_t count = 0;
  while(*s) {
    count += (*s++ & 0xC0) != 0x80;
  }
  return count;
}

void write_response(int client_socket, struct BialetResponse* response) {
  if(!response->status) {
    response->status = 404;
    response->body = BIALET_NOT_FOUND_PAGE;
    response->length = strlen(BIALET_NOT_FOUND_PAGE);
    response->header = BIALET_HEADERS;
  }

  if(response->length == 0) {
    response->length = strlen(response->body);
  }

  char* message = malloc(REQUEST_MESSAGE_SIZE);
  if(message == NULL) {
    perror("Failed to allocate memory for HTTP response");
    return;
  }
  int ok = snprintf(message, REQUEST_MESSAGE_SIZE,
                    "HTTP/1.1 %d %s\r\n"
                    "%s"
                    "Content-Length: %lu\r\n\r\n",
                    response->status, get_http_status_description(response->status),
                    response->header, (unsigned long)response->length);
  if(ok < 0) {
    perror("Failed to format HTTP response or buffer overflow");
    free(message);
    return;
  }

  write(client_socket, message, strlen(message));
  write(client_socket, response->body, response->length);
  free(message);
  close(client_socket);
}

void handle_client(int client_socket) {
  char    buffer[BUFFER_SIZE];
  ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
  if(bytes_read < 0) {
    perror("Error reading request");
    close(client_socket);
    return;
  }
  buffer[bytes_read] = '\0';

  struct HttpMessage* hm;
  hm = parse_request(buffer);
  message(magenta("Request"), hm->method.str, hm->uri.str);

  struct BialetResponse response = {0, "", "", 0};
  char                  path[PATH_SIZE];
  char                  wren_path[PATH_SIZE + 5];

  if(strcmp(hm->method.str, "POST") == 0 &&
     strstr(hm->message.str, "multipart/form-data") != NULL) {
    handle_file_upload(client_socket, hm, &response);
    if(response.status != 0) {
      write_response(client_socket, &response);
      return;
    }
  }

  struct stat file_stat;

  snprintf(path, PATH_SIZE, "%s%s", bialet_config.root_dir, hm->uri.str);
  // Remove query parameters if present
  char* query_start = strchr(path, '?');
  if(query_start) {
    *query_start = '\0'; // Truncate at '?'
  }

  // Handle routes ending with "/" or without
  if(path[strlen(path) - 1] == '/') {
    path[strlen(path) - 1] = '\0';
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
      strncpy(path + strlen(path) - 5, ".html", 6); // Try index.html
    }
  }

  if(strncmp(hm->uri.str, "/_", 2) == 0 || strstr(hm->uri.str, "/.") != NULL) {
    // Ignore files starting with _ or .
    clean_http_message(hm);
    response.status = 403;
    response.body = BIALET_FORBIDDEN_PAGE;
    write_response(client_socket, &response);
    return;
  }

  if(stat(path, &file_stat) != 0) {
    /*  Check if is favicon, send default */
    if(strcmp("/favicon.ico", hm->uri.str) == 0) {
      write(client_socket, FAVICON_RESPONSE, strlen(FAVICON_RESPONSE));
      write(client_socket, favicon_data, FAVICON_SIZE);
      clean_http_message(hm);
      close(client_socket);
      return;
    }

    // Search for _route.wren
    char* url_copy = strdup(hm->uri.str);
    while(1) {
      snprintf(path, PATH_SIZE, "%s%s/_route.wren", bialet_config.root_dir,
               url_copy);
      if(stat(path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        hm->routes = create_string(path, strlen(path));
        break;
      }
      char* last_slash = strrchr(url_copy, '/');
      if(!last_slash || last_slash == url_copy) { // Stop if root is reached
        free(url_copy);
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
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  rewind(file);

  char* file_content = malloc(file_size + 1);
  if(file_content == NULL) {
    perror("Error allocating memory for file content");
    fclose(file);
    clean_http_message(hm);
    close(client_socket);
    return;
  }

  fread(file_content, 1, file_size, file);
  file_content[file_size] = '\0';
  fclose(file);

  if(strstr(path, ".wren") != NULL) {
    // If file is a .wren file, run it
    response = bialetRun(path, file_content, hm);
  } else {
    // Otherwise serve static file
    response.status = 200;
    response.body = file_content;
    response.length = file_size;
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

void save_uploaded_file(struct HttpMessage* hm, struct HttpFile* files) {
  // TODO: Save uploaded files, implement again with new server
}

void parse_upload_file_headers(struct HttpFile* file, char* headers) {
  // TODO: Parsear lops headers!!

  // Parse Content-Disposition

  /*   char* filename_start = strstr(cd_header, "filename=\""); */
  /*   if(filename_start) { */
  /*     filename_start += 10; */
  /*     char* filename_end = strchr(filename_start, '"'); */
  /*     if(filename_end) { */
  /*       *filename_end = '\0'; */
  /*       filename = filename_start; */
  /*     } */
  /*   } */
  /* } */
}

void handle_file_upload(int client_socket, struct HttpMessage* hm,
                        struct BialetResponse* response) {
  char* request = hm->message.str;
  printf("%s\n\n-------- DONE -----------\n", request);

  char* boundary_start = strstr(request, "boundary=");
  if(!boundary_start) {
    response->status = 400;
    response->body = BIALET_BAD_REQUEST_PAGE;
    return;
  }
  boundary_start += 9; // Move past "boundary="
  char* boundary_end;
  if(*boundary_start == '"') {
    boundary_start++;
    boundary_end = strchr(boundary_start, '"');
    if(!boundary_end) {
      response->status = 400;
      response->body = BIALET_BAD_REQUEST_PAGE;
      return;
    }
  } else {
    boundary_end = boundary_start;
    while(*boundary_end && !strchr("; \r\n", *boundary_end))
      boundary_end++;
  }
  size_t boundary_len = boundary_end - boundary_start;
  char   boundary[boundary_len + 1];
  strncpy(boundary, boundary_start, boundary_len);
  boundary[boundary_len] = '\0';
  printf("Boundary value: %s\n", boundary);

  // Locate request body
  char* body_start = strstr(request, "\r\n\r\n");
  if(!body_start) {
    response->status = 400;
    response->body = BIALET_BAD_REQUEST_PAGE;
    return;
  }
  body_start += 4;

  // Validate initial boundary
  char initial_boundary[boundary_len + 5];
  snprintf(initial_boundary, sizeof(initial_boundary), "--%s\r\n", boundary);
  if(strncmp(body_start, initial_boundary, strlen(initial_boundary)) != 0) {
    response->status = 400;
    response->body = BIALET_BAD_REQUEST_PAGE;
    return;
  }
  char* current_part = body_start + strlen(initial_boundary);

  // Process multipart parts
  while(1) {
    char* headers_end = strstr(current_part, "\r\n\r\n");
    if(!headers_end)
      break;
    char* content_start = headers_end + 4;

    struct HttpFile* file = (struct HttpFile*)malloc(sizeof(struct HttpFile));
    parse_upload_file_headers(file, current_part);
    char boundary_marker[boundary_len + 8];
    snprintf(boundary_marker, sizeof(boundary_marker), "\r\n--%s", boundary);
    char* content_end = strstr(content_start, boundary_marker);
    if(!content_end)
      break;

    // Calculate file content
    size_t content_length = content_end - content_start - 1;
    printf("Content Length: %zu\n", content_length);
    // TODO: Obtener el contenido correcto!!
    // TODO: Probar con binarios!!
    char content[content_length + 1];
    strncpy(content, content_start, content_length);
    content[content_length] = '\0';
    printf("Content: \n\n--- Start ---\n%s\n--- End ---\n", content);
    // TODO: Guardar archivo en SQLite!!
    file->file = content;
    file->size = content_length;

    // Clean up
    free(file);
    file = NULL;

    char* next_boundary = strstr(content_start, boundary_marker);
    if(!next_boundary)
      break;
    current_part = next_boundary + strlen(boundary_marker) + 2;
  }
}
