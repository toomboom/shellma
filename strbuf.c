#include "strbuf.h"
#include <stdlib.h>


void strbuf_init(strbuf *str, int capacity)
{
    str->len = 0;
    str->capacity = capacity;
    str->chars = malloc(capacity);
}

void strbuf_clear(strbuf *str)
{
    str->len = 0; 
    str->chars[0] = '\0';
}

void strbuf_free(strbuf *str)
{
    str->len = str->capacity = 0;
    free(str->chars);
}

void strbuf_append(strbuf *str, char ch)
{
    if (str->len >= str->capacity-1) {
        str->capacity *= 2;
        str->chars = realloc(str->chars, str->capacity);
    }
    str->chars[str->len] = ch;
    str->chars[str->len+1] = '\0';
    str->len++;
}
