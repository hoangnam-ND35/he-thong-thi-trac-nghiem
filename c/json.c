#include "json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* xrealloc(void* ptr, size_t size) {
    void* next = realloc(ptr, size);
    if (!next) {
        fprintf(stderr, "Het bo nho\n");
        exit(1);
    }
    return next;
}

static Js* js_new(JsType type) {
    Js* value = (Js*)calloc(1, sizeof(Js));
    if (!value) exit(1);
    value->type = type;
    return value;
}

static void grow_child(Js* parent) {
    if (parent->len >= parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 4;
        parent->vals = (Js**)xrealloc(parent->vals, (size_t)parent->cap * sizeof(Js*));
        if (parent->type == JS_OBJ) {
            parent->keys = (char**)xrealloc(parent->keys, (size_t)parent->cap * sizeof(char*));
        }
    }
}

void js_free(Js* value) {
    int i;
    if (!value) return;
    free(value->s);
    for (i = 0; i < value->len; i++) {
        if (value->keys) free(value->keys[i]);
        js_free(value->vals[i]);
    }
    free(value->keys);
    free(value->vals);
    free(value);
}

typedef struct {
    const char* p;
    const char* err;
    int depth;
} Parser;

static void skip(Parser* parser) {
    while (*parser->p == ' ' || *parser->p == '\n' || *parser->p == '\r' || *parser->p == '\t') parser->p++;
}

static void append_byte(char** out, size_t* n, size_t* cap, unsigned char byte) {
    if (*n + 2 >= *cap) {
        *cap = *cap ? *cap * 2 : 32;
        *out = (char*)xrealloc(*out, *cap);
    }
    (*out)[(*n)++] = (char)byte;
    (*out)[*n] = 0;
}

static void append_utf8(char** out, size_t* n, size_t* cap, unsigned code) {
    if (code < 0x80) {
        append_byte(out, n, cap, (unsigned char)code);
    } else if (code < 0x800) {
        append_byte(out, n, cap, (unsigned char)(0xC0 | (code >> 6)));
        append_byte(out, n, cap, (unsigned char)(0x80 | (code & 0x3F)));
    } else {
        append_byte(out, n, cap, (unsigned char)(0xE0 | (code >> 12)));
        append_byte(out, n, cap, (unsigned char)(0x80 | ((code >> 6) & 0x3F)));
        append_byte(out, n, cap, (unsigned char)(0x80 | (code & 0x3F)));
    }
}

static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static int read_u4(Parser* parser, unsigned* code) {
    int i;
    *code = 0;
    for (i = 0; i < 4; i++) {
        int digit = hex_value(parser->p[i]);
        if (digit < 0) return 0;
        *code = (*code << 4) | (unsigned)digit;
    }
    parser->p += 4;
    return 1;
}

static char* parse_string(Parser* parser) {
    char* out = (char*)calloc(1, 32);
    size_t n = 0;
    size_t cap = 32;
    if (*parser->p != '"') {
        parser->err = "JSON không hợp lệ";
        free(out);
        return NULL;
    }
    parser->p++;
    while (*parser->p && *parser->p != '"') {
        unsigned char ch = (unsigned char)*parser->p++;
        if (ch == '\\') {
            char esc = *parser->p++;
            unsigned code = 0;
            if (esc == 'n') ch = '\n';
            else if (esc == 'r') ch = '\r';
            else if (esc == 't') ch = '\t';
            else if (esc == 'b') ch = '\b';
            else if (esc == 'f') ch = '\f';
            else if (esc == 'u') {
                if (!read_u4(parser, &code)) {
                    parser->err = "JSON không hợp lệ";
                    free(out);
                    return NULL;
                }
                append_utf8(&out, &n, &cap, code);
                continue;
            } else if (esc) ch = (unsigned char)esc;
            else {
                parser->err = "JSON không hợp lệ";
                free(out);
                return NULL;
            }
        }
        append_byte(&out, &n, &cap, ch);
    }
    if (*parser->p != '"') {
        parser->err = "JSON không hợp lệ";
        free(out);
        return NULL;
    }
    parser->p++;
    return out;
}

static Js* parse_value(Parser* parser);

