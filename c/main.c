#include "app.h"
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif

#ifdef _WIN32
static unsigned __stdcall sweeper_thread(void* unused) {
#else
static void* sweeper_thread(void* unused) {
#endif
    (void)unused;
    for (;;) {
        Sleep(2000);
        db_lock();
        attempt_sweep();
        db_unlock();
    }
    return 0;
}

#ifdef _WIN32
static unsigned __stdcall keeper_thread(void* unused) {
#else
static void* keeper_thread(void* unused) {
#endif
    (void)unused;
    for (;;) {
        Sleep(180000);
        db_lock();
        ops_backup();
        db_unlock();
    }
    return 0;
}

int main(void) {
    const char* env_port;
    int port = 8080;
    paths_init();
    srand((unsigned)time(NULL));
    if (!sha256_self_test() || !js_self_test()) {
        fprintf(stderr, "Kiem tra nen that bai\n");
        return 1;
    }
    if (!db_open()) return 1;
    seed_if_empty();
    stats_start();
    db_lock();
    ops_backup();
    db_unlock();
    register_routes();
#ifdef _WIN32
    {
        uintptr_t sweeper = _beginthreadex(NULL, 0, sweeper_thread, NULL, 0, NULL);
        uintptr_t keeper = _beginthreadex(NULL, 0, keeper_thread, NULL, 0, NULL);
        if (sweeper) CloseHandle((HANDLE)sweeper);
        if (keeper) CloseHandle((HANDLE)keeper);
    }
#else
    {
        pthread_t sweeper;
        pthread_t keeper;
        pthread_create(&sweeper, NULL, sweeper_thread, NULL);
        pthread_create(&keeper, NULL, keeper_thread, NULL);
        pthread_detach(sweeper);
        pthread_detach(keeper);
    }
#endif
    env_port = getenv("PORT");
    if (env_port && atoi(env_port) > 0 && atoi(env_port) < 65536) port = atoi(env_port);
    if (!http_serve(port)) return 1;
    db_close();
    return 0;
}
