#include "nv_json.h"
#include "nv_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

/* =============================================================================
 * JSON Value Types and Structures
 * =============================================================================
 */

typedef enum {
  JSON_NULL,
  JSON_BOOL,
  JSON_INTEGER,
  JSON_DOUBLE,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT
} json_type_t;

typedef struct json_property {
  char* key;
  struct nv_json_value* value;
} json_property_t;

typedef struct json_array_item {
  struct nv_json_value* value;
} json_array_item_t;

struct nv_json_value {
  json_type_t type;
  nv_arena_t* arena;
  union {
    int bool_val;
    long long int_val;
    double double_val;
    struct {
      char* data;
      size_t len;
    } string_val;
    struct {
      json_array_item_t* items;
      size_t count;
      size_t capacity;
    } array_val;
    struct {
      json_property_t* properties;
      size_t count;
      size_t capacity;
    } object_val;
  } data;
};

/* =============================================================================
 * Object and Array Creation
 * =============================================================================
 */

NV_API nv_json_t* nv_json_object(nv_arena_t* arena) {
  if (!arena) return NULL;
  
  nv_json_t* obj = (nv_json_t*)nv_arena_alloc(arena, sizeof(*obj));
  if (!obj) return NULL;
  
  obj->type = JSON_OBJECT;
  obj->arena = arena;
  obj->data.object_val.properties = NULL;
  obj->data.object_val.count = 0;
  obj->data.object_val.capacity = 0;
  
  return obj;
}

NV_API nv_json_t* nv_json_array(nv_arena_t* arena) {
  if (!arena) return NULL;
  
  nv_json_t* arr = (nv_json_t*)nv_arena_alloc(arena, sizeof(*arr));
  if (!arr) return NULL;
  
  arr->type = JSON_ARRAY;
  arr->arena = arena;
  arr->data.array_val.items = NULL;
  arr->data.array_val.count = 0;
  arr->data.array_val.capacity = 0;
  
  return arr;
}

/* =============================================================================
 * Helper: String Copying
 * =============================================================================
 */

static char* json_strdup(nv_arena_t* arena, const char* str) {
  if (!str) return NULL;
  size_t len = strlen(str);
  char* copy = (char*)nv_arena_alloc(arena, len + 1);
  if (!copy) return NULL;
  memcpy(copy, str, len + 1);
  return copy;
}

/* =============================================================================
 * Helper: Ensure Capacity for Container Growth
 * =============================================================================
 */

static int json_ensure_object_capacity(nv_json_t* obj) {
  if (!obj || obj->type != JSON_OBJECT) return -1;
  
  if (obj->data.object_val.count >= obj->data.object_val.capacity) {
    size_t new_cap = (obj->data.object_val.capacity == 0) ? 8 : (obj->data.object_val.capacity * 2);
    size_t alloc_size = new_cap * sizeof(json_property_t);
    
    json_property_t* new_props = (json_property_t*)nv_arena_alloc(obj->arena, alloc_size);
    if (!new_props) {
      NV_ERR("JSON: object property allocation failed");
      return -1;
    }
    
    if (obj->data.object_val.properties) {
      memcpy(new_props, obj->data.object_val.properties,
             obj->data.object_val.count * sizeof(json_property_t));
    }
    
    obj->data.object_val.properties = new_props;
    obj->data.object_val.capacity = new_cap;
  }
  
  return 0;
}

static int json_ensure_array_capacity(nv_json_t* arr) {
  if (!arr || arr->type != JSON_ARRAY) return -1;
  
  if (arr->data.array_val.count >= arr->data.array_val.capacity) {
    size_t new_cap = (arr->data.array_val.capacity == 0) ? 8 : (arr->data.array_val.capacity * 2);
    size_t alloc_size = new_cap * sizeof(json_array_item_t);
    
    json_array_item_t* new_items = (json_array_item_t*)nv_arena_alloc(arr->arena, alloc_size);
    if (!new_items) {
      NV_ERR("JSON: array item allocation failed");
      return -1;
    }
    
    if (arr->data.array_val.items) {
      memcpy(new_items, arr->data.array_val.items,
             arr->data.array_val.count * sizeof(json_array_item_t));
    }
    
    arr->data.array_val.items = new_items;
    arr->data.array_val.capacity = new_cap;
  }
  
  return 0;
}

/* =============================================================================
 * Object Property Methods
 * =============================================================================
 */

