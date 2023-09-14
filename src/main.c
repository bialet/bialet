#include "wren_vm.h"
#include "mongoose.h"

/*
 * WrenVM
 */
static void writeFn(WrenVM* vm, const char* text)
{
    wrenBuffer = StrAppend(wrenBuffer, "\n", text);
}

static WrenLoadModuleResult loadModuleFn(WrenVM* vm, const char* name) {

    char module[100];
    strcpy(module, zDir);
    strcat(module, "/");
    strcat(module, name);
    strcat(module, ".wren");
    char * buffer = 0;
    long length;
    FILE * f = fopen (module, "rb");
    if (f)
    {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = malloc (length + 1);
        if (buffer)
        {
            fread (buffer, 1, length, f);
        }
        fclose (f);
    }

    WrenLoadModuleResult result = {0};
    result.source = NULL;

    if (buffer)
    {
        buffer[length] = '\0';
        result.source = buffer;
    }

    return result;
}

void errorFn(WrenVM* vm, WrenErrorType errorType,
        const char* module, const int line,
        const char* msg)
{
    switch (errorType)
    {
        case WREN_ERROR_COMPILE:
            {
                printf("[%s line %d] [Error] %s\n", module, line, msg);
            }
            break;
        case WREN_ERROR_STACK_TRACE:
            {
                printf("[%s line %d] in %s\n", module, line, msg);
            }
            break;
        case WREN_ERROR_RUNTIME:
            {
                printf("[Runtime Error] %s\n", msg);
            }
            break;
    }
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/api/hello")) {              // On /api/hello requests,
                const char* module = "main";
                WrenVM* vm = 0;
                WrenConfiguration config;
                wrenInitConfiguration(&config);
                config.writeFn = &writeFn;
                config.errorFn = &errorFn;
                config.loadModuleFn = &loadModuleFn;
                vm = wrenNewVM(&config);


                // Parse template tags!

                WrenInterpretResult result = wrenInterpret(vm, module, script);
                wrenFreeVM(vm);
                closeConnection = 1;

                switch (result) {
                    case WREN_RESULT_SUCCESS:
                        {
                            SetTimeout(5, 806); /* LOG: Timeout send static file */
                            StartResponse("200 OK");
                            int length = strlen(wrenBuffer);
                            nOut += printf("Content-length: %d\r\n\r\n", length);
                            nOut += printf("%s", wrenBuffer);
                            free(wrenBuffer);
                            wrenBuffer = 0;
                        }
                        break;
                    case WREN_RESULT_COMPILE_ERROR:
                    case WREN_RESULT_RUNTIME_ERROR:
                    default:
                        {
                            CgiError();
                        }
                        break;
                }
      mg_http_reply(c, 200, "", "{%m:%d}\n",
                    MG_ESC("status"), 1);                   // Send dynamic JSON response
    } else {                                                // For all other URIs,
      struct mg_http_serve_opts opts = {.root_dir = "."};   // Serve files
      mg_http_serve_dir(c, hm, &opts);                      // From root_dir
    }
  }
}

int main() {

  struct mg_mgr mgr;
  mg_mgr_init(&mgr);                                      // Init manager
  mg_http_listen(&mgr, "http://0.0.0.0:8080", fn, NULL);  // Setup listener
  for (;;) mg_mgr_poll(&mgr, 1000);                       // Infinite event loop

  return 0;
}

