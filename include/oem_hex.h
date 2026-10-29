#ifndef EFERGY_OEM_HEX_H
#define EFERGY_OEM_HEX_H
#include <stdint.h>
typedef uint8_t (*oem_read_fn)(uint32_t,uint8_t *,uint16_t);
typedef uint8_t (*oem_emit_fn)(uint8_t);
/* Reconstruct the stock-loader carrier from a full 128 KiB PIC image. */
uint8_t oem_hex_generate(oem_read_fn read,oem_emit_fn emit);
#endif
