#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "vars.h"
#include "system.h"

#define VBR_GEN(tab)    ((void *) ((unsigned int) tab) + 0x100)
#define VBR_CACHE(tab)  ((void *) ((unsigned int) tab) + 0x400)
#define VBR_INT(tab)    ((void *) ((unsigned int) tab) + 0x600)

#define VO_VBR_SIZE     0x660
#define EXP_TABLE_SIZE  10

typedef struct
{
    uint32  type;
    uint32  code;
    void    *(*handler)(register_stack *, void *);
} exception_table_entry;

typedef struct
{
    /* Exception counters */
    uint32      general_exception_count;
    uint32      cache_exception_count;
    uint32      interrupt_exception_count;
    uint32      ubc_exception_count;
    uint32      odd_exception_count;

    /* Function hooks for various interrupts */
    exception_table_entry   table[EXP_TABLE_SIZE];

    /* Private status information */
    bool        vbr_switched;
} exception_table;

void init_ubc_a_exception(void);
void clear_ubc_a_exception(void);
uint32 add_exception_handler(const exception_table_entry *new_entry);
void init_asic_handler(void);

#endif
