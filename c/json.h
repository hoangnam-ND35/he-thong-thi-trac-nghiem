#ifndef OES_JSON_H
#define OES_JSON_H

#include <stddef.h>

typedef enum { JS_NULL, JS_BOOL, JS_NUM, JS_STR, JS_ARR, JS_OBJ } JsType;

typedef struct Js Js;
struct Js {
    JsType type;
    int b;
    double n;
    char* s;
    char** keys;
    Js** vals;
    int len;
    int cap;
};

Js* js_parse(const char* text, const char** err);
void js_free(Js* value);
const Js* js_get(const Js* object, const char* key);
int js_has(const Js* object, const char* key);
const char* js_str(const Js* object, const char* key, const char* fallback);
double js_num(const Js* object, const char* key, double fallback);
int js_bool(const Js* object, const char* key, int fallback);
int js_len(const Js* array);
const Js* js_at(const Js* array, int index);
int js_self_test(void);

typedef struct W {
    char* s;
    size_t n;
    size_t cap;
    unsigned char need[48];
    char kind[48];
    int depth;
} W;

void w_init(W* w);
void w_free(W* w);
char* w_take(W* w);
void w_raw(W* w, const char* text);
void w_obj(W* w);
void w_arr(W* w);
void w_end(W* w);
void w_key(W* w, const char* key);
void w_str(W* w, const char* value);
void w_num(W* w, double value);
void w_bool(W* w, int value);
void w_null(W* w);

#endif