NV_API void nv_json_str(nv_json_t* j, const char* key, const char* val) {
  if (!j || j->type != JSON_OBJECT) return;
  if (!key) return;
  
  if (json_ensure_object_capacity(j) != 0) return;
  
  nv_json_t* str_val = (nv_json_t*)nv_arena_alloc(j->arena, sizeof(*str_val));
  if (!str_val) return;
  
  str_val->type = JSON_STRING;
  str_val->arena = j->arena;
  
  if (val) {
    str_val->data.string_val.len = strlen(val);
    str_val->data.string_val.data = json_strdup(j->arena, val);
    if (!str_val->data.string_val.data) return;
  } else {
    str_val->data.string_val.len = 0;
    str_val->data.string_val.data = NULL;
  }
  
  json_property_t* prop = &j->data.object_val.properties[j->data.object_val.count];
  prop->key = json_strdup(j->arena, key);
  prop->value = str_val;
  
  if (!prop->key) return;
  
  j->data.object_val.count++;
}

NV_API void nv_json_int(nv_json_t* j, const char* key, long long val) {
  if (!j || j->type != JSON_OBJECT) return;
  if (!key) return;
  
  if (json_ensure_object_capacity(j) != 0) return;
  
  nv_json_t* int_val = (nv_json_t*)nv_arena_alloc(j->arena, sizeof(*int_val));
  if (!int_val) return;
  
  int_val->type = JSON_INTEGER;
  int_val->arena = j->arena;
  int_val->data.int_val = val;
  
  json_property_t* prop = &j->data.object_val.properties[j->data.object_val.count];
  prop->key = json_strdup(j->arena, key);
  prop->value = int_val;
  
  if (!prop->key) return;
  
  j->data.object_val.count++;
}

NV_API void nv_json_double(nv_json_t* j, const char* key, double val) {
  if (!j || j->type != JSON_OBJECT) return;
  if (!key) return;
  
  if (json_ensure_object_capacity(j) != 0) return;
  
  nv_json_t* dbl_val = (nv_json_t*)nv_arena_alloc(j->arena, sizeof(*dbl_val));
  if (!dbl_val) return;
  
  dbl_val->type = JSON_DOUBLE;
  dbl_val->arena = j->arena;
  dbl_val->data.double_val = val;
  
  json_property_t* prop = &j->data.object_val.properties[j->data.object_val.count];
  prop->key = json_strdup(j->arena, key);
  prop->value = dbl_val;
  
  if (!prop->key) return;
  
  j->data.object_val.count++;
}

NV_API void nv_json_bool(nv_json_t* j, const char* key, int val) {
  if (!j || j->type != JSON_OBJECT) return;
  if (!key) return;
  
  if (json_ensure_object_capacity(j) != 0) return;
  
  nv_json_t* bool_val = (nv_json_t*)nv_arena_alloc(j->arena, sizeof(*bool_val));
  if (!bool_val) return;
  
  bool_val->type = JSON_BOOL;
  bool_val->arena = j->arena;
  bool_val->data.bool_val = (val ? 1 : 0);
  
  json_property_t* prop = &j->data.object_val.properties[j->data.object_val.count];
  prop->key = json_strdup(j->arena, key);
  prop->value = bool_val;
  
  if (!prop->key) return;
  
  j->data.object_val.count++;
}

NV_API void nv_json_null(nv_json_t* j, const char* key) {
  if (!j || j->type != JSON_OBJECT) return;
  if (!key) return;
  
  if (json_ensure_object_capacity(j) != 0) return;
  
  nv_json_t* null_val = (nv_json_t*)nv_arena_alloc(j->arena, sizeof(*null_val));
  if (!null_val) return;
  
  null_val->type = JSON_NULL;
  null_val->arena = j->arena;
  
  json_property_t* prop = &j->data.object_val.properties[j->data.object_val.count];
  prop->key = json_strdup(j->arena, key);
  prop->value = null_val;
  
  if (!prop->key) return;
  
  j->data.object_val.count++;
}

NV_API void nv_json_nest(nv_json_t* parent, const char* key, nv_json_t* child) {
  if (!parent || parent->type != JSON_OBJECT) return;
  if (!key || !child) return;
  
  if (json_ensure_object_capacity(parent) != 0) return;
  
  json_property_t* prop = &parent->data.object_val.properties[parent->data.object_val.count];
  prop->key = json_strdup(parent->arena, key);
  prop->value = child;
  
  if (!prop->key) return;
  
  parent->data.object_val.count++;
}

/* =============================================================================
 * Array Element Methods
 * =============================================================================
 */

NV_API void nv_json_array_push(nv_json_t* arr, nv_json_t* item) {
  if (!arr || arr->type != JSON_ARRAY || !item) return;
  
  if (json_ensure_array_capacity(arr) != 0) return;
  
  json_array_item_t* slot = &arr->data.array_val.items[arr->data.array_val.count];
  slot->value = item;
  arr->data.array_val.count++;
}

/* =============================================================================
 * Serialization Helpers
 * =============================================================================
 */

