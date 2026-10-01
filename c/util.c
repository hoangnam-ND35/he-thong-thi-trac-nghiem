#include "util.h"

#include "sha256.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

char g_root[MAX_PATH];

void paths_init(void) {
    char full[MAX_PATH];
    const char* guess = ".";
    if (!file_exists("web\\index.html") && file_exists("..\\web\\index.html")) guess = "..";
    GetFullPathNameA(guess, MAX_PATH, full, NULL);
    copy_str(g_root, sizeof(g_root), full);
}

void path_under(char* out, size_t cap, const char* relative) {
    snprintf(out, cap, "%s\\%s", g_root, relative);
}

long long now_sec(void) {
    return (long long)time(NULL);
}

void random_hex(size_t nbytes, char* out) {
    static const char* hex = "0123456789abcdef";
    size_t i;
    for (i = 0; i < nbytes; i++) {
        unsigned value = ((unsigned)rand() ^ (GetTickCount() + (unsigned)i * 131u)) & 255u;
        out[i * 2] = hex[value >> 4];
        out[i * 2 + 1] = hex[value & 15];
    }
    out[nbytes * 2] = 0;
}

void hash_password(const char* salt, const char* password, char out[65]) {
    char material[512];
    int n = snprintf(material, sizeof(material), "%s:%s", salt ? salt : "", password ? password : "");
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(material)) n = (int)sizeof(material) - 1;
    sha256_hex(material, (size_t)n, out);
}

int constant_equal(const char* a, const char* b) {
    size_t i;
    unsigned diff = 0;
    size_t na = a ? strlen(a) : 0;
    size_t nb = b ? strlen(b) : 0;
    size_t n = na > nb ? na : nb;
    if (na != nb) diff = 1;
    for (i = 0; i < n; i++) {
        unsigned char ca = i < na ? (unsigned char)a[i] : 0;
        unsigned char cb = i < nb ? (unsigned char)b[i] : 0;
        diff |= (unsigned)(ca ^ cb);
    }
    return diff == 0;
}

const char* password_problem(const char* password) {
    size_t n = password ? strlen(password) : 0;
    size_t i;
    int letter = 0;
    int digit = 0;
    if (n < 8 || n > 64) return "Mật khẩu cần 8 đến 64 ký tự, gồm cả chữ và số";
    for (i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)password[i];
        if (ch >= '0' && ch <= '9') digit = 1;
        else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch >= 128) letter = 1;
    }
    if (!letter || !digit) return "Mật khẩu cần 8 đến 64 ký tự, gồm cả chữ và số";
    return NULL;
}

void copy_str(char* dest, size_t cap, const char* src) {
    size_t i = 0;
    if (!cap) return;
    if (!src) src = "";
    while (src[i] && i + 1 < cap) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = 0;
}

void trim_copy(char* dest, size_t cap, const char* src) {
    size_t n;
    if (!src) src = "";
    while (*src == ' ' || *src == '\n' || *src == '\r' || *src == '\t') src++;
    copy_str(dest, cap, src);
    n = strlen(dest);
    while (n > 0 && (dest[n - 1] == ' ' || dest[n - 1] == '\n' || dest[n - 1] == '\r' || dest[n - 1] == '\t')) dest[--n] = 0;
}

char* read_file(const char* path, size_t* out_len) {
    FILE* file = fopen(path, "rb");
    char* data;
    long size;
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    data = (char*)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }
    if (size && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    data[size] = 0;
    fclose(file);
    if (out_len) *out_len = (size_t)size;
    return data;
}

int file_exists(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

void ensure_dir(const char* path) {
    _mkdir(path);
}

int query_get(const char* query, const char* key, char* out, size_t cap) {
    size_t key_len;
    const char* p;
    size_t n = 0;
    if (!query || !key || !out || cap == 0) return 0;
    key_len = strlen(key);
    p = query;
    out[0] = 0;
    while (*p) {
        if ((p == query || p[-1] == '&') && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            while (*p && *p != '&' && n + 1 < cap) {
                if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
                    int hi = isdigit((unsigned char)p[1]) ? p[1] - '0' : tolower((unsigned char)p[1]) - 'a' + 10;
                    int lo = isdigit((unsigned char)p[2]) ? p[2] - '0' : tolower((unsigned char)p[2]) - 'a' + 10;
                    out[n++] = (char)((hi << 4) | lo);
                    p += 3;
                } else if (*p == '+') {
                    out[n++] = ' ';
                    p++;
                } else out[n++] = *p++;
            }
            out[n] = 0;
            return 1;
        }
        p++;
    }
    return 0;
}

int query_int(const char* query, const char* key) {
    char buf[64];
    if (!query_get(query, key, buf, sizeof(buf))) return 0;
    return atoi(buf);
}
