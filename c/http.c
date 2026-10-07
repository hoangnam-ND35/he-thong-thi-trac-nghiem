#include "http.h"

#include "app.h"
#include "db.h"
#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define OES_THREAD_RET unsigned
#define OES_THREAD_CALL __stdcall
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define closesocket close
#define OES_THREAD_RET void*
#define OES_THREAD_CALL
#endif

#define MAX_ROUTES 96
#define MAX_HEADER (1024 * 1024)
#define MAX_BODY (5 * 1024 * 1024)

typedef struct Route {
    const char* method;
    const char* pattern;
    Handler handler;
} Route;

static Route g_routes[MAX_ROUTES];
static int g_route_count = 0;

typedef struct CacheItem {
    char key[520];
    long long mtime;
    char type[64];
    char* body;
    int len;
    int used;
} CacheItem;

static CacheItem g_cache[48];
static CRITICAL_SECTION g_cache_lock;
static int g_cache_ready = 0;
static int g_listen_port = 8080;

void http_add_route(const char* method, const char* pattern, Handler handler) {
    if (g_route_count >= MAX_ROUTES) return;
    g_routes[g_route_count].method = method;
    g_routes[g_route_count].pattern = pattern;
    g_routes[g_route_count].handler = handler;
    g_route_count++;
}

void reply_fail(Response* response, int status, const char* message) {
    W w;
    w_init(&w);
    w_obj(&w);
    w_key(&w, "ok");
    w_bool(&w, 0);
    w_key(&w, "error");
    w_str(&w, message ? message : "Lỗi");
    w_end(&w);
    free(response->body);
    response->status = status;
    response->body = w_take(&w);
    response->body_len = (int)strlen(response->body);
    snprintf(response->type, sizeof(response->type), "application/json; charset=utf-8");
}

void reply_begin(W* w) {
    w_init(w);
    w_raw(w, "{\"ok\":true,\"data\":");
}

void reply_json(Response* response, W* w) {
    w_raw(w, "}");
    free(response->body);
    response->status = 200;
    response->body = w_take(w);
    response->body_len = (int)strlen(response->body);
    snprintf(response->type, sizeof(response->type), "application/json; charset=utf-8");
}

void reply_raw(Response* response, int status, const char* type, char* body, int length, const char* filename) {
    free(response->body);
    response->status = status;
    response->body = body;
    response->body_len = length;
    snprintf(response->type, sizeof(response->type), "%s", type);
    if (filename && filename[0]) {
        snprintf(response->extra + strlen(response->extra), sizeof(response->extra) - strlen(response->extra),
                 "Content-Disposition: attachment; filename=\"%s\"\r\n", filename);
    }
}

void cookie_session(Response* response, const char* token, int max_age) {
    const char* secure = "";
    if (getenv("RENDER") || getenv("FORCE_SECURE_COOKIE")) secure = "; Secure";
    snprintf(response->extra + strlen(response->extra), sizeof(response->extra) - strlen(response->extra),
             "Set-Cookie: session=%s; Path=/; Max-Age=%d; HttpOnly; SameSite=Lax%s\r\n",
             token ? token : "", max_age, secure);
}

int http_listen_port(void) {
    return g_listen_port > 0 ? g_listen_port : 8080;
}

void http_write_lan_ips(W* w) {
    char host[256];
    struct addrinfo hints;
    struct addrinfo* res = NULL;
    struct addrinfo* p;
    w_key(w, "port");
    w_num(w, http_listen_port());
    w_key(w, "lanIps");
    w_arr(w);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (gethostname(host, sizeof(host)) == 0 && getaddrinfo(host, NULL, &hints, &res) == 0) {
        for (p = res; p; p = p->ai_next) {
            char ip[64];
            struct sockaddr_in* sa = (struct sockaddr_in*)p->ai_addr;
            if (!sa) continue;
#ifdef _WIN32
            copy_str(ip, sizeof(ip), inet_ntoa(sa->sin_addr));
#else
            if (!inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) continue;
#endif
            if (!strncmp(ip, "127.", 4)) continue;
            w_str(w, ip);
        }
        freeaddrinfo(res);
    }
    w_end(w);
}

