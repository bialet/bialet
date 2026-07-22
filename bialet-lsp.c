#include "src/wren.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_HEADER 256
#define MAX_BODY   (16 * 1024 * 1024)
#define MAX_TOKENS 32768

typedef struct {
  char* data;
  int   len;
  int   cap;
} Buffer;

typedef struct {
  int    line;
  int    col;
  char*  message;
} LspDiagnostic;

typedef struct {
  int line;
  int col;
  int len;
  int type;
  int mod;
} SemanticToken;

static Buffer buf = {NULL, 0, 0};
static char   projectRoot[4096];
static WrenVM* lspVM = NULL;

#define MAX_DOCS 64
static char* docUris[MAX_DOCS];
static char* docTexts[MAX_DOCS];
static int   docCount = 0;

static void cacheDoc(const char* uri, const char* text) {
  for(int i = 0; i < docCount; i++) {
    if(strcmp(docUris[i], uri) == 0) {
      free(docTexts[i]);
      docTexts[i] = strdup(text);
      return;
    }
  }
  if(docCount < MAX_DOCS) {
    docUris[docCount] = strdup(uri);
    docTexts[docCount] = strdup(text);
    docCount++;
  }
}

static char* cachedDoc(const char* uri) {
  for(int i = 0; i < docCount; i++)
    if(strcmp(docUris[i], uri) == 0)
      return docTexts[i];
  return NULL;
}

static void bufEnsure(int extra) {
  int need = buf.len + extra;
  if(need <= buf.cap)
    return;
  int cap = buf.cap ? buf.cap * 2 : 4096;
  while(cap < need)
    cap *= 2;
  char* nd = realloc(buf.data, cap);
  if(!nd) {
    fprintf(stderr, "bialet-lsp: out of memory\n");
    exit(1);
  }
  buf.data = nd;
  buf.cap = cap;
}

static void bufWrite(const char* s, int len) {
  bufEnsure(len + 2);
  memcpy(buf.data + buf.len, s, len);
  buf.len += len;
  buf.data[buf.len] = '\0';
}

static void bufWriteF(const char* fmt, ...)
    __attribute__((format(printf, 1, 2)));
static void bufWriteF(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int needed = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  va_start(args, fmt);
  bufEnsure(needed + 1);
  vsnprintf(buf.data + buf.len, needed + 1, fmt, args);
  va_end(args);
  buf.len += needed;
}

static void bufReset(void) { buf.len = 0; }

static void sendMessage(const char* json) {
  int len = (int)strlen(json);
  dprintf(STDOUT_FILENO, "Content-Length: %d\r\n\r\n%s", len, json);
  fflush(stdout);
}

static int readHeader(void) {
  char hdr[MAX_HEADER];
  int  hlen = 0;
  while(hlen < MAX_HEADER - 1) {
    char    c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if(n <= 0)
      return -1;
    hdr[hlen++] = c;
    if(hlen >= 4 && memcmp(hdr + hlen - 4, "\r\n\r\n", 4) == 0)
      break;
  }
  hdr[hlen] = '\0';
  const char* cl = strstr(hdr, "Content-Length: ");
  if(!cl)
    return -1;
  return atoi(cl + 16);
}

static int readBody(int len, char** out) {
  if(len <= 0 || len > MAX_BODY)
    return -1;
  char* body = malloc(len + 1);
  if(!body)
    return -1;
  size_t total = 0;
  while(total < (size_t)len) {
    ssize_t n = read(STDIN_FILENO, body + total, len - total);
    if(n <= 0) {
      free(body);
      return -1;
    }
    total += (size_t)n;
  }
  body[len] = '\0';
  *out = body;
  return 0;
}

static char* jsonString(const char* json, const char* key) {
  char        search[256];
  int         slen = snprintf(search, sizeof(search), "\"%s\"", key);
  const char* pos = strstr(json, search);
  if(!pos)
    return NULL;
  pos += slen;
  while(*pos == ' ' || *pos == ':')
    pos++;
  if(*pos != '"')
    return NULL;
  pos++;
  const char* end = pos;
  while(*end && *end != '"') {
    if(*end == '\\' && *(end + 1))
      end++;
    end++;
  }
  int   len = (int)(end - pos);
  char* val = malloc(len + 1);
  if(!val)
    return NULL;
  memcpy(val, pos, len);
  val[len] = '\0';
  char* wp = val;
  for(char* rp = val; *rp; rp++) {
    if(*rp == '\\' && *(rp + 1)) {
      switch(rp[1]) {
        case '"': *wp++ = '"'; break;
        case '\\': *wp++ = '\\'; break;
        case '/': *wp++ = '/'; break;
        case 'n': *wp++ = '\n'; break;
        case 'r': *wp++ = '\r'; break;
        case 't': *wp++ = '\t'; break;
        default: *wp++ = rp[1]; break;
      }
      rp++;
    } else {
      *wp++ = *rp;
    }
  }
  *wp = '\0';
  return val;
}

