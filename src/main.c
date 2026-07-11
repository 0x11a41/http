#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "cutils/itypes.h"
#include "lib/slice.h"
#include "cutils/da.h"
#include "cutils/utils.h"

#define PORT 8080
#define VERSION "HTTP/1.1"

#define STATUS_LINE_MAX_LEN 64
#define HEADER_FIELD_MAX_LEN 256


static const char *allowed_methods[] = {
    "GET", "POST",
};


static bool is_valid_method(Slice *method)
{
    u8 n = sizeof(allowed_methods) / sizeof(char*);
    for (u8 i = 0; i < n; i++) {
        if (slice_cmp_str(method, allowed_methods[i])) return true;
    }
    return false;
}

typedef enum {
    STATUS_OK                    = 200,
    STATUS_CONTINUE              = 100,
    STATUS_MOVED_PERMANENTLY     = 301,
    STATUS_NOT_MODIFIED          = 304,
    STATUS_BAD_REQUEST           = 400,
    STATUS_NOT_FOUND             = 404,
    STATUS_METHOD_NOT_ALLOWED    = 405,
    STATUS_INTERNAL_SERVER_ERROR = 500,
    STATUS_NOT_IMPLEMENTED       = 501,
    STATUS_VERSION_NOT_SUPPORTED = 505,
} HTTPStatusCode;

typedef enum {
    TARGET_MALFORMED,
    TARGET_ORIGIN_FORM,
    TARGET_ABSOLUTE_FORM,
    TARGET_AUTHORITY_FORM,
    TARGET_ASTERISK_FORM,
} HTTPTargetForm;

static inline void debug_target_form(HTTPTargetForm form)
{
    if (DEBUG == 0) return;
    switch (form) {
        case TARGET_ABSOLUTE_FORM : printf("Absolute form") ; break;
        case TARGET_ASTERISK_FORM : printf("Asterisk form") ; break;
        case TARGET_AUTHORITY_FORM: printf("Authority form"); break;
        case TARGET_ORIGIN_FORM   : printf("Origin form")   ; break;
        case TARGET_MALFORMED     : printf("Malformed")     ; break;
    }
}

static inline const char* http_reason_phrase(HTTPStatusCode code)
{
    switch (code) {
        case STATUS_OK                   : return "Ok";
        case STATUS_CONTINUE             : return "Continue";
        case STATUS_MOVED_PERMANENTLY    : return "Moved Permanently";
        case STATUS_NOT_MODIFIED         : return "Not Modified";
        case STATUS_BAD_REQUEST          : return "Bad Request";
        case STATUS_NOT_FOUND            : return "Not Found";
        case STATUS_METHOD_NOT_ALLOWED   : return "Method Not Allowed";
        case STATUS_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case STATUS_NOT_IMPLEMENTED      : return "Not Implemented";
        case STATUS_VERSION_NOT_SUPPORTED: return "Version Not Supported";
        default                          : return "Unknown";
    }
}

typedef struct {
    char* buf;
    usize curr;
    usize len;
} Lexer;

typedef struct {
    Slice field;
    Slice value;
} HTTPHeaderFieldLine;

typedef struct {
    HTTPHeaderFieldLine *content;
    usize len;
    usize capacity;
} HTTPRequestHeader;

typedef struct {
    Slice method;
    struct {
        HTTPTargetForm form;
        Slice raw;
    } target;
    Slice version;
    HTTPRequestHeader header;
    Slice body;
} HTTPRequest;

static void debug_http_request(HTTPRequest *req)
{
    if (DEBUG == 0) return;

    debug("\n");
    debug("[ ");
        slice_debug(&req->method);
    debug(" ]::");
    debug("[ ");
        slice_debug(&req->target.raw);
        debug(" => "); debug_target_form(req->target.form);
    debug(" ]::");
    debug("[ ");
        slice_debug(&req->version);
    debug(" ]\n");

    for (u32 i = 0; i < req->header.len; i++) {
        debug("[ ");
            slice_debug(&req->header.content[i].field);
        debug(" ]::");
        debug("[ ");
            slice_debug(&req->header.content[i].value);
        debug(" ]\n");
    }

    debug("[[ ");
        slice_debug(&req->body);
    debug(" ]]\n");
}

typedef struct {
    HTTPStatusCode status;
    char *type;
    char *body;
    usize len;
} HTTPResponse;


static bool token(char ch)
{
    if (isalnum(ch)) return true;
    switch (ch) {
        case '!': case '#': case '$': case '%': case '&': 
        case '\'': case '*': case '+': case '-': case '.': 
        case '^': case '_': case '`': case '|': case '~':
            return true;
    }
    return false;
}

static inline bool CR(char ch) { return ch == '\r'; }
static inline bool LF(char ch) { return ch == '\n'; }
static inline bool SP(char ch) { return ch == ' '; }
static inline bool target(char ch) { return !(ch <= 32 || ch == 127); }
static inline bool http_version(char ch) { return isalnum(ch) || ch == '.' || ch == '/'; }
static inline bool COLON(char ch) { return ch == ':'; }
static inline bool FIELDVAL(char ch) { return !(ch < 32 || ch == 127); }
static inline bool ANY(char ch) { (void)ch; return true; }

static inline bool lexer_advance(Lexer *lexer, Slice *lexeme, bool(*validate)(char))
{
    *lexeme = (Slice){ lexer->buf + lexer->curr, 0 };
    while (lexer->curr < lexer->len && validate(lexer->buf[lexer->curr])) {
        lexer->curr++;
        lexeme->len++;
    }
    return lexer->curr < lexer->len;
}