static int match_route(const char* pattern, const char* path, int* id) {
    const char* a = pattern;
    const char* b = path;
    *id = 0;
    while (*a && *b) {
        if (*a == '#') {
            if (!isdigit((unsigned char)*b)) return 0;
            while (isdigit((unsigned char)*b)) {
                *id = *id * 10 + (*b - '0');
                b++;
            }
            a++;
            continue;
        }
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static const char* mime_of(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!_stricmp(dot, ".html")) return "text/html; charset=utf-8";
    if (!_stricmp(dot, ".css")) return "text/css; charset=utf-8";
    if (!_stricmp(dot, ".js")) return "application/javascript; charset=utf-8";
    if (!_stricmp(dot, ".svg")) return "image/svg+xml";
    if (!_stricmp(dot, ".png")) return "image/png";
    if (!_stricmp(dot, ".jpg") || !_stricmp(dot, ".jpeg")) return "image/jpeg";
    if (!_stricmp(dot, ".ico")) return "image/x-icon";
    return "application/octet-stream";
}

static void header_value(const char* headers, const char* name, char* out, size_t cap) {
    size_t name_len = strlen(name);
    const char* p = headers;
    out[0] = 0;
    while (*p) {
        if (_strnicmp(p, name, name_len) == 0 && p[name_len] == ':') {
            size_t n = 0;
            p += name_len + 1;
            while (*p == ' ') p++;
            while (*p && *p != '\r' && *p != '\n' && n + 1 < cap) out[n++] = *p++;
            out[n] = 0;
            return;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
}

static void take_cookie(const char* cookie, char* out, size_t cap) {
    const char* p = cookie ? strstr(cookie, "session=") : NULL;
    size_t n = 0;
    out[0] = 0;
    if (!p) return;
    p += 8;
    while (*p && *p != ';' && n + 1 < cap) out[n++] = *p++;
    out[n] = 0;
}

static long long file_mtime(const char* path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA info;
    ULARGE_INTEGER stamp;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info)) return 0;
    stamp.LowPart = info.ftLastWriteTime.dwLowDateTime;
    stamp.HighPart = info.ftLastWriteTime.dwHighDateTime;
    return (long long)stamp.QuadPart;
#else
    struct stat info;
    if (stat(path, &info) != 0) return 0;
    return (long long)info.st_mtime;
#endif
}

static int serve_static(const char* url_path, Response* response) {
    char relative[520];
    char full[MAX_PATH];
    char* slash;
    const char* web_path = url_path;
    char* data;
    size_t len = 0;
    int i;
    long long mtime;
    if (!web_path[0] || strcmp(web_path, "/") == 0) web_path = "/index.html";
    if (strstr(web_path, "..") || strchr(web_path, ':')) {
        reply_fail(response, 400, "Đường dẫn không hợp lệ");
        return 1;
    }
    snprintf(relative, sizeof(relative), "web%s", web_path);
    for (slash = relative; *slash; slash++) {
        if (*slash == '/') *slash = '\\';
    }
    path_under(full, sizeof(full), relative);
    mtime = file_mtime(full);
    if (!mtime) return 0;
    EnterCriticalSection(&g_cache_lock);
    for (i = 0; i < 48; i++) {
        if (g_cache[i].used && strcmp(g_cache[i].key, full) == 0 && g_cache[i].mtime == mtime) {
            response->body = (char*)malloc((size_t)g_cache[i].len + 1);
            if (response->body) memcpy(response->body, g_cache[i].body, (size_t)g_cache[i].len);
            response->body_len = g_cache[i].len;
            if (response->body) response->body[response->body_len] = 0;
            snprintf(response->type, sizeof(response->type), "%s", g_cache[i].type);
            response->status = 200;
            LeaveCriticalSection(&g_cache_lock);
            return 1;
        }
    }
    LeaveCriticalSection(&g_cache_lock);
    data = read_file(full, &len);
    if (!data) return 0;
    response->status = 200;
    response->body = data;
    response->body_len = (int)len;
    snprintf(response->type, sizeof(response->type), "%s", mime_of(full));
    EnterCriticalSection(&g_cache_lock);
    for (i = 0; i < 48; i++) {
        if (!g_cache[i].used || strcmp(g_cache[i].key, full) == 0) {
            free(g_cache[i].body);
            g_cache[i].used = 1;
            g_cache[i].mtime = mtime;
            copy_str(g_cache[i].key, sizeof(g_cache[i].key), full);
            copy_str(g_cache[i].type, sizeof(g_cache[i].type), response->type);
            g_cache[i].len = (int)len;
            g_cache[i].body = (char*)malloc(len + 1);
            if (g_cache[i].body) memcpy(g_cache[i].body, data, len + 1);
            break;
        }
    }
    LeaveCriticalSection(&g_cache_lock);
    return 1;
}

static void dispatch(Request* request, Response* response) {
    int i;
    if (strcmp(request->path, "/favicon.ico") == 0) {
        response->status = 204;
        response->body = (char*)calloc(1, 1);
        response->body_len = 0;
        snprintf(response->type, sizeof(response->type), "image/x-icon");
        return;
    }
    if (strncmp(request->path, "/api/", 5) != 0) {
        if (!serve_static(request->path, response)) reply_fail(response, 404, "Không tìm thấy trang");
        return;
    }
    if ((strcmp(request->method, "POST") == 0 || strcmp(request->method, "PUT") == 0) &&
        strcmp(request->csrf, "OnlineExam") != 0) {
        reply_fail(response, 403, "Thiếu tiêu đề bảo mật");
        return;
    }
    if ((strcmp(request->method, "POST") == 0 || strcmp(request->method, "PUT") == 0) && request->body && request->body[0]) {
        const char* err = NULL;
        request->json = js_parse(request->body, &err);
        if (!request->json) {
            reply_fail(response, 400, err ? err : "JSON không hợp lệ");
            return;
        }
    }
    for (i = 0; i < g_route_count; i++) {
        if (strcmp(g_routes[i].method, request->method) != 0) continue;
        if (!match_route(g_routes[i].pattern, request->path, &request->id)) continue;
        db_lock();
        g_routes[i].handler(request, response);
        db_unlock();
        return;
    }
    reply_fail(response, 404, "Không có API này");
}

static int send_all(SOCKET sock, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, data + sent, len - sent, 0);
        if (n <= 0) return 0;
        sent += n;
    }
    return 1;
}