static int jsonInt(const char* json, const char* key, int def) {
  char        search[256];
  snprintf(search, sizeof(search), "\"%s\"", key);
  const char* pos = strstr(json, search);
  if(!pos)
    return def;
  pos += strlen(search);
  while(*pos == ' ' || *pos == ':')
    pos++;
  return atoi(pos);
}

static char* jsonStringNested(const char* json, const char* outerKey,
                               const char* innerKey) {
  char        search[256];
  snprintf(search, sizeof(search), "\"%s\"", outerKey);
  const char* pos = strstr(json, search);
  if(!pos)
    return NULL;
  return jsonString(pos, innerKey);
}

static int jsonIntNested(const char* json, const char* outerKey,
                          const char* innerKey, int def) {
  char        search[256];
  snprintf(search, sizeof(search), "\"%s\"", outerKey);
  const char* pos = strstr(json, search);
  if(!pos)
    return def;
  return jsonInt(pos, innerKey, def);
}

static void uriToPath(const char* uri, char* path, int pathSize) {
  if(strncmp(uri, "file://", 7) == 0)
    snprintf(path, pathSize, "%s", uri + 7);
  else
    snprintf(path, pathSize, "%s", uri);
}

static void jsonEscape(const char* s, char* out, int outSize) {
  int j = 0;
  for(int i = 0; s[i] && j < outSize - 2; i++) {
    switch(s[i]) {
      case '"': out[j++] = '\\'; out[j++] = '"'; break;
      case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
      case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
      case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
      case '\t': out[j++] = '\\'; out[j++] = 't'; break;
      default: out[j++] = s[i]; break;
    }
  }
  out[j] = '\0';
}

static int fileExists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static char* findImportFile(const char* docPath, const char* importPath) {
  char* result = malloc(4096);
  if(!result)
    return NULL;

  snprintf(result, 4096, "%s/%s.wren", projectRoot, importPath);
  if(fileExists(result))
    return result;

  {
    char        dir[4096];
    const char* lastSep = strrchr(docPath, '/');
    if(lastSep) {
      int dlen = (int)(lastSep - docPath);
      memcpy(dir, docPath, dlen);
      dir[dlen] = '\0';
      snprintf(result, 4096, "%s/%s.wren", dir, importPath);
      if(fileExists(result))
        return result;
    }
  }

  snprintf(result, 4096, "%s/%s", projectRoot, importPath);
  if(fileExists(result))
    return result;

  free(result);
  return NULL;
}

typedef struct {
  const char* label;
  const char* detail;
  int         kind;
  const char* insertText;
} Completion;

