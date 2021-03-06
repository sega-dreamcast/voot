/*  npc.h

DESCRIPTION

    NetPlayCommand system header file.

CHANGELOG

    Tue Jan 22 17:54:03 PST 2002    Scott Robinson <scott_np@dsn.itgo.com>
        Just added this changelog entry. The file has actually existed for
        quite some time.

    Tue Jan 22 23:37:22 PST 2002    Scott Robinson <scott_np@dsn.itgo.com>
        Added the debugging callback functionality.

    Sun Feb 24 00:57:12 PST 2002    Scott Robinson <scott_vo@quadhome.com>
        Added callback structure and definitions for the new client assisted
        callback functionality.

    Sun Nov 24 05:39:12 PST 2002    Scott Robinson <scott_vo@quadhome.com>
        Added condition for lower processor usage threading.

*/

#ifndef __NPC_H__
#define __NPC_H__

#include <netdb.h>
#include "vars.h"
#include "voot.h"

#define NPC_EVENT_QUEUE_SIZE    (1024 * 5)

typedef enum
{
    C_NONE,                 /* No command. */
    C_HELP,                 /* Display the help text. */

    C_CONNECT_SLAVE,        /* Connect to the given slave. */

    C_CONNECT_SERVER,       /* Connect to the given server. */

    C_LISTEN_SERVER,        /* Listens on the server port. */

    C_PACKET_FROM_SLAVE,    /* On reception of a packet from the slave. */
    C_CLOSE_SLAVE,          /* Need to close the slave socket. */

    C_PACKET_FROM_SERVER,   /* On reception of a packet from the server. */
    C_CLOSE_SERVER,         /* Need to close the server socket. */

    C_LISTEN_SOCKET,        /* Starts a polling thread. */

    C_EXIT                  /* Exit from the client. */
} npc_command;

typedef struct
{
    npc_command     type;

    /*
        C_CONNECT_SLAVE, C_CONNECT_SERVER
    */
    char            *text;

    /*
        C_CONNECT_SLAVE, C_CONNECT_SERVER, C_LISTEN_SERVER
    */
    uint16          port;

    /*
        C_PACKET_FROM_SLAVE, C_PACKET_FROM_SERVER
    */
    voot_packet     *packet;

    /*
        C_LISTEN_THREAD
    */
    npc_command     listen_type;
    volatile int32  *listen_socket;
    pthread_t       *listen_socket_thread;

    /*
        C_EXIT
    */
    int32           code;

} npc_command_t;

typedef struct
{
    volatile int32  *socket;
    npc_command     type;
} npc_io_check_t;

typedef enum    /* This is all a rather blatant ripoff from syslog - but they've been using it for years, so I see no reason to improve. */
{
    LOG_EMERG,      /* system is unusable */
    LOG_ALERT,      /* action must be taken immediately - system level problem with global effects */
    LOG_CRIT,       /* critical conditions - system level problems */
    LOG_ERR,        /* error conditions - internal level problems */
    LOG_WARNING,    /* warning conditions - something bad but handable happened internally */
    LOG_NOTICE,     /* normal, but significant, condition - normal users would want to see this */
    LOG_INFO,       /* informational message - nice for me, but not for final release */
    LOG_DEBUG       /* debug-level message - hyper verbose debugging, even for me */
} npc_log_level;

typedef void npc_log_callback_f (npc_log_level, const char *, ...);
typedef bool npc_packet_callback_f (uint8 type, const voot_packet *);

typedef struct
{
    npc_log_callback_f      *log;
    npc_packet_callback_f   *packet;
} npc_callbacks_t;

typedef struct
{
    char                *slave_name;
    uint16              slave_port;
    volatile int32      slave_socket;
    pthread_t           slave_poll_thread;

    char                *server_name;
    uint16              server_port;
    volatile int32      server_socket;
    volatile int32      server_socket_wait;
    pthread_t           server_listen_thread;
    pthread_t           server_poll_thread;

    npc_command_t*      event_queue[NPC_EVENT_QUEUE_SIZE];
    uint32              event_queue_size;
    uint32              event_queue_tail;
    pthread_mutex_t     event_queue_busy;
    pthread_cond_t      event_queue_alert;

    npc_callbacks_t     callbacks;
} npc_data_t;

#define NPC_LOG(sys, sev, args...) (sys.callbacks.log)(sev, ## args)
#define CLIENT_PACKET(sys, type, pkt) (sys.callbacks.packet)(type, pkt)

int32 npc_handle_command(npc_command_t *command);
bool npc_add_event_queue(npc_command_t *command);
npc_command_t* npc_get_event_queue(void);
npc_command_t* npc_get_event(void);
void* npc_io_check(void *in_arg);
int npc_connect(char *dest_name, uint16 dest_port, int32 conntype);
int npc_server_listen(void);
void npc_exit(int code);
void npc_init(npc_callbacks_t *callbacks);
npc_data_t* npc_expose(void);

#endif
