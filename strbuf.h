#ifndef STRBUF_SENTRY
#define STRBUF_SENTRY


typedef struct {
    int len, capacity;
    char *chars;
} strbuf;

void strbuf_init(strbuf *str, int capacity);
void strbuf_free(strbuf *str);
void strbuf_clear(strbuf *str);
void strbuf_append(strbuf *str, char ch);

#endif