static Completion completions[] = {
    {"Response.json", "Send JSON response", 2,
     "Response.json(${1:data})"},
    {"Response.redirect", "Redirect to URL", 2,
     "Response.redirect(\"${1:url}\")"},
    {"Response.status", "Set HTTP status", 2,
     "Response.status(${1:code})"},
    {"Response.header", "Set response header", 2,
     "Response.header(\"${1:name}\", \"${2:value}\")"},
    {"Response.out", "Append to response body", 2,
     "Response.out(\"${1:text}\")"},
    {"Response.cors", "Enable CORS", 2,
     "Response.cors(\"${1:*}\", \"${2:GET, POST}\", \"${3:Content-Type}\")"},
    {"Response.file", "Serve file by ID", 2,
     "Response.file(${1:id})"},
    {"Response.page", "Generate HTML page", 2,
     "Response.page(\"${1:title}\", \"${2:message}\")"},
    {"Response.end", "End response with status page", 2,
     "Response.end(${1:code}, \"${2:title}\", \"${3:message}\")"},
    {"Response.forbidden", "Send 403 Forbidden", 2,
     "Response.forbidden()"},
    {"Response.login", "Send login page", 2,
     "Response.login()"},
    {"Response.addCookieHeader", "Add Set-Cookie header", 2,
     "Response.addCookieHeader(\"${1:value}\")"},

    {"Request.get", "Get query parameter", 2,
     "Request.get(\"${1:name}\")"},
    {"Request.query", "Get query parameter (alias)", 2,
     "Request.query(\"${1:name}\")"},
    {"Request.post", "Get POST parameter", 2,
     "Request.post(\"${1:name}\")"},
    {"Request.header", "Get request header", 2,
     "Request.header(\"${1:name}\")"},
    {"Request.route", "Get route segment", 2,
     "Request.route(${1:pos})"},
    {"Request.file", "Get uploaded file", 2,
     "Request.file(\"${1:name}\")"},
    {"Request.login", "Basic HTTP auth check", 2,
     "Request.login(\"${1:user}\", \"${2:pass}\")"},
    {"Request.json", "Parse JSON body", 2,
     "Request.json()"},
    {"Request.isGet", "True if GET request", 5,
     "Request.isGet"},
    {"Request.isPost", "True if POST request", 5,
     "Request.isPost"},
    {"Request.isJson", "True if content-type is JSON", 5,
     "Request.isJson"},
    {"Request.method", "HTTP method string", 5,
     "Request.method"},
    {"Request.uri", "Request URI", 5,
     "Request.uri"},
    {"Request.body", "Request body", 5,
     "Request.body"},

    {"Cookie.set", "Set a cookie", 2,
     "Cookie.set(\"${1:name}\", \"${2:value}\"${3:, options})"},
    {"Cookie.get", "Get a cookie", 2,
     "Cookie.get(\"${1:name}\"${2:, default})"},
    {"Cookie.delete", "Delete a cookie", 2,
     "Cookie.delete(\"${1:name}\")"},

    {"Session.new", "Create new session instance", 20,
     "Session.new()"},
    {"Session.get", "Get session value", 2,
     "Session.get(\"${1:key}\")"},
    {"Session.set", "Set session value", 2,
     "Session.set(\"${1:key}\", ${2:value})"},
    {"Session.destroy", "Destroy the session", 2,
     "Session.destroy()"},
    {"Session.csrf", "CSRF hidden input field", 5,
     "Session.csrf"},
    {"Session.csrfOk", "Validate CSRF token", 5,
     "Session.csrfOk"},
    {"Session.id", "Session ID", 5,
     "Session.id"},
    {"Session.name", "Session cookie name", 5,
     "Session.name"},

    {"Json.parse", "Parse JSON string", 2,
     "Json.parse(${1:jsonString})"},
    {"Json.stringify", "Convert to JSON string", 2,
     "Json.stringify(${1:object})"},

    {"Util.randomString", "Generate random string", 2,
     "Util.randomString(${1:length})"},
    {"Util.hash", "Hash a password", 2,
     "Util.hash(\"${1:password}\")"},
    {"Util.verify", "Verify password against hash", 2,
     "Util.verify(\"${1:password}\", \"${2:hash}\")"},
    {"Util.toNum", "Convert string to number", 2,
     "Util.toNum(${1:val})"},
    {"Util.hexToDec", "Hex string to decimal", 2,
     "Util.hexToDec(\"${1:hex}\")"},
    {"Util.toHex", "Byte to hex string", 2,
     "Util.toHex(${1:byte})"},
    {"Util.urlDecode", "Decode URL-encoded string", 2,
     "Util.urlDecode(\"${1:str}\")"},
    {"Util.urlEncode", "URL-encode a string", 2,
     "Util.urlEncode(\"${1:str}\")"},
    {"Util.htmlEscape", "Escape HTML characters", 2,
     "Util.htmlEscape(\"${1:str}\")"},
    {"Util.params", "Encode key-value pairs to query string", 2,
     "Util.params(${1:params})"},
    {"Util.encodeBase64", "Encode to Base64", 2,
     "Util.encodeBase64(\"${1:input}\")"},
    {"Util.decodeBase64", "Decode from Base64", 2,
     "Util.decodeBase64(\"${1:input}\")"},
    {"Util.lpad", "Left-pad a string", 2,
     "Util.lpad(\"${1:str}\", ${2:count}, \"${3:char}\")"},
    {"Util.reverse", "Reverse a string", 2,
     "Util.reverse(\"${1:str}\")"},

    {"Config.get", "Get config value", 2,
     "Config.get(\"${1:key}\")"},
    {"Config.set", "Set config value", 2,
     "Config.set(\"${1:key}\", ${2:value})"},
    {"Config.bool", "Get config as boolean", 2,
     "Config.bool(\"${1:key}\")"},
    {"Config.num", "Get config as number", 2,
     "Config.num(\"${1:key}\")"},
    {"Config.delete", "Delete config key", 2,
     "Config.delete(\"${1:key}\")"},
    {"Config.json", "Get/set config as JSON", 2,
     "Config.json(\"${1:key}\"${2:, value})"},

    {"Db.migrate", "Run database migration", 2,
     "Db.migrate(${1:version}, ${2:schema})"},
    {"Db.save", "Save record to table", 2,
     "Db.save(\"${1:table}\", ${2:values})"},
    {"Db.delete", "Delete record from table", 2,
     "Db.delete(\"${1:table}\", ${2:id})"},
    {"Db.clean", "Remove expired sessions/temp files", 5,
     "Db.clean"},

    {"Http.request", "Perform HTTP request", 2,
     "Http.request(\"${1:url}\", \"${2:GET}\", ${3:data}, ${4:options})"},
    {"Http.get", "HTTP GET request", 2,
     "Http.get(\"${1:url}\"${2:, options})"},
    {"Http.post", "HTTP POST request", 2,
     "Http.post(\"${1:url}\", ${2:data}${3:, options})"},
    {"Http.put", "HTTP PUT request", 2,
     "Http.put(\"${1:url}\", ${2:data}${3:, options})"},
    {"Http.delete", "HTTP DELETE request", 2,
     "Http.delete(\"${1:url}\"${2:, options})"},

    {"Date.new", "Create new Date", 20,
     "Date.new(${1:year}, ${2:month}, ${3:day})"},
    {"Date.format", "Format date string", 2,
     "Date.format(\"${1:format}\")"},
    {"Date.diff", "Difference between dates", 2,
     "Date.diff(${1:otherDate})"},
    {"Date.year", "Year component", 5, "Date.year"},
    {"Date.month", "Month component", 5, "Date.month"},
    {"Date.day", "Day component", 5, "Date.day"},
    {"Date.hour", "Hour component", 5, "Date.hour"},
    {"Date.minute", "Minute component", 5, "Date.minute"},
    {"Date.second", "Second component", 5, "Date.second"},
    {"Date.dayOfWeek", "Day of week (0-6)", 5, "Date.dayOfWeek"},
    {"Date.dayOfYear", "Day of year (1-365)", 5, "Date.dayOfYear"},
    {"Date.date", "Date as YYYY-MM-DD", 5, "Date.date"},
    {"Date.time", "Time as HH:MM:SS", 5, "Date.time"},
    {"Date.unix", "Unix timestamp", 5, "Date.unix"},
    {"Date.iso", "ISO 8601 string", 5, "Date.iso"},
    {"Date.inUtc", "Date in UTC", 5, "Date.inUtc"},
    {"Date.tz", "Timezone offset", 5, "Date.tz"},

    {"File.new", "Create File instance", 20,
     "File.new(${1:data})"},
    {"File.get", "Fetch file from database", 2,
     "File.get(${1:id})"},
    {"File.create", "Create file in database", 2,
     "File.create(\"${1:name}\", \"${2:type}\", ${3:content}${4:, size})"},
    {"File.id", "File database ID", 5, "File.id"},
    {"File.type", "File MIME type", 5, "File.type"},
    {"File.name", "File name", 5, "File.name"},
    {"File.size", "File size in bytes", 5, "File.size"},
    {"File.isTemp", "Is file temporary?", 5, "File.isTemp"},
    {"File.createdAt", "File creation date", 5, "File.createdAt"},
    {"File.destroy", "Delete file from database", 2,
     "File.destroy"},
    {"File.save", "Mark file as permanent", 2,
     "File.save"},
    {"File.temp", "Mark file as temporary", 2,
     "File.temp"},

    {"Cron.every", "Run job every N minutes", 2,
     "Cron.every(${1:minutes}) { |d| ${2:body} }"},
    {"Cron.at", "Run job at specific time", 2,
     "Cron.at(${1:hour}, ${2:minute}${3:, dayOfWeek}) { |d| ${4:body} }"},

    {"Mcp.new", "Create MCP server", 20,
     "Mcp.new(\"${1:name}\", \"${2:version}\")"},
    {"Mcp.addTool", "Register MCP tool class", 2,
     "Mcp.addTool(${1:ToolClass})"},
    {"Mcp.addPrompt", "Set MCP system prompt", 2,
     "Mcp.addPrompt(\"${1:prompt}\")"},
    {"Mcp.serve", "Start MCP server", 5,
     "Mcp.serve"},

    {".fetch", "Execute query, return rows", 2,
     ".fetch()"},
    {".first", "Execute query, return first row", 2,
     ".first()"},
    {".val", "Execute query, return single value", 2,
     ".val()"},
    {".toNum", "Convert query string to number", 2,
     ".toNum"},
    {".to", "Map query results to class", 2,
     ".to(${1:ClassName})"},
    {".toBool", "Convert string to boolean", 2,
     ".toBool"},
    {".safe", "HTML-escape the string", 2,
     ".safe"},
    {".lower", "Lowercase string", 2,
     ".lower"},
    {".upper", "Uppercase string", 2,
     ".upper"},
    {".first", "First element of list", 2,
     ".first"},

    {"import", "Import a module", 14,
     "import \"${1:path}\" for ${2:Name}"},
    {"System.print", "Print to stdout", 2,
     "System.print(${1:value})"},
    {"System.log", "Log to server log", 2,
     "System.log(\"${1:message}\")"},
    {"System.clock", "CPU time in seconds", 2,
     "System.clock"},

    {"class", "Define a class", 14,
     "class ${1:Name} {\n  ${2}\n}"},
    {"foreign class", "Define a foreign class", 14,
     "foreign class ${1:Name} {\n  ${2}\n}"},
    {"var", "Declare a variable", 14,
     "var ${1:name} = ${2:value}"},
    {"static", "Static method/field modifier", 14,
     "static "},
    {"construct", "Constructor method", 14,
     "construct ${1:new}(${2:params}) {\n  ${3}\n}"},
    {NULL, NULL, 0, NULL},
};