static void write_response(SOCKET sock, Response* response, long long elapsed_ms) {
    char head[4096];
    const char* reason = "OK";
    int n;
    if (response->status == 400) reason = "Bad Request";
    else if (response->status == 401) reason = "Unauthorized";
    else if (response->status == 403) reason = "Forbidden";
    else if (response->status == 404) reason = "Not Found";
    else if (response->status == 204) reason = "No Content";
    else if (response->status >= 500) reason = "Error";
    if (!response->type[0]) snprintf(response->type, sizeof(response->type), "text/plain; charset=utf-8");
    n = snprintf(head, sizeof(head),
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %d\r\n"
                 "Cache-Control: no-cache\r\n"
                 "X-Content-Type-Options: nosniff\r\n"
                 "X-Frame-Options: DENY\r\n"
                 "Referrer-Policy: same-origin\r\n"
                 "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'self'; form-action 'self'\r\n"
                 "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n"
                 "Server-Timing: app;dur=%lld\r\n"
                 "%s"
                 "Connection: close\r\n\r\n",
                 response->status, reason, response->type, response->body_len, elapsed_ms, response->extra);
    if (n > 0) send_all(sock, head, n);
    if (response->body && response->body_len > 0) send_all(sock, response->body, response->body_len);
}

static OES_THREAD_RET OES_THREAD_CALL client_thread(void* arg) {
    SOCKET sock = (SOCKET)(intptr_t)arg;
    char* buf = (char*)malloc(MAX_HEADER);
    int used = 0;
    int header_end = -1;
    Request request;
    Response response;
    char header_copy[8192];
    char cookie[512];
    int content_length = 0;
#ifdef _WIN32
    LARGE_INTEGER started;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&started);
#else
    struct timespec started;
    clock_gettime(CLOCK_MONOTONIC, &started);
