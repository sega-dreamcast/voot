/*  main.c

DESCRIPTION

    The code first ran by the first stage loader. We quickly abandon this
    shell and hook to various parts of the system.

*/

#include "vars.h"
#include "exception-lowlevel.h"
#include "exception.h"
#include "util.h"
#include "assert.h"
#include "warez_load.h"

#define LOADED_POINT        0x8C300000
#define REAL_LOAD_POINT     0x8C010000

int32 dc_main(int32 do_warez)
{
    unsigned long bin_size;

    /* STAGE: Initialize the UBC. */
    *UBC_R_BBRA = *UBC_R_BBRB = 0;
    *UBC_R_BRCR = UBC_BRCR_UBDE | UBC_BRCR_PCBA | UBC_BRCR_PCBB;
    dbr_set(exception_handler_lowlevel);
    vbr_set(vbr_buffer);

    /* STAGE: Initialize both UBC channels. */
    init_ubc_a_exception();

    /* STAGE: Wait enough cycles for the UBC to be working properly. */
    ubc_wait();

    /* STAGE: Handle the 1ST_READ.BIN */
    if (do_warez)
    {
        warez_load(*((unsigned long *) REAL_LOAD_POINT));
    }
    else
    {
        /* STAGE: Relocate the 1st_read.bin */
        bin_size = *((unsigned long *) REAL_LOAD_POINT);
        memmove((uint8 *) REAL_LOAD_POINT, (uint8 *) LOADED_POINT, bin_size);

        /* STAGE: Execute the 1ST_READ.BIN */
        (*(void (*)()) REAL_LOAD_POINT) ();
    }

    /* STAGE: Freeze it in an interesting fashion. */
    assert(0);
}