static SemanticToken stokens[MAX_TOKENS];
static int           stokenCount;

static int isWrenIdent(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int isWrenDigit(char c) { return c >= '0' && c <= '9'; }

static void addSemToken(int line, int col, int len, int type, int mod) {
  if(stokenCount >= MAX_TOKENS)
    return;
  stokens[stokenCount].line = line;
  stokens[stokenCount].col = col;
  stokens[stokenCount].len = len;
  stokens[stokenCount].type = type;
  stokens[stokenCount].mod = mod;
  stokenCount++;
}

static void tokenizeWren(const char* src) {
  stokenCount = 0;
  const char* p = src;
  int         line = 0;
  int         col = 0;

  static const char* bialetClasses[] = {
      "Request", "Response", "Cookie", "Session", "Json", "Util",
      "Config", "Db", "Http", "Date", "File", "Cron", "Mcp",
      "System", "Layout", "Num", "ClassAttributes", "Test",
      "TestResponse", NULL};
  static const char* keywords[] = {
      "break", "continue", "class", "construct", "else",  "false",
      "for",   "foreign",  "if",    "import",    "as",    "in",
      "is",    "null",     "return", "static",   "super", "this",
      "true",  "var",      "while",  NULL};

  while(*p) {
    if(*p == '\n') {
      line++;
      col = 0;
      p++;
      continue;
    }
    if(*p == ' ' || *p == '\t' || *p == '\r') {
      col++;
      p++;
      continue;
    }

    // line comment
    if(*p == '/' && *(p + 1) == '/') {
      int startCol = col, len = 0;
      while(*p && *p != '\n') { p++; col++; len++; }
      addSemToken(line, startCol, len, 2, 0);
      continue;
    }

    // block comment
    if(*p == '/' && *(p + 1) == '*') {
      int startCol = col, startLine = line, len = 0;
      while(*p && !(*p == '*' && *(p + 1) == '/')) {
        if(*p == '\n') { line++; col = 0; } else col++;
        len++; p++;
      }
      if(*p) { len += 2; p += 2; col += 2; }
      addSemToken(startLine, startCol, len, 2, 0);
      continue;
    }

    // tag strings: <div class="x">...{{ name }}...</div>
    if(*p == '<' && (isWrenIdent(*(p + 1)) || *(p + 1) == '/' ||
                     *(p + 1) == '!')) {
      int startCol = col, startLine = line, len = 1;
      p++; col++;

      int isClosing = 0;
      if(*p == '/') {
        isClosing = 1;
        len++; p++; col++;
      }

      const char* tagName = p;
      int         tagLen = 0;
      while(*p && (isWrenIdent(*p) || isWrenDigit(*p))) {
        tagLen++; col++; p++;
      }

      while(*p && *p != '>') {
        if(*p == '"') {
          len++; p++; col++;
          while(*p && *p != '"') {
            if(*p == '\\') { len++; p++; col++; }
            if(*p == '\n') { line++; col = 0; }
            else col++;
            len++; p++;
          }
        } else if(*p == '\n') {
          line++; col = 0;
        } else {
          col++;
        }
        len++; p++;
      }
      if(*p == '>') { len++; p++; col++; }

      int selfClose = isClosing || (*(p - 2) == '/');
      if(selfClose || tagLen == 0) {
        addSemToken(startLine, startCol, len, 9, 0);
        continue;
      }

      // matched open/close pair — consume until matching </tagName>
      int depth = 1;
      while(*p && depth > 0) {
        const char* bc = p;

        if(*p == '<') {
          if(*(p + 1) == '/') {
            const char* ct = p + 2;
            int         cl = 0;
            while(ct[cl] && (isWrenIdent(ct[cl]) || isWrenDigit(ct[cl]))) cl++;
            int nameMatch = (cl == tagLen &&
                             memcmp(ct, tagName, tagLen) == 0);
            depth--;
            if(depth == 0) {
              if(nameMatch) {
                len++; p++; col++;
                len++; p++; col++;
                while(*p && *p != '>') {
                  if(*p == '\n') { line++; col = 0; }
                  else col++;
                  len++; p++;
                }
                if(*p == '>') { len++; p++; col++; }
              }
              break;
            }
          } else if(*(p + 1) != '=' && *(p + 1) != '<' &&
                    *(p + 1) != '>' && isWrenIdent(*(p + 1))) {
            depth++;
          }
        }

        if(p == bc) {
          if(*p == '{' && *(p + 1) == '{') {
            len += 2; p += 2; col += 2;
            int hb = 1;
            while(*p && hb > 0) {
              if(*p == '{' && *(p + 1) == '{') { hb++; len++; p++; col++; }
              else if(*p == '}' && *(p + 1) == '}') { hb--; }
              else if(*p == '\n') { line++; col = 0; }
              else if(*p == '"') {
                len++; p++; col++;
                while(*p && *p != '"') {
                  if(*p == '\\') { len++; p++; col++; }
                  if(*p == '\n') { line++; col = 0; }
                  else col++;
                  len++; p++;
                }
              } else { col++; }
              len++; p++;
            }
            if(*p == '}') { len++; p++; col++; }
          } else if(*p == '\n') {
            line++; col = 0; len++; p++;
          } else if(*p == '"') {
            len++; p++; col++;
            while(*p && *p != '"') {
              if(*p == '\\') { len++; p++; col++; }
              if(*p == '\n') { line++; col = 0; }
              else col++;
              len++; p++;
            }
            len++; p++; col++;
          } else {
            col++; len++; p++;
          }
        }
      }
      addSemToken(startLine, startCol, len, 9, 0);
      continue;
    }

    // backtick SQL query (type 8)
    if(*p == '`') {
      int startCol = col, startLine = line, len = 1;
      p++; col++;
      while(*p && *p != '`') {
        if(*p == '\n') { line++; col = 0; }
        else col++;
        len++; p++;
      }
      if(*p == '`') { len++; p++; col++; }
      addSemToken(startLine, startCol, len, 8, 0);
      continue;
    }

    // double-quoted string (type 1)
    if(*p == '"') {
      int startCol = col, startLine = line, len = 1;
      p++; col++;
      while(*p && *p != '"') {
        if(*p == '\\') {
          len += 2; p += 2; col += 2;
        } else if(*p == '\n') {
          line++; col = 0; len++; p++;
        } else if(*p == '%' && *(p + 1) == '(') {
          len += 2; p += 2; col += 2;
          int depth = 1;
          while(*p && depth > 0) {
            if(*p == '%' && *(p + 1) == '(') {
              depth++; len += 2; p += 2; col += 2;
            } else if(p > src && *p == ')' && *(p - 1) == '%') {
              depth--; len++; p++; col++;
            } else if(*p == '\n') {
              line++; col = 0; len++; p++;
            } else {
              len++; col++; p++;
            }
          }
        } else {
          len++; col++; p++;
        }
      }
      if(*p == '"') { len++; p++; col++; }
      addSemToken(startLine, startCol, len, 1, 0);
      continue;
    }

    // number
    if(isWrenDigit(*p)) {
      int startCol = col, len = 0, dots = 0;
      if(*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) {
        while(*p && (isWrenDigit(*p) ||
                     (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F') ||
                     *p == 'x' || *p == 'X')) {
          len++; col++; p++;
        }
      } else {
        while(*p && (isWrenDigit(*p) ||
                     (*p == '.' && dots == 0 && isWrenDigit(*(p + 1))))) {
          if(*p == '.') dots++;
          len++; col++; p++;
        }
      }
      addSemToken(line, startCol, len, 3, 0);
      continue;
    }

    // identifier
    if(isWrenIdent(*p)) {
      const char* s = p;
      int len = 0;
      while(*p && (isWrenIdent(*p) || isWrenDigit(*p))) {
        len++; col++; p++;
      }
      int type = 4; // variable
      for(int i = 0; keywords[i]; i++) {
        if((int)strlen(keywords[i]) == len &&
           memcmp(s, keywords[i], len) == 0) {
          type = 0; break; // keyword
        }
      }
      if(type == 4) {
        for(int i = 0; bialetClasses[i]; i++) {
          if((int)strlen(bialetClasses[i]) == len &&
             memcmp(s, bialetClasses[i], len) == 0) {
            type = 6; break; // class
          }
        }
      }
      addSemToken(line, col - len, len, type, 0);
      continue;
    }

    // operator
    {
      int startCol = col, len = 0;
      if((*p == *(p + 1)) && (*p == '=' || *p == '>' || *p == '|' ||
                              *p == '&' || *p == '.')) {
        len = 2;
        if(*p == '.' && *(p + 2) == '.') len = 3;
        col += len; p += len;
      } else if(*p == '!' && *(p + 1) == '=') {
        len = 2; col += 2; p += 2;
      } else if(*p == '<' && *(p + 1) == '=') {
        len = 2; col += 2; p += 2;
      } else if(*p == '>' && *(p + 1) == '=') {
        len = 2; col += 2; p += 2;
      } else {
        len = 1; col++; p++;
      }
      addSemToken(line, startCol, len, 7, 0);
    }
  }
}

static void handleInitialize(const char* json) {
  int id = jsonInt(json, "id", 0);
  char* rootUri = jsonString(json, "rootUri");
  if(!rootUri)
    rootUri = jsonString(json, "rootPath");
  if(rootUri) {
    uriToPath(rootUri, projectRoot, sizeof(projectRoot));
    free(rootUri);
  } else {
    getcwd(projectRoot, sizeof(projectRoot));
  }

  bufReset();
  bufWriteF(
      "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{"
      "\"capabilities\":{"
      "\"textDocumentSync\":1,"
      "\"completionProvider\":{"
      "\"triggerCharacters\":[\".\"]"
      "},"
      "\"definitionProvider\":true,"
      "\"semanticTokensProvider\":{"
      "\"legend\":{"
      "\"tokenTypes\":["
      "\"keyword\",\"string\",\"comment\",\"number\","
      "\"variable\",\"function\",\"class\",\"operator\","
      "\"query\",\"tag\""
      "],"
      "\"tokenModifiers\":["
      "\"declaration\",\"readonly\""
      "]"
      "},"
      "\"full\":true"
      "}"
      "},"
      "\"serverInfo\":{\"name\":\"bialet-lsp\",\"version\":\"0.1.0\"}"
      "}}",
      id);
  sendMessage(buf.data);
}

static int   diagCount = 0;
static LspDiagnostic diagnostics[256];

static void diagErrorCb(int line, int col, const char* message, void* userData) {
  (void)userData;
  if(diagCount >= 256)
    return;
  diagnostics[diagCount].line = line - 1;
  diagnostics[diagCount].col = col;
  diagnostics[diagCount].message = strdup(message);
  diagCount++;
}

static void clearDiagnostics(void) {
  for(int i = 0; i < diagCount; i++)
    free(diagnostics[i].message);
  diagCount = 0;
}

static void sendDiagnostics(const char* uri) {
  char escUri[4096];
  jsonEscape(uri, escUri, sizeof(escUri));

  bufReset();
  bufWriteF(
      "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
      "\"params\":{\"uri\":\"%s\",\"diagnostics\":[",
      escUri);

  int first = 1;
  for(int i = 0; i < diagCount; i++) {
    char escMsg[1024];
    jsonEscape(diagnostics[i].message, escMsg, sizeof(escMsg));
    if(!first)
      bufWrite(",", 1);
    bufWriteF(
        "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
        "\"end\":{\"line\":%d,\"character\":%d}},"
        "\"severity\":1,\"source\":\"bialet\",\"message\":\"%s\"}",
        diagnostics[i].line, diagnostics[i].col, diagnostics[i].line,
        diagnostics[i].col + 1, escMsg);
    first = 0;
  }
  bufWrite("]}}", 3);
  sendMessage(buf.data);
}

static void checkDocument(const char* uri, const char* text) {
  clearDiagnostics();
  wrenCheckSyntax(lspVM, text, diagErrorCb, NULL);
  wrenCollectGarbage(lspVM);
  sendDiagnostics(uri);
}

static void handleDidOpen(const char* json) {
  char* uri = jsonStringNested(json, "textDocument", "uri");
  char* text = jsonStringNested(json, "textDocument", "text");
  if(uri && text) {
    cacheDoc(uri, text);
    checkDocument(uri, text);
  }
  free(uri);
  free(text);
}

static void handleDidChange(const char* json) {
  char* uri = jsonStringNested(json, "textDocument", "uri");
  char* text = NULL;
  const char* changesKey = "\"contentChanges\"";
  const char* changesPos = strstr(json, changesKey);
  if(changesPos) {
    text = jsonString(changesPos, "text");
    if(!text)
      text = jsonString(json, "text");
  }
  if(uri && text) {
    cacheDoc(uri, text);
    checkDocument(uri, text);
  }
  free(uri);
  free(text);
}

static void handleDidSave(const char* json) {
  char* uri = jsonStringNested(json, "textDocument", "uri");
  if(uri) {
    struct stat st;
    char        path[4096];
    uriToPath(uri, path, sizeof(path));
    char* text = NULL;
    if(stat(path, &st) == 0 && st.st_size < MAX_BODY) {
      FILE* f = fopen(path, "r");
      if(f) {
        text = malloc(st.st_size + 1);
        if(text) {
          size_t n = fread(text, 1, st.st_size, f);
          text[n] = '\0';
        }
        fclose(f);
      }
    }
    if(text) {
      checkDocument(uri, text);
      free(text);
    }
    free(uri);
  }
}

static char* getDocText(const char* json) {
  char* uri = jsonStringNested(json, "textDocument", "uri");
  if(!uri)
    return NULL;

  char* cached = cachedDoc(uri);
  if(cached) {
    char* result = strdup(cached);
    free(uri);
    return result;
  }

  char path[4096];
  uriToPath(uri, path, sizeof(path));

  FILE* f = fopen(path, "r");
  if(!f) {
    free(uri);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  if(sz < 0 || sz > MAX_BODY) {
    fclose(f);
    free(uri);
    return NULL;
  }
  rewind(f);
  char* text = malloc(sz + 1);
  if(text) {
    size_t n = fread(text, 1, sz, f);
    text[n] = '\0';
    cacheDoc(uri, text);
  }
  fclose(f);
  free(uri);
  return text;
}

static void handleSemanticTokens(const char* json) {
  int   id = jsonInt(json, "id", 0);
  char* text = getDocText(json);

  bufReset();
  bufWriteF("{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"data\":[", id);

  if(text) {
    tokenizeWren(text);
    int prevLine = 0;
    int prevCol = 0;
    int first = 1;
    for(int i = 0; i < stokenCount; i++) {
      int dLine = stokens[i].line - prevLine;
      int dCol = (dLine == 0) ? (stokens[i].col - prevCol) : stokens[i].col;
      if(!first)
        bufWrite(",", 1);
      bufWriteF("%d,%d,%d,%d,%d", dLine, dCol, stokens[i].len,
                stokens[i].type, stokens[i].mod);
      prevLine = stokens[i].line;
      prevCol = stokens[i].col;
      first = 0;
    }
    free(text);
  }

  bufWrite("]}}", 3);
  sendMessage(buf.data);
}

static void handleCompletion(const char* json) {
  int id = jsonInt(json, "id", 0);

  bufReset();
  bufWriteF(
      "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"isIncomplete\":false,"
      "\"items\":[",
      id);

  int first = 1;
  for(int i = 0; completions[i].label; i++) {
    if(!first)
      bufWrite(",", 1);
    char escLabel[256], escDetail[512], escInsert[2048];
    jsonEscape(completions[i].label, escLabel, sizeof(escLabel));
    jsonEscape(completions[i].detail, escDetail, sizeof(escDetail));
    jsonEscape(completions[i].insertText, escInsert, sizeof(escInsert));
    bufWriteF(
        "{\"label\":\"%s\",\"kind\":%d,\"detail\":\"%s\","
        "\"insertText\":\"%s\",\"insertTextFormat\":2}",
        escLabel, completions[i].kind, escDetail, escInsert);
    first = 0;
  }
  bufWrite("]}}", 3);
  sendMessage(buf.data);
}

static void handleDefinition(const char* json) {
  int id = jsonInt(json, "id", 0);
  char* uri = jsonStringNested(json, "textDocument", "uri");
  int   line = jsonIntNested(json, "position", "line", 0);
  (void)jsonIntNested(json, "position", "character", 0);

  char path[4096];
  if(uri)
    uriToPath(uri, path, sizeof(path));

  char* text = NULL;
  if(uri && fileExists(path)) {
    FILE* f = fopen(path, "r");
    if(f) {
      fseek(f, 0, SEEK_END);
      long sz = ftell(f);
      if(sz >= 0 && sz < MAX_BODY) {
        rewind(f);
        text = malloc(sz + 1);
        if(text) {
          long n = fread(text, 1, sz, f);
          text[n] = '\0';
        }
      }
      fclose(f);
    }
  }

  char* target = NULL;
  int   targetLine = 0;
  int   targetCol = 0;

  if(text) {
    const char* cur = text;
    int         cl = 0;
    while(cl < line && *cur) {
      if(*cur == '\n')
        cl++;
      cur++;
    }
    const char* ls = cur;
    while(*cur && *cur != '\n')
      cur++;
    int   llen = (int)(cur - ls);
    char* lb = malloc(llen + 1);
    if(lb) {
      memcpy(lb, ls, llen);
      lb[llen] = '\0';
      const char* imp = strstr(lb, "import ");
      if(imp) {
        const char* q1 = strchr(imp + 7, '"');
        if(q1) {
          const char* q2 = strchr(q1 + 1, '"');
          if(q2) {
            int   ipLen = (int)(q2 - q1 - 1);
            char* ip = malloc(ipLen + 1);
            memcpy(ip, q1 + 1, ipLen);
            ip[ipLen] = '\0';
            target = findImportFile(path, ip);
            free(ip);
          }
        }
      }
      free(lb);
    }
  }

  bufReset();
  bufWriteF("{\"jsonrpc\":\"2.0\",\"id\":%d,", id);
  if(target) {
    char tu[4096], et[4096];
    snprintf(tu, sizeof(tu), "file://%s", target);
    jsonEscape(tu, et, sizeof(et));
    bufWriteF(
        "\"result\":{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,"
        "\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}}",
        et, targetLine, targetCol, targetLine, targetCol + 1);
    free(target);
  } else {
    bufWrite("\"result\":null}", 14);
  }
  sendMessage(buf.data);
  free(text);
  free(uri);
}

static void handleShutdown(const char* json) {
  int id = jsonInt(json, "id", 0);
  bufReset();
  bufWriteF("{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
  sendMessage(buf.data);
  wrenFreeVM(lspVM);
  clearDiagnostics();
  exit(0);
}

static void dispatch(const char* json) {
  char* method = jsonString(json, "method");
  if(!method) {
    int id = jsonInt(json, "id", 0);
    if(id) {
      bufReset();
      bufWriteF(
          "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":-32600,"
          "\"message\":\"Invalid Request\"}}",
          id);
      sendMessage(buf.data);
    }
    return;
  }

  if(strcmp(method, "initialize") == 0)
    handleInitialize(json);
  else if(strcmp(method, "textDocument/didOpen") == 0)
    handleDidOpen(json);
  else if(strcmp(method, "textDocument/didChange") == 0)
    handleDidChange(json);
  else if(strcmp(method, "textDocument/didSave") == 0)
    handleDidSave(json);
  else if(strcmp(method, "textDocument/completion") == 0)
    handleCompletion(json);
  else if(strcmp(method, "textDocument/definition") == 0)
    handleDefinition(json);
  else if(strcmp(method, "textDocument/semanticTokens/full") == 0)
    handleSemanticTokens(json);
  else if(strcmp(method, "shutdown") == 0)
    handleShutdown(json);

  free(method);
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if(!getcwd(projectRoot, sizeof(projectRoot)))
    strcpy(projectRoot, ".");

  WrenConfiguration config;
  wrenInitConfiguration(&config);
  lspVM = wrenNewVM(&config);

  fprintf(stderr, "bialet-lsp: started\n");

  while(1) {
    int len = readHeader();
    if(len < 0)
      break;
    char* body = NULL;
    if(readBody(len, &body) != 0)
      break;
    dispatch(body);
    free(body);
  }

  wrenFreeVM(lspVM);
  clearDiagnostics();
  return 0;
}
