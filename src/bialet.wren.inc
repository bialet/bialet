// Generated automatically from src/bialet.wren. Do not edit.
static const char* bialetModuleSource =
"class Response {\n"
"  static init() {\n"
"    __headers = {\"Content-Type\": \"text/html\"}\n"
"    __status = 200\n"
"    __out = \"\"\n"
"  }\n"
"  static out(out) { __out = __out + \"\\r\\n\" + out }\n"
"  static out() { __out.trim() }\n"
"  static status(status) { __status = status }\n"
"  static status() { __status }\n"
"  static headers() { __headers.keys.map{|k| k + \": \" + __headers[k] + \"\\r\\n\"}.join() }\n"
"  static header(header, value) {\n"
"    __headers[header] = value\n"
"  }\n"
"}\n"
"\n"
"Response.init()\n";
