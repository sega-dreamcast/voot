/*  trap.c

    Serial trapping, tapping, and injection logic.

*/

#include "vars.h"
#include "voot.h"
#include "exception-lowlevel.h"
#include "exception.h"
#include "serial.h"
#include "trap.h"

/*
    SCIF Write:
        WRITE on 0xFFE8000C in R2   (PC: 8c0397f4)
        Each write occurs within a single pageflip.

    SCIF Read:
        READ  on 0xFFE80014 in R3   (PC: 8c039b58)
*/

#define PHY_FIFO_SIZE   16
#define NET_FIFO_SIZE   64

struct
{
    struct
    {
        uint8   data;
        dir_e   direction;
    } data[PHY_FIFO_SIZE];

    uint32      start;
    uint32      size;
} phy_fifo;

struct
{
    char        data[NET_FIFO_SIZE];

    uint32      start;
    uint32      size; 
} net_fifo;

/*
 *  Module Initialization Functions
 */

void init_ubc_b_serial(void)
{
    exception_table_entry new;

    /* STAGE: Configure UBC Channel B for breakpoint on serial port access */
    *UBC_R_BARB = 0xFFE80000;
    *UBC_R_BAMRB = UBC_BAMR_NOASID | UBC_BAMR_MASK_10;
    *UBC_R_BBRB = UBC_BBR_RW | UBC_BBR_OPERAND;

    ubc_wait();

    /* STAGE: Add exception handler for serial access. */
    new.type = EXP_TYPE_GEN;
    new.code = EXP_CODE_UBC;
    new.handler = serial_handler;

    add_exception_handler(&new);

    /* STAGE: Add exception handler for RXI interrupts. */
    new.type = EXP_TYPE_INT;
    new.code = EXP_CODE_RXI;
    new.handler = rxi_handler;

    add_exception_handler(&new);
} 

/*
 *  Network FIFO Interface Wrapper
 */

static uint32 net_size(void)
{
    return net_fifo.size;
}

static int32 net_fifo_del(void)
{
    uint32 out_data;

    if (!net_fifo.size)
        return -1;

    /* STAGE: Retrieve the current byte in the net fifo. */
    out_data = net_fifo.data[net_fifo.start];

    /* STAGE: Flush the current byte in the net fifo. */
    net_fifo.start = ++net_fifo.start % NET_FIFO_SIZE;
    net_fifo.size--;

    return out_data;
}

static bool net_fifo_add(uint8 in_data)
{
    uint32 fifo_index;

    if (net_fifo.size >= NET_FIFO_SIZE)
        return FALSE;

    /* STAGE: Add the byte to the end of the network fifo. */
    fifo_index = (net_fifo.start + net_fifo.size) % NET_FIFO_SIZE;
    net_fifo.data[fifo_index] = in_data;
    net_fifo.size++;

    return TRUE;
}

/*
 *  Physical FIFO Interface Wrapper
 */

static bool phy_fifo_add(uint8 in_data, dir_e dir, bool touch_serial)
{
    uint32 fifo_index;

    /* STAGE: Do we have space in the physical fifo? */
    if (phy_fifo.size >= PHY_FIFO_SIZE)
        return FALSE;

    /* STAGE: Add the byte to the SCIF, if requested. */
    if (touch_serial)
    {
        uint32 timeout_count;

        /* STAGE: Check if we can transmit on the SCIF. */
        if (!(*SCIF_R_FS & SCIF_FS_TDFE))
        {
            biudp_printf(VOOT_PACKET_TYPE_DEBUG, "SCIF not in TX mode!\n");
            return FALSE;
        }
        
        /* STAGE: Attempt to transmit the byte. */
        *SCIF_R_FTG = in_data;
        *SCIF_R_FS &= ~(SCIF_FS_TDFE | SCIF_FS_TEND);

        /* STAGE: Wait for the timeout. */
        timeout_count = 0;
        while((timeout_count < SCIF_TIMEOUT) && !(*SCIF_R_FS & SCIF_FS_TEND))
            timeout_count++;

        if (timeout_count == SCIF_TIMEOUT)
        {
            biudp_printf(VOOT_PACKET_TYPE_DEBUG, "SCIF timeout during TX.\n");
            return FALSE;
        }
    }        

    /* STAGE: Add the byte to the end of the physical fifo. */
    fifo_index = (phy_fifo.start + phy_fifo.size) % PHY_FIFO_SIZE;
    phy_fifo.data[fifo_index].data = in_data;
    phy_fifo.data[fifo_index].direction = dir;
    phy_fifo.size++;

    return TRUE;
}

static int32 phy_fifo_del(uint8 check_data, bool touch_serial)
{
    /* STAGE: Do we even have data in the physical fifo? */
    if (!phy_fifo.size)
        return -1;

    /* STAGE: Flush the current byte in the SCIF, if requested. */
    if (touch_serial)
    {
        check_data = *SCIF_R_FRD;
        *SCIF_R_FS &= ~(SCIF_FS_RDF | SCIF_FS_DR);
    }

    if (phy_fifo.data[phy_fifo.start].data != check_data)
        return -2;

    /* STAGE: Flush the current byte in the physical fifo. */
    phy_fifo.start = ++phy_fifo.start % PHY_FIFO_SIZE;
    phy_fifo.size--;

    return check_data;
}

static uint32 phy_size(void)
{
    /* STAGE: Return with number of bytes within physical fifo. */
    return phy_fifo.size;
}