static Js* parse_array(Parser* parser) {
    Js* array = js_new(JS_ARR);
    parser->p++;
    skip(parser);
    if (*parser->p == ']') {
        parser->p++;
        return array;
    }
    while (*parser->p) {
        Js* child;
        if (parser->depth > 32) {
            parser->err = "JSON quá sâu";
            js_free(array);
            return NULL;
        }
        child = parse_value(parser);
        if (!child) {
            js_free(array);
            return NULL;
        }
        grow_child(array);
        array->vals[array->len++] = child;
        skip(parser);
        if (*parser->p == ',') {
            parser->p++;
            skip(parser);
            continue;
        }
        if (*parser->p == ']') {
            parser->p++;
            return array;
        }
        break;
    }
    parser->err = "JSON không hợp lệ";
    js_free(array);
    return NULL;
}

static Js* parse_object(Parser* parser) {
    Js* object = js_new(JS_OBJ);
    parser->p++;
    skip(parser);
    if (*parser->p == '}') {
        parser->p++;
        return object;
    }
    while (*parser->p) {
        char* key;
        Js* child;
        skip(parser);
        key = parse_string(parser);
        if (!key) {
            js_free(object);
            return NULL;
        }
        skip(parser);
        if (*parser->p != ':') {
            parser->err = "JSON không hợp lệ";
            free(key);
            js_free(object);
            return NULL;
        }
        parser->p++;
        child = parse_value(parser);
        if (!child) {
            free(key);
            js_free(object);
            return NULL;
        }
        grow_child(object);
        object->keys[object->len] = key;
        object->vals[object->len] = child;
        object->len++;
        skip(parser);
        if (*parser->p == ',') {
            parser->p++;
            continue;
        }
        if (*parser->p == '}') {
            parser->p++;
            return object;
        }
        break;
    }
    parser->err = "JSON không hợp lệ";
    js_free(object);
    return NULL;
}

static Js* parse_value(Parser* parser) {
    Js* value;
    skip(parser);
    parser->depth++;
    if (*parser->p == '"') {
        value = js_new(JS_STR);
        value->s = parse_string(parser);
        if (!value->s) {
            js_free(value);
            value = NULL;
        }
    } else if (*parser->p == '{') {
        value = parse_object(parser);
    } else if (*parser->p == '[') {
        value = parse_array(parser);
    } else if (!strncmp(parser->p, "true", 4)) {
        value = js_new(JS_BOOL);
        value->b = 1;
        parser->p += 4;
    } else if (!strncmp(parser->p, "false", 5)) {
        value = js_new(JS_BOOL);
        parser->p += 5;
    } else if (!strncmp(parser->p, "null", 4)) {
        value = js_new(JS_NULL);
        parser->p += 4;
    } else if (*parser->p == '-' || isdigit((unsigned char)*parser->p)) {
        char* end = NULL;
        value = js_new(JS_NUM);
        value->n = strtod(parser->p, &end);
        if (end == parser->p) {
            parser->err = "JSON không hợp lệ";
            js_free(value);
            value = NULL;
        } else parser->p = end;
    } else {
        parser->err = "JSON không hợp lệ";
        value = NULL;
    }
    parser->depth--;
    return value;
}

Js* js_parse(const char* text, const char** err) {
    Parser parser;
    Js* value;
    parser.p = text ? text : "";
    parser.err = NULL;
    parser.depth = 0;
    value = parse_value(&parser);
    skip(&parser);
    if (value && *parser.p) {
        parser.err = "JSON không hợp lệ";
        js_free(value);
        value = NULL;
    }
    if (err) *err = value ? NULL : (parser.err ? parser.err : "JSON không hợp lệ");
    return value;
}

const Js* js_get(const Js* object, const char* key) {
    int i;
    if (!object || object->type != JS_OBJ || !key) return NULL;
    for (i = 0; i < object->len; i++) {
        if (strcmp(object->keys[i], key) == 0) return object->vals[i];
    }
    return NULL;
}

int js_has(const Js* object, const char* key) {
    return js_get(object, key) != NULL;
}

const char* js_str(const Js* object, const char* key, const char* fallback) {
    const Js* value = js_get(object, key);
    if (!value || value->type != JS_STR || !value->s) return fallback;
    return value->s;
}

double js_num(const Js* object, const char* key, double fallback) {
    const Js* value = js_get(object, key);
    if (!value) return fallback;
    if (value->type == JS_NUM) return value->n;
    if (value->type == JS_STR && value->s && value->s[0]) return atof(value->s);
    if (value->type == JS_BOOL) return value->b;
    return fallback;
}

