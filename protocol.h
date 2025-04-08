#pragma once

/**
 *  Regular expressions for the fileds of a request line.
 */

#define TYPE_REGEX   "[a-zA-Z]{1,8}"
#define URI_REGEX    "/[a-zA-Z0-9.-]{1,63}"
#define HTTP_REGEX   "HTTP/[0-9].[0-9]"
#define HTTP_VERSION "HTTP/1.1"

#define HEADER_FIELD_REGEX "[a-zA-Z0-9.-]{1,128}"
#define HEADER_VALUE_REGEX "[ -~]{1,128}"

#define MAX_HEADER_LEN 2048