typedef struct {
  char* buf;
  size_t pos;
  size_t capacity;
  nv_arena_t* arena;
} json_builder_t;

/* Append char to buffer, growing as needed */
NV_INTERNAL int json_builder_append_char(json_builder_t* b, char c) {
  if (b->pos >= b->capacity) {
    size_t new_cap = (b->capacity == 0) ? 256 : (b->capacity * 2);
    char* new_buf = (char*)nv_arena_alloc(b->arena, new_cap);
    if (!new_buf) return -1;
    
    if (b->buf) {
      memcpy(new_buf, b->buf, b->pos);
    }
    
    b->buf = new_buf;
    b->capacity = new_cap;
  }
  
  b->buf[b->pos++] = c;
  return 0;
}

/* Append string to buffer */
NV_INTERNAL int json_builder_append_str(json_builder_t* b, const char* str) {
  if (!str) return 0;
  for (const char* p = str; *p; p++) {
    if (json_builder_append_char(b, *p) != 0) return -1;
  }
  return 0;
}

/* Escape and append string value */
NV_INTERNAL int json_builder_append_escaped(json_builder_t* b, const char* str) {
  if (!str) return json_builder_append_str(b, "null");
  
  if (json_builder_append_char(b, '"') != 0) return -1;
  
  for (const char* p = str; *p; p++) {
    unsigned char c = (unsigned char)*p;
    
    switch (c) {
      case '"': 
        if (json_builder_append_str(b, "\\\"") != 0) return -1;
        break;
      case '\\':
        if (json_builder_append_str(b, "\\\\") != 0) return -1;
        break;
      case '\n':
        if (json_builder_append_str(b, "\\n") != 0) return -1;
        break;
      case '\r':
        if (json_builder_append_str(b, "\\r") != 0) return -1;
        break;
      case '\t':
        if (json_builder_append_str(b, "\\t") != 0) return -1;
        break;
      case '\b':
        if (json_builder_append_str(b, "\\b") != 0) return -1;
        break;
      case '\f':
        if (json_builder_append_str(b, "\\f") != 0) return -1;
        break;
      default:
        if (c < 0x20) {
          /* Control character - escape as \uXXXX */
          char hex[8];
          snprintf(hex, sizeof(hex), "\\u%04x", c);
          if (json_builder_append_str(b, hex) != 0) return -1;
        } else {
          if (json_builder_append_char(b, c) != 0) return -1;
        }
    }
  }
  
  return json_builder_append_char(b, '"');
}

/* Forward declaration for recursive serialization */
NV_INTERNAL int json_builder_serialize_value(json_builder_t* b, const nv_json_t* val);

NV_INTERNAL int json_builder_serialize_object(json_builder_t* b, const nv_json_t* obj) {
  if (json_builder_append_char(b, '{') != 0) return -1;
  
  for (size_t i = 0; i < obj->data.object_val.count; i++) {
    const json_property_t* prop = &obj->data.object_val.properties[i];
    
    /* Key */
    if (json_builder_append_char(b, '"') != 0) return -1;
    if (json_builder_append_str(b, prop->key) != 0) return -1;
    if (json_builder_append_char(b, '"') != 0) return -1;
    if (json_builder_append_char(b, ':') != 0) return -1;
    
    /* Value */
    if (json_builder_serialize_value(b, prop->value) != 0) return -1;
    
    /* Comma (except after last) */
    if (i < obj->data.object_val.count - 1) {
      if (json_builder_append_char(b, ',') != 0) return -1;
    }
  }
  
  return json_builder_append_char(b, '}');
}

NV_INTERNAL int json_builder_serialize_array(json_builder_t* b, const nv_json_t* arr) {
  if (json_builder_append_char(b, '[') != 0) return -1;
  
  for (size_t i = 0; i < arr->data.array_val.count; i++) {
    const json_array_item_t* item = &arr->data.array_val.items[i];
    
    if (json_builder_serialize_value(b, item->value) != 0) return -1;
    
    /* Comma (except after last) */
    if (i < arr->data.array_val.count - 1) {
      if (json_builder_append_char(b, ',') != 0) return -1;
    }
  }
  
  return json_builder_append_char(b, ']');
}

