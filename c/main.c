#include "app.h"
#include "sha256.h"

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static unsigned __stdcall sweeper_thread(void* unused) {
    (void)unused;
    for (;;) {
        Sleep(2000);
        db_lock();
        attempt_sweep();
        db_unlock();
    }
    return 0;
}

static unsigned __stdcall keeper_thread(void* unused) {
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
    uintptr_t sweeper;
    uintptr_t keeper;
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
    sweeper = _beginthreadex(NULL, 0, sweeper_thread, NULL, 0, NULL);
    keeper = _beginthreadex(NULL, 0, keeper_thread, NULL, 0, NULL);
    if (sweeper) CloseHandle((HANDLE)sweeper);
    if (keeper) CloseHandle((HANDLE)keeper);
    if (!http_serve(8080)) return 1;
    db_close();
    return 0;
}
