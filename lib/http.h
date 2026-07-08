#pragma once

#define VERSION "HTTP/1.1"

typedef enum {
    HTTP_OK                    = 200,
    HTTP_CONTINUE              = 100,
    HTTP_NOT_MODIFIED          = 304,
    HTTP_BAD_REQUEST           = 400,
    HTTP_NOT_FOUND             = 404,
    HTTP_METHOD_NOT_ALLOWED    = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_NOT_IMPLEMENTED       = 501,
    HTTP_VERSION_NOT_SUPPORTED = 505
} HTTPStatusCode;

static inline const char* http_reason_phrase(HTTPStatusCode code)
{
    switch (code) {
        case HTTP_OK                   : return "Ok";
        case HTTP_CONTINUE             : return "Continue";
        case HTTP_NOT_MODIFIED         : return "Not Modified";
        case HTTP_BAD_REQUEST          : return "Bad Request";
        case HTTP_NOT_FOUND            : return "Not Found";
        case HTTP_METHOD_NOT_ALLOWED   : return "Method Not Allowed";
        case HTTP_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_NOT_IMPLEMENTED      : return "Not Implemented";
        case HTTP_VERSION_NOT_SUPPORTED: return "Version Not Supported";
        default                        : return "Unknown";
    }
}