NV_INTERNAL int json_builder_serialize_value(json_builder_t* b, const nv_json_t* val) {
  if (!val) return json_builder_append_str(b, "null");
  
  switch (val->type) {
    case JSON_NULL:
      return json_builder_append_str(b, "null");
    
    case JSON_BOOL:
      return json_builder_append_str(b, val->data.bool_val ? "true" : "false");
    
    case JSON_INTEGER: {
      char buf[32];
      snprintf(buf, sizeof(buf), "%lld", val->data.int_val);
      return json_builder_append_str(b, buf);
    }
    
    case JSON_DOUBLE: {
      double num = val->data.double_val;
      if (isnan(num) || isinf(num)) {
        return json_builder_append_str(b, "null");
      }
      
      char buf[32];
      /* Check if it's an integer value */
      if (num == (double)(long long)num) {
        snprintf(buf, sizeof(buf), "%.0f", num);
      } else {
        /* Use %.17g for full precision, removing trailing zeros */
        snprintf(buf, sizeof(buf), "%.17g", num);
      }
      return json_builder_append_str(b, buf);
    }
    
    case JSON_STRING:
      if (val->data.string_val.data) {
        return json_builder_append_escaped(b, val->data.string_val.data);
      } else {
        return json_builder_append_str(b, "null");
      }
    
    case JSON_ARRAY:
      return json_builder_serialize_array(b, val);
    
    case JSON_OBJECT:
      return json_builder_serialize_object(b, val);
  }
  
  return -1;
}

/* =============================================================================
 * Serialization
 * =============================================================================
 */

NV_API const char* nv_json_serialize(nv_json_t* j) {
  if (!j) return "null";
  if (!j->arena) return "null";
  
  json_builder_t builder = {
    .buf = NULL,
    .pos = 0,
    .capacity = 0,
    .arena = j->arena
  };
  
  if (json_builder_serialize_value(&builder, j) != 0) {
    NV_ERR("JSON: serialization failed");
    return "null";
  }
  
  /* Null-terminate */
  if (json_builder_append_char(&builder, '\0') != 0) {
    NV_ERR("JSON: failed to null-terminate");
    return "null";
  }
  
  return builder.buf ? builder.buf : "null";
}

/* =============================================================================
 * Parser Implementation
 * =============================================================================
 */

typedef enum {
  JSON_TOKEN_EOF,
  JSON_TOKEN_LBRACE,      /* { */
  JSON_TOKEN_RBRACE,      /* } */
  JSON_TOKEN_LBRACKET,    /* [ */
  JSON_TOKEN_RBRACKET,    /* ] */
  JSON_TOKEN_COLON,       /* : */
  JSON_TOKEN_COMMA,       /* , */
  JSON_TOKEN_STRING,
  JSON_TOKEN_NUMBER,
  JSON_TOKEN_TRUE,
  JSON_TOKEN_FALSE,
  JSON_TOKEN_NULL,
  JSON_TOKEN_ERROR
} json_token_type_t;

typedef struct {
  json_token_type_t type;
  const char* value;
  size_t length;
  size_t pos;  /* Position in input for error reporting */
} json_token_t;

typedef struct {
  const char* input;
  size_t pos;
  size_t len;
} json_lexer_t;

/* Parsed JSON value structure */
struct nv_json_parsed {
  json_type_t type;
  nv_arena_t* arena;
  union {
    int bool_val;
    long long int_val;
    double double_val;
    struct {
      char* data;
      size_t len;
    } string_val;
    struct {
      struct nv_json_parsed** items;
      size_t count;
    } array_val;
    struct {
      char** keys;
      struct nv_json_parsed** values;
      size_t count;
    } object_val;
  } data;
};

/**
 * Lexer - tokenize JSON input
 */
static void json_lexer_init(json_lexer_t* lex, const char* input) {
  lex->input = input;
  lex->pos = 0;
  lex->len = strlen(input);
}

static void json_skip_whitespace(json_lexer_t* lex) {
  while (lex->pos < lex->len && isspace((unsigned char)lex->input[lex->pos])) {
    lex->pos++;
  }
}

