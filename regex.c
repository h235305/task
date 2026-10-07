/*
 * Extract href/title pairs from complete HTML <a>...</a> elements.
 * Linux: gcc -std=c11 -Wall -Wextra -Wpedantic -O2 regex.c -o regex
 * Usage: ./regex [url.txt]       (default input: url.txt)
 * Input and output use UTF-8. Relative URLs are preserved as supplied.
 * POSIX EREs tokenize tags and attributes; a small state machine pairs tags.
 * This avoids a greedy .* swallowing several links on the same line.
 * Supports ordinary HTML fragments, not arbitrary malformed/nested anchors.
 */
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Match complete quoted/unquoted attribute values, including embedded >. */
#define NAME "[^[:space:]\"'<>/=]+"
#define VALUE "(\"[^\"]*\"|'[^']*'|[^[:space:]\"'=<>`]+)"
#define ATTR "([[:space:]]+" NAME "([[:space:]]*=[[:space:]]*" VALUE ")?)"
static const char token_pattern[] =
    "<!--([^-]|-[^-]|--+[^->])*(--+>|-*$)|"
    "<(/?)([[:alpha:]][[:alnum:]_:-]*)" ATTR "*[[:space:]]*/?>";
static const char attribute_pattern[] =
    "[[:space:]]+(" NAME ")([[:space:]]*=[[:space:]]*"
    "(\"([^\"]*)\"|'([^']*)'|([^[:space:]\"'=<>`]+)))?";

typedef struct { char *data; size_t len, cap; } Buffer;

static void die(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void append(Buffer *b, const char *s, size_t n)
{
    size_t need;
    if (n > SIZE_MAX - b->len - 1) die("Input too large.");
    need = b->len + n + 1;
    if (need > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        char *p;
        while (cap < need) {
            if (cap > SIZE_MAX / 2) { cap = need; break; }
            cap *= 2;
        }
        p = realloc(b->data, cap);
        if (!p) die("Out of memory.");
        b->data = p;
        b->cap = cap;
    }
    if (n) memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void clear(Buffer *b)
{
    b->len = 0;
    if (b->data) b->data[0] = '\0';
}

static int equals(const char *s, size_t n, const char *word)
{
    size_t i;
    if (n != strlen(word)) return 0;
    for (i = 0; i < n; ++i)
        if (tolower((unsigned char)s[i]) != (unsigned char)word[i]) return 0;
    return 1;
}

static char *slice(const char *s, regmatch_t m)
{
    Buffer b = {0};
    append(&b, s + m.rm_so, (size_t)(m.rm_eo - m.rm_so));
    return b.data;
}

static void compile(regex_t *re, const char *pattern)
{
    int rc = regcomp(re, pattern, REG_EXTENDED | REG_ICASE);
    if (rc) {
        char message[256];
        regerror(rc, re, message, sizeof message);
        fprintf(stderr, "regcomp: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static int match(regex_t *re, const char *s, size_t n, regmatch_t *m)
{
    int rc = regexec(re, s, n, m, 0);
    if (rc == REG_NOMATCH) return 0;
    if (rc) {
        char message[256];
        regerror(rc, re, message, sizeof message);
        fprintf(stderr, "regexec: %s\n", message);
        exit(EXIT_FAILURE);
    }
    return 1;
}

static char *attribute(regex_t *re, const char *tag, const char *name)
{
    const char *p = tag;
    regmatch_t m[7];
    while (match(re, p, 7, m)) {
        if (equals(p + m[1].rm_so, (size_t)(m[1].rm_eo - m[1].rm_so), name)) {
            int k;
            for (k = 4; k <= 6; ++k)
                if (m[k].rm_so >= 0) return slice(p, m[k]);
            /* A boolean attribute has an empty value; first occurrence wins. */
            { Buffer b = {0}; append(&b, "", 0); return b.data; }
        }
        p += m[0].rm_eo;
    }
    return NULL;
}

static void utf8(Buffer *b, unsigned long cp)
{
    char out[4];
    size_t n;
    if (cp < 0x80) { out[0] = (char)cp; n = 1; }
    else if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 63)); n = 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xe0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 63));
        out[2] = (char)(0x80 | (cp & 63)); n = 3;
    } else {
        out[0] = (char)(0xf0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 63));
        out[2] = (char)(0x80 | ((cp >> 6) & 63));
        out[3] = (char)(0x80 | (cp & 63)); n = 4;
    }
    append(b, out, n);
}