int js_bool(const Js* object, const char* key, int fallback) {
    const Js* value = js_get(object, key);
    if (!value) return fallback;
    if (value->type == JS_BOOL) return value->b;
    if (value->type == JS_NUM) return value->n != 0;
    if (value->type == JS_STR && value->s) return strcmp(value->s, "true") == 0 || strcmp(value->s, "1") == 0;
    return fallback;
}

int js_len(const Js* array) {
    return array ? array->len : 0;
}

const Js* js_at(const Js* array, int index) {
    if (!array || index < 0 || index >= array->len) return NULL;
    return array->vals[index];
}

static void ensure(W* w, size_t extra) {
    if (w->n + extra + 1 < w->cap) return;
    while (w->n + extra + 1 >= w->cap) w->cap = w->cap ? w->cap * 2 : 256;
    w->s = (char*)xrealloc(w->s, w->cap);
}

void w_init(W* w) {
    memset(w, 0, sizeof(*w));
    w->cap = 256;
    w->s = (char*)calloc(1, w->cap);
    if (!w->s) exit(1);
}

void w_free(W* w) {
    free(w->s);
    w->s = NULL;
    w->n = 0;
}

char* w_take(W* w) {
    char* out = w->s ? w->s : (char*)calloc(1, 1);
    w->s = NULL;
    w->n = 0;
    return out;
}

void w_raw(W* w, const char* text) {
    size_t len = text ? strlen(text) : 0;
    ensure(w, len);
    if (len) memcpy(w->s + w->n, text, len);
    w->n += len;
    w->s[w->n] = 0;
}

static void pre(W* w) {
    if (w->depth > 0 && w->need[w->depth - 1]) w_raw(w, ",");
    if (w->depth > 0) w->need[w->depth - 1] = 1;
}

static void emit_str(W* w, const char* value) {
    const unsigned char* p = (const unsigned char*)(value ? value : "");
    w_raw(w, "\"");
    while (*p) {
        char buf[8];
        unsigned char ch = *p++;
        if (ch == '"' || ch == '\\') {
            buf[0] = '\\';
            buf[1] = (char)ch;
            buf[2] = 0;
            w_raw(w, buf);
        } else if (ch < 0x20) {
            snprintf(buf, sizeof(buf), "\\u%04x", ch);
            w_raw(w, buf);
        } else {
            buf[0] = (char)ch;
            buf[1] = 0;
            w_raw(w, buf);
        }
    }
    w_raw(w, "\"");
}

void w_obj(W* w) {
    pre(w);
    w_raw(w, "{");
    if (w->depth < 48) {
        w->kind[w->depth] = '{';
        w->need[w->depth] = 0;
        w->depth++;
    }
}

void w_arr(W* w) {
    pre(w);
    w_raw(w, "[");
    if (w->depth < 48) {
        w->kind[w->depth] = '[';
        w->need[w->depth] = 0;
        w->depth++;
    }
}

void w_end(W* w) {
    if (w->depth <= 0) return;
    w->depth--;
    w_raw(w, w->kind[w->depth] == '[' ? "]" : "}");
}

void w_key(W* w, const char* key) {
    pre(w);
    emit_str(w, key);
    w_raw(w, ":");
    if (w->depth > 0) w->need[w->depth - 1] = 0;
}

void w_str(W* w, const char* value) {
    pre(w);
    emit_str(w, value);
}

void w_num(W* w, double value) {
    char buf[64];
    pre(w);
    if (isfinite(value) && fabs(value - round(value)) < 1e-9 && fabs(value) < 1e15) snprintf(buf, sizeof(buf), "%.0f", value);
    else snprintf(buf, sizeof(buf), "%.10g", value);
    w_raw(w, buf);
}

void w_bool(W* w, int value) {
    pre(w);
    w_raw(w, value ? "true" : "false");
}

void w_null(W* w) {
    pre(w);
    w_raw(w, "null");
}

int js_self_test(void) {
    const char* err = NULL;
    Js* sample = js_parse("{\"ok\":true,\"n\":2,\"s\":\"a\",\"arr\":[1,2]}", &err);
    int good = sample && js_bool(sample, "ok", 0) && js_num(sample, "n", 0) == 2 && js_len(js_get(sample, "arr")) == 2;
    js_free(sample);
    return good;
}