static json_token_t json_next_token(json_lexer_t* lex) {
  json_skip_whitespace(lex);
  
  json_token_t tok = {JSON_TOKEN_ERROR, NULL, 0, lex->pos};
  
  if (lex->pos >= lex->len) {
    tok.type = JSON_TOKEN_EOF;
    return tok;
  }
  
  char ch = lex->input[lex->pos];
  
  switch (ch) {
    case '{': tok.type = JSON_TOKEN_LBRACE; lex->pos++; return tok;
    case '}': tok.type = JSON_TOKEN_RBRACE; lex->pos++; return tok;
    case '[': tok.type = JSON_TOKEN_LBRACKET; lex->pos++; return tok;
    case ']': tok.type = JSON_TOKEN_RBRACKET; lex->pos++; return tok;
    case ':': tok.type = JSON_TOKEN_COLON; lex->pos++; return tok;
    case ',': tok.type = JSON_TOKEN_COMMA; lex->pos++; return tok;
    
    case '"': {
      /* String literal */
      size_t start = lex->pos + 1;
      lex->pos++;
      while (lex->pos < lex->len && lex->input[lex->pos] != '"') {
        if (lex->input[lex->pos] == '\\') {
          lex->pos++; /* Skip escape char */
        }
        lex->pos++;
      }
      if (lex->pos >= lex->len) {
        NV_ERR("JSON parse: unterminated string at byte %zu", tok.pos);
        return tok; /* ERROR */
      }
      tok.type = JSON_TOKEN_STRING;
      tok.value = &lex->input[start];
      tok.length = lex->pos - start;
      lex->pos++; /* Skip closing quote */
      return tok;
    }
    
    case 't':
      if (lex->pos + 4 <= lex->len && memcmp(&lex->input[lex->pos], "true", 4) == 0) {
        tok.type = JSON_TOKEN_TRUE;
        lex->pos += 4;
        return tok;
      }
      break;
    
    case 'f':
      if (lex->pos + 5 <= lex->len && memcmp(&lex->input[lex->pos], "false", 5) == 0) {
        tok.type = JSON_TOKEN_FALSE;
        lex->pos += 5;
        return tok;
      }
      break;
    
    case 'n':
      if (lex->pos + 4 <= lex->len && memcmp(&lex->input[lex->pos], "null", 4) == 0) {
        tok.type = JSON_TOKEN_NULL;
        lex->pos += 4;
        return tok;
      }
      break;
    
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
      /* Number literal */
      size_t start_pos = lex->pos;
      if (ch == '-') lex->pos++;
      
      /* Integer part */
      if (lex->pos >= lex->len || !isdigit((unsigned char)lex->input[lex->pos])) {
        return tok; /* ERROR */
      }
      
      if (lex->input[lex->pos] == '0') {
        lex->pos++;
      } else {
        while (lex->pos < lex->len && isdigit((unsigned char)lex->input[lex->pos])) {
          lex->pos++;
        }
      }
      
      /* Fractional part */
      if (lex->pos < lex->len && lex->input[lex->pos] == '.') {
        lex->pos++;
        if (lex->pos >= lex->len || !isdigit((unsigned char)lex->input[lex->pos])) {
          return tok; /* ERROR */
        }
        while (lex->pos < lex->len && isdigit((unsigned char)lex->input[lex->pos])) {
          lex->pos++;
        }
      }
      
      /* Exponent part */
      if (lex->pos < lex->len && (lex->input[lex->pos] == 'e' || lex->input[lex->pos] == 'E')) {
        lex->pos++;
        if (lex->pos < lex->len && (lex->input[lex->pos] == '+' || lex->input[lex->pos] == '-')) {
          lex->pos++;
        }
        if (lex->pos >= lex->len || !isdigit((unsigned char)lex->input[lex->pos])) {
          return tok; /* ERROR */
        }
        while (lex->pos < lex->len && isdigit((unsigned char)lex->input[lex->pos])) {
          lex->pos++;
        }
      }
      
      tok.type = JSON_TOKEN_NUMBER;
      tok.value = &lex->input[start_pos];
      tok.length = lex->pos - start_pos;
      return tok;
    }
  }
  
  NV_ERR("JSON parse: unexpected character '%c' at byte %zu", ch, tok.pos);
  return tok; /* ERROR */
}

/**
 * Unescape JSON string (in-place in arena buffer)
 */
static char* json_unescape_string(nv_arena_t* arena, const char* src, size_t src_len) {
  /* Allocate output buffer */
  char* out = (char*)nv_arena_alloc(arena, src_len + 1);
  if (!out) return NULL;
  
  size_t out_pos = 0;
  for (size_t i = 0; i < src_len; i++) {
    if (src[i] == '\\' && i + 1 < src_len) {
      i++;
      switch (src[i]) {
        case '"': out[out_pos++] = '"'; break;
        case '\\': out[out_pos++] = '\\'; break;
        case '/': out[out_pos++] = '/'; break;
        case 'b': out[out_pos++] = '\b'; break;
        case 'f': out[out_pos++] = '\f'; break;
        case 'n': out[out_pos++] = '\n'; break;
        case 'r': out[out_pos++] = '\r'; break;
        case 't': out[out_pos++] = '\t'; break;
        case 'u': {
          /* Unicode escape \uXXXX - just copy as-is for now */
          if (i + 4 < src_len) {
            out[out_pos++] = '\\';
            out[out_pos++] = 'u';
            out[out_pos++] = src[i+1];
            out[out_pos++] = src[i+2];
            out[out_pos++] = src[i+3];
            out[out_pos++] = src[i+4];
            i += 4;
          }
          break;
        }
        default:
          out[out_pos++] = src[i];
      }
    } else {
      out[out_pos++] = src[i];
    }
  }
  out[out_pos] = '\0';
  
  return out;
}

