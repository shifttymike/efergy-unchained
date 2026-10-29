#ifndef EFERGY_HTTP_PARSER_H
#define EFERGY_HTTP_PARSER_H
#include <stdint.h>
#include "config.h"
#define HTTP_BUFFER_SIZE 768u
enum { HTTP_STATUS=1,HTTP_CONFIG,HTTP_FLASH,HTTP_REBOOT,HTTP_INDEX,HTTP_UPDATE,HTTP_UPDATE_DATA,HTTP_UPDATE_COMMIT,HTTP_UPDATE_ROLLBACK,HTTP_UPDATE_CANCEL
#ifdef OEM_RESTORE_BUILD
,HTTP_OEM,HTTP_OEM_DATA,HTTP_OEM_COMMIT
#endif
};
typedef struct {
    char buffer[HTTP_BUFFER_SIZE];
    uint16_t used,content_length,header_bytes;
    uint8_t state,post,route,saw_cr,has_length,form_type;
} http_parser_t;
void http_parser_init(http_parser_t *p);
/* 0: more input, 200: complete, otherwise HTTP error code. */
uint16_t http_parser_feed(http_parser_t *p,uint8_t byte);
/* Patch supplied config in place; caller keeps it separate from active config. */
uint8_t config_form_apply(char *body,config_t *candidate);
#endif