#endif
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    response.status = 500;
    if (!buf) {
        closesocket(sock);
        return 0;
    }
    while (used < MAX_HEADER - 1) {
        int n = recv(sock, buf + used, MAX_HEADER - 1 - used, 0);
        char* mark;
        if (n <= 0) break;
        used += n;
        buf[used] = 0;
        mark = strstr(buf, "\r\n\r\n");
        if (mark) {
            header_end = (int)(mark - buf);
            break;
        }
    }
    if (header_end < 0) {
        free(buf);
        closesocket(sock);
        return 0;
    }
    {
        char line[1024];
        int i = 0;
        while (i < header_end && buf[i] != '\r' && i < (int)sizeof(line) - 1) {
            line[i] = buf[i];
            i++;
        }
        line[i] = 0;
        sscanf(line, "%7s %511s", request.method, request.path);
    }
    {
        char* q = strchr(request.path, '?');
        if (q) {
            copy_str(request.query, sizeof(request.query), q + 1);
            *q = 0;
        }
    }
    copy_str(header_copy, sizeof(header_copy), buf);
    header_value(header_copy, "Content-Length", cookie, sizeof(cookie));
    content_length = atoi(cookie);
    header_value(header_copy, "Cookie", cookie, sizeof(cookie));
    take_cookie(cookie, request.session, sizeof(request.session));
    header_value(header_copy, "X-Requested-With", request.csrf, sizeof(request.csrf));
    {
        char forwarded[160];
        char* comma;
        header_value(header_copy, "X-Forwarded-For", forwarded, sizeof(forwarded));
        comma = strchr(forwarded, ',');
        if (comma) *comma = 0;
        trim_copy(request.ip, sizeof(request.ip), forwarded);
        if (!request.ip[0]) copy_str(request.ip, sizeof(request.ip), "127.0.0.1");
    }
    if (content_length < 0 || content_length > MAX_BODY) {
        reply_fail(&response, 400, "Nội dung quá lớn");
    } else {
        int have = used - (header_end + 4);
        int need = content_length;
        request.body = (char*)calloc(1, (size_t)need + 1);
        if (have > 0 && request.body) memcpy(request.body, buf + header_end + 4, (size_t)(have > need ? need : have));
        while (request.body && have < need) {
            int n = recv(sock, request.body + have, need - have, 0);
            if (n <= 0) break;
            have += n;
        }
        if (request.body) request.body[need] = 0;
        request.body_len = need;
        if (have < need) reply_fail(&response, 400, "Thiếu nội dung yêu cầu");
        else dispatch(&request, &response);
    }
    if (!response.body) {
        response.body = (char*)calloc(1, 1);
        response.body_len = 0;
    }
    {
        double elapsed;
#ifdef _WIN32
        LARGE_INTEGER ended;
        QueryPerformanceCounter(&ended);
        elapsed = (double)(ended.QuadPart - started.QuadPart) * 1000.0 / (double)freq.QuadPart;
#else
        struct timespec ended;
        clock_gettime(CLOCK_MONOTONIC, &ended);
        elapsed = (double)(ended.tv_sec - started.tv_sec) * 1000.0 + (double)(ended.tv_nsec - started.tv_nsec) / 1000000.0;
#endif
        if (strncmp(request.path, "/api/", 5) == 0) stats_note(elapsed);
        write_response(sock, &response, (long long)(elapsed + 0.5));
    }
    js_free(request.json);
    free(request.body);
    free(response.body);
    free(buf);
    closesocket(sock);
    return 0;
}

int http_serve(int port) {
    SOCKET server;
    struct sockaddr_in addr;
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
#endif
    InitializeCriticalSection(&g_cache_lock);
    g_cache_ready = 1;
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) return 0;
    {
        int yes = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "Khong mo duoc cong %d\n", port);
        closesocket(server);
        return 0;
    }
    listen(server, 64);
    g_listen_port = port;
    printf("Dang lang nghe http://127.0.0.1:%d\n", port);
    {
        char host[256];
        struct addrinfo hints;
        struct addrinfo* res = NULL;
        struct addrinfo* p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (gethostname(host, sizeof(host)) == 0 && getaddrinfo(host, NULL, &hints, &res) == 0) {
            for (p = res; p; p = p->ai_next) {
                char ip[64];
                struct sockaddr_in* sa = (struct sockaddr_in*)p->ai_addr;
                if (!sa) continue;
#ifdef _WIN32
                copy_str(ip, sizeof(ip), inet_ntoa(sa->sin_addr));
#else
                inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
#endif
                if (!strncmp(ip, "127.", 4)) continue;
                printf("Mo tren dien thoai/may khac (cung Wi-Fi): http://%s:%d\n", ip, port);
            }
            freeaddrinfo(res);
        }
    }
    fflush(stdout);
    for (;;) {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;
#ifdef _WIN32
        {
            uintptr_t thread = _beginthreadex(NULL, 0, client_thread, (void*)(intptr_t)client, 0, NULL);
            if (thread) CloseHandle((HANDLE)thread);
            else closesocket(client);
        }
#else
        {
            pthread_t thread;
            if (pthread_create(&thread, NULL, client_thread, (void*)(intptr_t)client) == 0) pthread_detach(thread);
            else closesocket(client);
        }
#endif
    }
}