/* Forward declaration for recursive parsing */
static nv_json_val_t* json_parse_value(nv_arena_t* arena, json_lexer_t* lex);

static nv_json_val_t* json_parse_object(nv_arena_t* arena, json_lexer_t* lex) {
  nv_json_val_t* obj = (nv_json_val_t*)nv_arena_alloc(arena, sizeof(*obj));
  if (!obj) return NULL;
  
  obj->type = JSON_OBJECT;
  obj->arena = arena;
  obj->data.object_val.keys = NULL;
  obj->data.object_val.values = NULL;
  obj->data.object_val.count = 0;
  
  size_t capacity = 10;
  obj->data.object_val.keys = (char**)nv_arena_alloc(arena, capacity * sizeof(char*));
  obj->data.object_val.values = (nv_json_val_t**)nv_arena_alloc(arena, capacity * sizeof(nv_json_val_t*));
  if (!obj->data.object_val.keys || !obj->data.object_val.values) {
    return NULL;
  }
  
  json_skip_whitespace(lex);
  if (lex->pos < lex->len && lex->input[lex->pos] == '}') {
    lex->pos++;
    return obj; /* Empty object */
  }
  
  while (1) {
    json_token_t key_tok = json_next_token(lex);
    if (key_tok.type != JSON_TOKEN_STRING) {
      NV_ERR("JSON parse: expected string key at byte %zu", key_tok.pos);
      return NULL;
    }
    
    char* key = json_unescape_string(arena, key_tok.value, key_tok.length);
    if (!key) return NULL;
    
    json_token_t colon_tok = json_next_token(lex);
    if (colon_tok.type != JSON_TOKEN_COLON) {
      NV_ERR("JSON parse: expected ':' at byte %zu", colon_tok.pos);
      return NULL;
    }
    
    nv_json_val_t* val = json_parse_value(arena, lex);
    if (!val) return NULL;
    
    if (obj->data.object_val.count >= capacity) {
      capacity *= 2;
      char** new_keys = (char**)nv_arena_alloc(arena, capacity * sizeof(char*));
      nv_json_val_t** new_vals = (nv_json_val_t**)nv_arena_alloc(arena, capacity * sizeof(nv_json_val_t*));
      if (!new_keys || !new_vals) return NULL;
      
      memcpy(new_keys, obj->data.object_val.keys, obj->data.object_val.count * sizeof(char*));
      memcpy(new_vals, obj->data.object_val.values, obj->data.object_val.count * sizeof(nv_json_val_t*));
      
      obj->data.object_val.keys = new_keys;
      obj->data.object_val.values = new_vals;
    }
    
    obj->data.object_val.keys[obj->data.object_val.count] = key;
    obj->data.object_val.values[obj->data.object_val.count] = val;
    obj->data.object_val.count++;
    
    json_token_t next = json_next_token(lex);
    if (next.type == JSON_TOKEN_RBRACE) {
      return obj;
    } else if (next.type == JSON_TOKEN_COMMA) {
      continue;
    } else {
      NV_ERR("JSON parse: expected ',' or '}' at byte %zu", next.pos);
      return NULL;
    }
  }
}

static nv_json_val_t* json_parse_array(nv_arena_t* arena, json_lexer_t* lex) {
  nv_json_val_t* arr = (nv_json_val_t*)nv_arena_alloc(arena, sizeof(*arr));
  if (!arr) return NULL;
  
  arr->type = JSON_ARRAY;
  arr->arena = arena;
  arr->data.array_val.items = NULL;
  arr->data.array_val.count = 0;
  
  size_t capacity = 10;
  arr->data.array_val.items = (nv_json_val_t**)nv_arena_alloc(arena, capacity * sizeof(nv_json_val_t*));
  if (!arr->data.array_val.items) return NULL;
  
  json_skip_whitespace(lex);
  if (lex->pos < lex->len && lex->input[lex->pos] == ']') {
    lex->pos++;
    return arr; /* Empty array */
  }
  
  while (1) {
    nv_json_val_t* val = json_parse_value(arena, lex);
    if (!val) return NULL;
    
    if (arr->data.array_val.count >= capacity) {
      capacity *= 2;
      nv_json_val_t** new_items = (nv_json_val_t**)nv_arena_alloc(arena, capacity * sizeof(nv_json_val_t*));
      if (!new_items) return NULL;
      
      memcpy(new_items, arr->data.array_val.items, arr->data.array_val.count * sizeof(nv_json_val_t*));
      arr->data.array_val.items = new_items;
    }
    
    arr->data.array_val.items[arr->data.array_val.count++] = val;
    
    json_token_t next = json_next_token(lex);
    if (next.type == JSON_TOKEN_RBRACKET) {
      return arr;
    } else if (next.type == JSON_TOKEN_COMMA) {
      continue;
    } else {
      NV_ERR("JSON parse: expected ',' or ']' at byte %zu", next.pos);
      return NULL;
    }
  }
}