static inline void lexer_skip(Lexer *lexer, bool(*validate)(char))
{
    while (lexer->curr < lexer->len && validate(lexer->buf[lexer->curr])) lexer->curr++;
}

static HTTPTargetForm get_target_form(Slice *target)
{
    if (target->len <= 0) goto malformed;

    if (slice_begins_with_str(target, "http"))
        return TARGET_ABSOLUTE_FORM;
    else if (slice_cmp_str(target, "*"))
        return TARGET_ASTERISK_FORM;
    else if (*target->s == '/')
        return TARGET_ORIGIN_FORM;
    else goto malformed;
    
    malformed:
        return TARGET_MALFORMED;
}

static void parse_request(Lexer *lexer, HTTPRequest *req, HTTPResponse *res)
{
    // ------------------
    // -- request line --
    // ------------------
    lexer_skip(lexer, SP);
    lexer_advance(lexer, &req->method,  token);                        // method
    if (req->method.len > 16 || !is_valid_method(&req->method)) {
        res->status = STATUS_NOT_IMPLEMENTED;
        return;
    }
    lexer_skip(lexer, SP);

    lexer_advance(lexer, &req->target.raw,  target);                          // target
    req->target.form = get_target_form(&req->target.raw);
    if (req->target.form == TARGET_MALFORMED) {
        res->status = STATUS_MOVED_PERMANENTLY;
        return;
    }
    lexer_skip(lexer, SP);

    lexer_advance(lexer, &req->version, http_version);                 // version
    if (!slice_cmp_str(&req->version, VERSION)) {
        res->status = STATUS_VERSION_NOT_SUPPORTED;
        return;
    }
    lexer_skip(lexer, SP);
    lexer_skip(lexer, CR); lexer_skip(lexer, LF);
    // -------------------
    // -- Header fields --
    // -------------------
    Slice head_body_separator = {0};

    while (head_body_separator.len == 0) {
        Slice field = {0}, value = {0};
        lexer_skip(lexer, SP); lexer_advance(lexer, &field, token);
        lexer_skip(lexer, COLON);
        lexer_skip(lexer, SP); lexer_advance(lexer, &value, FIELDVAL); lexer_skip(lexer, SP);

        lexer_skip(lexer, CR); lexer_skip(lexer, LF);

        da_append(req->header, ((HTTPHeaderFieldLine){field, value}));

        lexer_skip(lexer, CR);
        lexer_advance(lexer, &head_body_separator, LF);
    }
    // ----------
    // -- Body --
    // ----------
    lexer_advance(lexer, &req->body, ANY);
    res->status = STATUS_OK;
}

static inline u32 write_status_line(HTTPStatusCode code, char* dest)
{
    return snprintf(dest, STATUS_LINE_MAX_LEN, "%s %u %s\r\n", VERSION, code, http_reason_phrase(code));
}

static inline u32 write_http_date(char *dest) {
    time_t raw_time;
    struct tm gmt_time;
    time(&raw_time);
    gmtime_r(&raw_time, &gmt_time); 
    return strftime(dest, 32, "%a, %d %b %Y %H:%M:%S GMT", &gmt_time);
}

static inline u32 write_header(char* dest, usize len)
{
    char date[32];
    write_http_date(date);
    return snprintf(dest, 512,
        "Date: %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
    , date, len);
}

static void* serve_request(void *void_fd)
{
    i32 fd = *(i32 *)void_fd;
    free(void_fd);

    HTTPRequest req = {0};
    HTTPResponse res = {0};
    char buf[2048];
    
    ssize_t bytes = recv(fd, buf, sizeof(buf) - 1, 0);
    if (bytes > 0) {
        Lexer lexer = { buf, 0, bytes };
        parse_request(&lexer, &req, &res);
        debug_http_request(&req);

        char status_line[STATUS_LINE_MAX_LEN];
        char header[HEADER_FIELD_MAX_LEN];
        const char* body =
            "<html>"
                "<body>"
                    "<center>"
                        "<h1>I'm a skyline with no brakes</h1>"
                    "</center>"
                "</body>"
            "</html>";

        write_status_line(res.status, status_line);
        write_header(header, strlen(body));

        snprintf(buf, 2048, "%s%s\r\n%s", status_line, header, body);

        send(fd, buf, strlen(buf), 0);
        
    }
    free(req.header.content);
    close(fd);
    return NULL;
}

i32 main()
{
    DEBUG = 1;
    i32 serv_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serv_fd != -1);

    i32 opt = 1;
    setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);


    assert(bind(serv_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != -1);
    assert(listen(serv_fd, SOMAXCONN) != -1);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, KB(256));

    printf("http server running. open http://localhost:%d\n", PORT);

    for (;;) {
        struct sockaddr_in client_addr = {0};
        socklen_t addrlen = sizeof(client_addr);

        i32 client_fd = accept(serv_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }

        i32 *exclusive_fd = malloc(sizeof(i32));
        assert(exclusive_fd != NULL);
        *exclusive_fd = client_fd;
        
        pthread_t tid;

        if (pthread_create(&tid, &attr, serve_request, exclusive_fd) != 0) {
            perror("Failed to create thread");
            close(client_fd);
            free(exclusive_fd);
        }

    }

    pthread_attr_destroy(&attr);
    shutdown(serv_fd, SHUT_RD);
    close(serv_fd);
    return 0;
}