/* Decode common named entities and Unicode numeric references. */
static char *decode(const char *s)
{
    static const struct { const char *name; unsigned long cp; } entities[] = {
        {"amp",38}, {"lt",60}, {"gt",62}, {"quot",34}, {"apos",39},
        {"nbsp",160}, {"hellip",8230}, {"mdash",8212}, {"ndash",8211},
        {"lsquo",8216}, {"rsquo",8217}, {"ldquo",8220}, {"rdquo",8221}
    };
    Buffer b = {0};
    while (*s) {
        const char *end = NULL;
        unsigned long cp = 0;
        int found = 0;
        if (*s == '&') {
            size_t k;
            for (k = 1; k <= 32 && s[k]; ++k)
                if (s[k] == ';') { end = s + k; break; }
        }
        if (end && end - s <= 32) {
            size_t n = (size_t)(end - s - 1), i;
            for (i = 0; i < sizeof entities / sizeof entities[0]; ++i)
                if (strlen(entities[i].name) == n &&
                    memcmp(s + 1, entities[i].name, n) == 0) {
                    cp = entities[i].cp; found = 1; break;
                }
            if (s[1] == '#') {
                const char *digits = s + 2;
                char *stop;
                int base = 10;
                if (*digits == 'x' || *digits == 'X') { ++digits; base = 16; }
                if ((*digits >= '0' && *digits <= '9') ||
                    (base == 16 && ((*digits >= 'a' && *digits <= 'f') ||
                                    (*digits >= 'A' && *digits <= 'F')))) {
                    errno = 0;
                    cp = strtoul(digits, &stop, base);
                    found = !errno && stop == end && cp > 0 && cp <= 0x10ffff &&
                            !(cp >= 0xd800 && cp <= 0xdfff);
                }
            }
        }
        if (found) { utf8(&b, cp); s = end + 1; }
        else { append(&b, s, 1); ++s; }
    }
    append(&b, "", 0);
    return b.data;
}

static char *clean(const char *s, int title)
{
    char *decoded = decode(s);
    Buffer b = {0};
    size_t i = 0, end = strlen(decoded);
    int space = 0;
    while (i < end) {
        unsigned char c = (unsigned char)decoded[i];
        size_t width = (title && c == 0xc2 && i + 1 < end &&
                        (unsigned char)decoded[i + 1] == 0xa0) ? 2 : 1;
        if (isspace(c) || width == 2) {
            if (title) { space = b.len != 0; i += width; continue; }
        }
        if (space) { append(&b, " ", 1); space = 0; }
        append(&b, decoded + i, width);
        i += width;
    }
    free(decoded);
    append(&b, "", 0);
    /* URL whitespace is trimmed, not collapsed in the middle. */
    if (!title) {
        size_t start = 0;
        while (start < b.len && isspace((unsigned char)b.data[start])) ++start;
        while (b.len > start && isspace((unsigned char)b.data[b.len - 1])) --b.len;
        memmove(b.data, b.data + start, b.len - start);
        b.data[b.len - start] = '\0';
    }
    return b.data;
}