static nv_json_val_t* json_parse_value(nv_arena_t* arena, json_lexer_t* lex) {
  json_token_t tok = json_next_token(lex);
  
  nv_json_val_t* val = (nv_json_val_t*)nv_arena_alloc(arena, sizeof(*val));
  if (!val) return NULL;
  
  val->arena = arena;
  
  switch (tok.type) {
    case JSON_TOKEN_LBRACE:
      return json_parse_object(arena, lex);
    
    case JSON_TOKEN_LBRACKET:
      return json_parse_array(arena, lex);
    
    case JSON_TOKEN_STRING:
      val->type = JSON_STRING;
      val->data.string_val.data = json_unescape_string(arena, tok.value, tok.length);
      val->data.string_val.len = strlen(val->data.string_val.data);
      break;
    
    case JSON_TOKEN_NUMBER:
      val->type = JSON_DOUBLE;
      /* Try parsing as integer first */
      {
        char* end;
        long long i = strtoll(tok.value, &end, 10);
        if (end == tok.value + tok.length) {
          val->type = JSON_INTEGER;
          val->data.int_val = i;
        } else {
          val->data.double_val = strtod(tok.value, NULL);
        }
      }
      break;
    
    case JSON_TOKEN_TRUE:
      val->type = JSON_BOOL;
      val->data.bool_val = 1;
      break;
    
    case JSON_TOKEN_FALSE:
      val->type = JSON_BOOL;
      val->data.bool_val = 0;
      break;
    
    case JSON_TOKEN_NULL:
      val->type = JSON_NULL;
      break;
      
    default:
      NV_ERR("JSON parse: unexpected token type %d", tok.type);
      return NULL;
  }
  
  return val;
}

NV_API nv_json_val_t* nv_json_parse(nv_arena_t* arena, const char* input) {
  if (!arena || !input) return NULL;
  
  json_lexer_t lex;
  json_lexer_init(&lex, input);
  
  nv_json_val_t* res = json_parse_value(arena, &lex);
  
  if (res) {
    /* Check for trailing content */
    json_skip_whitespace(&lex);
    if (lex.pos < lex.len) {
      NV_ERR("JSON parse: unexpected trailing content at byte %zu", lex.pos);
      return NULL;
    }
  }
  
  return res;
}

/* =============================================================================
 * Type Checking
 * =============================================================================
 */

NV_API int nv_json_is_object(const nv_json_val_t* v) { return v && v->type == JSON_OBJECT; }
NV_API int nv_json_is_array(const nv_json_val_t* v) { return v && v->type == JSON_ARRAY; }
NV_API int nv_json_is_string(const nv_json_val_t* v) { return v && v->type == JSON_STRING; }
NV_API int nv_json_is_number(const nv_json_val_t* v) { return v && (v->type == JSON_INTEGER || v->type == JSON_DOUBLE); }

/* =============================================================================
 * Object Accessors
 * =============================================================================
 */

NV_API const char* nv_json_get_str(const nv_json_val_t* obj, const char* key) {
  if (!nv_json_is_object(obj) || !key) return NULL;
  
  for (size_t i = 0; i < obj->data.object_val.count; i++) {
    if (strcmp(obj->data.object_val.keys[i], key) == 0) {
      nv_json_val_t* val = obj->data.object_val.values[i];
      if (val && val->type == JSON_STRING) {
        return val->data.string_val.data;
      }
      return NULL;
    }
  }
  return NULL;
}

NV_API long long nv_json_get_int(const nv_json_val_t* obj, const char* key) {
  if (!nv_json_is_object(obj) || !key) return 0;
  
  for (size_t i = 0; i < obj->data.object_val.count; i++) {
    if (strcmp(obj->data.object_val.keys[i], key) == 0) {
      nv_json_val_t* val = obj->data.object_val.values[i];
      if (val && val->type == JSON_INTEGER) return val->data.int_val;
      if (val && val->type == JSON_DOUBLE) return (long long)val->data.double_val;
      return 0;
    }
  }
  return 0;
}

NV_API double nv_json_get_double(const nv_json_val_t* obj, const char* key) {
  if (!nv_json_is_object(obj) || !key) return 0.0;
  
  for (size_t i = 0; i < obj->data.object_val.count; i++) {
    if (strcmp(obj->data.object_val.keys[i], key) == 0) {
      nv_json_val_t* val = obj->data.object_val.values[i];
      if (val && val->type == JSON_DOUBLE) return val->data.double_val;
      if (val && val->type == JSON_INTEGER) return (double)val->data.int_val;
      return 0.0;
    }
  }
  return 0.0;
}

