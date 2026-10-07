#ifndef OES_HTTP_H
#define OES_HTTP_H

#include "json.h"

typedef struct Request {
    char method[8];
    char path[512];
    char query[2048];
    char* body;
    int body_len;
    char session[80];
    char ip[64];
    char csrf[64];
    int id;
    Js* json;
} Request;

typedef struct Response {
    int status;
    char* body;
    int body_len;
    char type[96];
    char extra[2048];
} Response;

typedef void (*Handler)(Request* request, Response* response);

void http_add_route(const char* method, const char* pattern, Handler handler);
void reply_fail(Response* response, int status, const char* message);
void reply_begin(W* w);
void reply_json(Response* response, W* w);
void reply_raw(Response* response, int status, const char* type, char* body, int length, const char* filename);
void cookie_session(Response* response, const char* token, int max_age);
int http_listen_port(void);
void http_write_lan_ips(W* w);
int http_serve(int port);

#endif