static uint32 phy_flush_fifo(uint8 *temp_buffer)
{
    uint32 work_index, temp_index;

    /* STAGE: Flush all data not marked IN in the physical fifo meanwhile
        saving the remaining data in the temp_buffer */
    temp_index = 0;
    for (work_index = 0; work_index < phy_fifo.size; work_index++)
    {
        uint32 fifo_index;

        fifo_index = (phy_fifo.start + work_index) % PHY_FIFO_SIZE;

        if (phy_fifo.data[fifo_index].direction == IN)
        {
            temp_buffer[temp_index] = phy_fifo.data[fifo_index].data;
            temp_index++;
        }
    }
    
    /* STAGE: Reset the physical fifo. */
    phy_fifo.start = phy_fifo.size = 0;

    /* STAGE: Flush/syncronize the SCIF fifo. */ 
    while (*SCIF_R_FS & SCIF_FS_DR)
    {
        uint8 serial_data;

        /* DEBUG: Obtain the serial data? */
        serial_data = *SCIF_R_FRD;

        /* STAGE: Drop a single character from the SCIF fifo. */
        *SCIF_R_FS &= ~(SCIF_FS_RDF | SCIF_FS_DR);
    }

    /* DEBUG: Notification of physical FIFO flush */
    biudp_printf(VOOT_PACKET_TYPE_DEBUG, "Physical FIFO flush and syncronized!\n");

    return temp_index;
}

static void phy_sync(void)
{
    uint8 temp_buffer[PHY_FIFO_SIZE];
    uint32 work_index, temp_index;

    /* STAGE: Flush the physical fifo and save the IN bytes within temp_buffer */
    temp_index = 0;
    if (phy_size())
        temp_index = phy_flush_fifo(temp_buffer);

    /* STAGE: Inject the temporary buffer data in both the SCIF and physical fifos. */
    for (work_index = 0; work_index < temp_index; work_index++)
    {
        if(!phy_fifo_add(temp_buffer[work_index], IN, TRUE))
        {
            biudp_printf(VOOT_PACKET_TYPE_DEBUG, "Physical fifo overflow in resynchronization!\n");
            break;
        }
    }

    /* STAGE: Transfer data from the net fifo to the physical fifo - marker IN */
    while (phy_size() < PHY_FIFO_SIZE && net_size())
    {
        int8 net_in_data;

        net_in_data = net_fifo_del();

        if (net_in_data >= 0x0 && !phy_fifo_add(net_in_data, IN, TRUE))
        {
            biudp_printf(VOOT_PACKET_TYPE_DEBUG, "Physical fifo overflow in net injection!\n");
            break;
        }
    }

    /* DEBUG: Notification of resynchronization completion. */
    biudp_printf(VOOT_PACKET_TYPE_DEBUG, "Fifos resynchronized. %u bytes retained.\n", phy_size());
}

/*
 *  External Module Access Interface
 */
 
uint32 trap_inject_data(const uint8 *data, uint32 size)
{
    uint32 data_index;

    /* STAGE: Add incoming data to net fifo. */
    for (data_index = 0; data_index < size; data_index++)
    {
        if (!net_fifo_add(data[data_index]))
        {
            biudp_printf(VOOT_PACKET_TYPE_DEBUG, "Net fifo overflow in RX!\n");
            break;
        }
    }

    /* STAGE: Resynchronize net and physical fifos. */
    phy_sync();

    return data_index;
}

/*
 *  Exception and Interrupt Handlers
 */

void* rxi_handler(register_stack *stack, void *current_vector)
{
    void *return_vector;

    /* STAGE: If something horrible happens, we don't want VOOT freaking out about it. */
    return_vector = my_exception_finish;

    /* TODO: In the future, instead of resynchronizing the whole physical
        system on every RXI we can just wait until a non IN bytes comes
        through. Only when that occurs do we actually resyncronize. I
        imagine that would reduce a bit of lag. */

    /* STAGE: Synchronize the physical fifo. */
    phy_sync();

    /* STAGE: If there is any remaining data in the physical fifo, pass on the interrupt. */
    if (phy_size())
        return_vector = current_vector;

    return return_vector;
}

static void* my_serial_handler(register_stack *stack, void *current_vector)
{
    /* STAGE: Reconfigure serial port for testing. */
    serial_set_baudrate(57600);
    *SCIF_R_FC |= SCIF_FC_LOOP;

    /* STAGE: SPC based trap checker. */
    switch (spc())
    {
        /* STAGE: Trapped transmission. */
        case 0x8c0397f4:
            /* DEBUG: Immediate mode dumping of outgoing serial data. */
            biudp_printf(VOOT_PACKET_TYPE_DATA, "%c", stack->r2);

            /* TODO (future): Add outgoing data to per-frame dump buffer. */

            /* STAGE: Add outgoing data to physical fifo. */
            if (!phy_fifo_add(stack->r2, OUT, FALSE))
                biudp_printf(VOOT_PACKET_TYPE_DEBUG, "Physical fifo overflow in TX!\n");

            break;        

        /* STAGE: Trapped reception. */
        case 0x8c039b58:
            /* DEBUG: Immediate mode dumping of incoming serial data. */
            biudp_printf(VOOT_PACKET_TYPE_DEBUG, "<%c\n", stack->r3);

            /* STAGE: Remove incoming data from physical fifo. */
            if (phy_fifo_del(stack->r3, FALSE) < 0)
                biudp_printf(VOOT_PACKET_TYPE_DEBUG, "Physical fifo underflow in RX!\n");

            break;
    }

    return current_vector;
}

void* serial_handler(register_stack *stack, void *current_vector)
{
    /* STAGE: We only break on the serial (channel B) exception. */
    if (*UBC_R_BRCR & UBC_BRCR_CMFB)
    {
        /* STAGE: Be sure to clear the proper bit. */
        *UBC_R_BRCR &= ~(UBC_BRCR_CMFB);

        /* STAGE: Pass control to the actual code base. */
        return my_serial_handler(stack, current_vector);
    }
    else
        return current_vector;
}