NV_API int nv_json_get_bool(const nv_json_val_t* obj, const char* key) {
  if (!nv_json_is_object(obj) || !key) return 0;
  
  for (size_t i = 0; i < obj->data.object_val.count; i++) {
    if (strcmp(obj->data.object_val.keys[i], key) == 0) {
      nv_json_val_t* val = obj->data.object_val.values[i];
      if (val && val->type == JSON_BOOL) return val->data.bool_val;
      return 0;
    }
  }
  return 0;
}

NV_API nv_json_val_t* nv_json_get_obj(const nv_json_val_t* obj, const char* key) {
  if (!nv_json_is_object(obj) || !key) return NULL;
  
  for (size_t i = 0; i < obj->data.object_val.count; i++) {
    if (strcmp(obj->data.object_val.keys[i], key) == 0) {
      return obj->data.object_val.values[i];
    }
  }
  return NULL;
}

/* =============================================================================
 * Array Accessors
 * =============================================================================
 */

NV_API size_t nv_json_array_len(const nv_json_val_t* arr) {
  if (!nv_json_is_array(arr)) return 0;
  return arr->data.array_val.count;
}

NV_API nv_json_val_t* nv_json_array_get(const nv_json_val_t* arr, size_t index) {
  if (!nv_json_is_array(arr)) return NULL;
  if (index >= arr->data.array_val.count) return NULL;
  return arr->data.array_val.items[index];
}

/* =============================================================================
 * Serialization for Parsed Values (Forwarding)
 * =============================================================================
 */

static int json_builder_serialize_parsed_value(json_builder_t* b, const nv_json_val_t* val);

static int json_builder_serialize_parsed_object(json_builder_t* b, const nv_json_val_t* obj) {
  if (json_builder_append_char(b, '{') != 0) return -1;
  
  for (size_t i = 0; i < obj->data.object_val.count; i++) {
    if (json_builder_append_char(b, '"') != 0) return -1;
    if (json_builder_append_str(b, obj->data.object_val.keys[i]) != 0) return -1;
    if (json_builder_append_char(b, '"') != 0) return -1;
    if (json_builder_append_char(b, ':') != 0) return -1;
    
    if (json_builder_serialize_parsed_value(b, obj->data.object_val.values[i]) != 0) return -1;
    
    if (i < obj->data.object_val.count - 1) {
      if (json_builder_append_char(b, ',') != 0) return -1;
    }
  }
  return json_builder_append_char(b, '}');
}

static int json_builder_serialize_parsed_array(json_builder_t* b, const nv_json_val_t* arr) {
  if (json_builder_append_char(b, '[') != 0) return -1;
  
  for (size_t i = 0; i < arr->data.array_val.count; i++) {
    if (json_builder_serialize_parsed_value(b, arr->data.array_val.items[i]) != 0) return -1;
    
    if (i < arr->data.array_val.count - 1) {
      if (json_builder_append_char(b, ',') != 0) return -1;
    }
  }
  return json_builder_append_char(b, ']');
}

static int json_builder_serialize_parsed_value(json_builder_t* b, const nv_json_val_t* val) {
  if (!val) return json_builder_append_str(b, "null");
  
  switch (val->type) {
    case JSON_NULL: return json_builder_append_str(b, "null");
    case JSON_BOOL: return json_builder_append_str(b, val->data.bool_val ? "true" : "false");
    case JSON_INTEGER: {
      char buf[32];
      snprintf(buf, sizeof(buf), "%lld", val->data.int_val);
      return json_builder_append_str(b, buf);
    }
    case JSON_DOUBLE: {
      double num = val->data.double_val;
       if (isnan(num) || isinf(num)) return json_builder_append_str(b, "null");
       char buf[32];
       if (num == (double)(long long)num) snprintf(buf, sizeof(buf), "%.0f", num);
       else snprintf(buf, sizeof(buf), "%.17g", num);
       return json_builder_append_str(b, buf);
    }
    case JSON_STRING:
      return json_builder_append_escaped(b, val->data.string_val.data);
    case JSON_ARRAY: return json_builder_serialize_parsed_array(b, val);
    case JSON_OBJECT: return json_builder_serialize_parsed_object(b, val);
    default: return -1;
  }
}

NV_API const char* nv_json_val_serialize(nv_arena_t* arena, const nv_json_val_t* val) {
  if (!val || !arena) return "null";
  json_builder_t builder = { .buf = NULL, .pos = 0, .capacity = 0, .arena = arena };
  if (json_builder_serialize_parsed_value(&builder, val) != 0) return "null";
  if (json_builder_append_char(&builder, '\0') != 0) return "null";
  return builder.buf ? builder.buf : "null";
}
