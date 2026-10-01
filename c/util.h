#ifndef OES_UTIL_H
#define OES_UTIL_H

#include <stddef.h>

void paths_init(void);
void path_under(char* out, size_t cap, const char* relative);
long long now_sec(void);
void random_hex(size_t nbytes, char* out);
void hash_password(const char* salt, const char* password, char out[65]);
int constant_equal(const char* a, const char* b);
const char* password_problem(const char* password);
void copy_str(char* dest, size_t cap, const char* src);
void trim_copy(char* dest, size_t cap, const char* src);
char* read_file(const char* path, size_t* out_len);
int file_exists(const char* path);
void ensure_dir(const char* path);
int query_get(const char* query, const char* key, char* out, size_t cap);
int query_int(const char* query, const char* key);

#endif
