#ifndef OES_UTIL_H
#define OES_UTIL_H

#include <stddef.h>

#ifndef _WIN32
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#define MAX_PATH 512
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
typedef pthread_mutex_t CRITICAL_SECTION;
#define InitializeCriticalSection(lock) pthread_mutex_init((lock), NULL)
#define DeleteCriticalSection(lock) pthread_mutex_destroy(lock)
#define EnterCriticalSection(lock) pthread_mutex_lock(lock)
#define LeaveCriticalSection(lock) pthread_mutex_unlock(lock)
static inline void oes_sleep_ms(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
}
#define Sleep(ms) oes_sleep_ms((unsigned)(ms))
#endif

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
int list_subdirs(const char* directory, char names[][32], int cap);
void remove_dir_contents(const char* directory);
int query_get(const char* query, const char* key, char* out, size_t cap);
int query_int(const char* query, const char* key);

#endif