static int emit(const char *href, const char *title, const Buffer *body)
{
    char *url, *text;
    int printed;
    if (!href) return 0;
    url = clean(href, 0);
    text = clean(title ? title : "", 1);
    if (!*text) {
        free(text);
        text = clean(body->data ? body->data : "", 1);
    }
    printed = *url && *text;
    if (printed && printf("URL: %s\nTitle: %s\n\n", url, text) < 0)
        die("Failed to write output.");
    free(url);
    free(text);
    return printed;
}

static char *read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    Buffer b = {0};
    char chunk[8192];
    size_t n;
    if (!fp) { fprintf(stderr, "%s: %s\n", path, strerror(errno)); return NULL; }
    while ((n = fread(chunk, 1, sizeof chunk, fp)) != 0) {
        if (memchr(chunk, '\0', n)) {
            fprintf(stderr, "%s: embedded NUL; expected UTF-8 text.\n", path);
            fclose(fp); free(b.data); return NULL;
        }
        append(&b, chunk, n);
    }
    if (ferror(fp)) {
        fprintf(stderr, "%s: read error.\n", path);
        fclose(fp); free(b.data); return NULL;
    }
    if (fclose(fp) != 0) { free(b.data); return NULL; }
    append(&b, "", 0);
    return b.data;
}

int main(int argc, char **argv)
{
    regex_t tokens, attributes;
    regmatch_t m[9];
    char *html, *href = NULL, *title = NULL;
    const char *cursor;
    Buffer body = {0};
    size_t anchors = 0, printed = 0;
    int active = 0;
    const char *raw = NULL;
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [url.txt]\n", argv[0]); return EXIT_FAILURE;
    }
    html = read_file(argc == 2 ? argv[1] : "url.txt");
    if (!html) return EXIT_FAILURE;
    compile(&tokens, token_pattern);
    compile(&attributes, attribute_pattern);
    cursor = html;
    while (match(&tokens, cursor, 9, m)) {
        const char *tag = cursor + m[0].rm_so;
        size_t tag_len = (size_t)(m[0].rm_eo - m[0].rm_so);
        if (active && !raw) append(&body, cursor, (size_t)m[0].rm_so);
        if (tag_len >= 4 && memcmp(tag, "<!--", 4) == 0) {
            cursor += m[0].rm_eo; continue;
        }
        /* Groups 1-2: comment; 3: slash; 4: tag name. */
        {
            const char *name = cursor + m[4].rm_so;
            size_t name_len = (size_t)(m[4].rm_eo - m[4].rm_so);
            int closing = m[3].rm_eo > m[3].rm_so;
            if (raw) {
                if (closing && equals(name, name_len, raw)) raw = NULL;
            } else if (equals(name, name_len, "script") || equals(name, name_len, "style")) {
                if (!closing) raw = equals(name, name_len, "script") ? "script" : "style";
            } else if (equals(name, name_len, "a")) {
                if (closing) {
                    if (active) { ++anchors; printed += (size_t)emit(href, title, &body); }
                    active = 0;
                    free(href); href = NULL; free(title); title = NULL;
                    clear(&body);
                } else {
                    char *opening = slice(cursor, m[0]);
                    /* A new opening anchor abandons an unclosed previous one. */
                    free(href); free(title); clear(&body);
                    href = attribute(&attributes, opening, "href");
                    title = attribute(&attributes, opening, "title");
                    free(opening); active = 1;
                }
            } else if (active && (equals(name, name_len, "br") ||
                                 equals(name, name_len, "p") ||
                                 equals(name, name_len, "div"))) {
                append(&body, " ", 1);
            }
        }
        cursor += m[0].rm_eo;
    }
    /* Only closed anchor elements are emitted. Empty placeholders are skipped. */
    free(href); free(title); free(body.data); free(html);
    regfree(&attributes); regfree(&tokens);
    if (fflush(stdout) == EOF || ferror(stdout)) die("Failed to flush output.");
    fprintf(stderr, "Complete anchors: %zu; printed: %zu; skipped: %zu\n",
            anchors, printed, anchors - printed);
    return EXIT_SUCCESS;
}
