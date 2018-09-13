#include "live_api_parse.h"

#include <string.h>
#include <jsmn/jsmn.h>


static bool json_string_eq(const char *buffer,
        const jsmntok_t *token, const char *str)
{
    if(token->type != JSMN_STRING) {
       return false;
    }
    const size_t token_len = (token->end - token->start);
    if(strlen(str) != token_len) {
        return false;
    }
    return (strncmp(buffer+token->start, str, token_len) ==0);
}

static int parse(jsmn_parser *parser, uint8_t *buffer, size_t sizeof_buffer,
        jsmntok_t *tokens, size_t max_tokens)
{
    jsmn_init(parser);
    const int num_tokens = jsmn_parse(parser, (char*)buffer, sizeof_buffer,
            tokens, max_tokens);
    if(num_tokens < 1) {
        // parsing failed
        return num_tokens < 0 ? num_tokens : -10;
    }
    const int num_values = tokens[0].size;
    if((1+2*num_values) != num_tokens) {
        // invalid json: one top-level object expected
        return -20;
    }
    return num_values;
}

static inline const jsmntok_t *json_key(jsmntok_t *tokens, size_t i)
{
    return &tokens[1+2*i];
}
static inline const jsmntok_t *json_value(jsmntok_t *tokens, size_t i)
{
    return &tokens[2+2*i];
}

static inline const char *json_get_str(char *buffer, const jsmntok_t *value)
{
    // zero-terminate the string and return it
    buffer[value->end] = 0;
    return buffer + value->start;
}

bool live_api_parse_register(RegisterMessage *result,
        uint8_t *buffer, size_t sizeof_buffer)
{
    jsmn_parser parser;
    const int max_tokens = 10;
    jsmntok_t tokens[max_tokens];

    const int num_items = parse(&parser, buffer, sizeof_buffer,
            tokens, max_tokens);
    if(num_items < 0) {
        return false;
    }

    for(int i=0;i<num_items;i++) {
        const jsmntok_t *key = json_key(tokens, i);
        const jsmntok_t *value = json_value(tokens, i);
        if(json_string_eq((char*)buffer, key, "username")) {
            result->username = json_get_str((char*)buffer, value);
        }
        if(json_string_eq((char*)buffer, key, "password")) {
            result->password = json_get_str((char*)buffer, value);
        }
        if(json_string_eq((char*)buffer, key, "random")) {
            result->random = json_get_str((char*)buffer, value);
        }
    }
    if(result->username && result->password && result->random) {
        return true;
    }
    return false;
}
