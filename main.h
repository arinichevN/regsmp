
#ifndef REGSMP_MAIN_H
#define REGSMP_MAIN_H


#include "lib/dbl.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/gpio.h"
#include "lib/app.h"
#include "lib/configl.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/tsv.h"
#include "lib/regpidonfhc.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/regulator.h"
#include "lib/acp/prog.h"
#include "lib/acp/regonf.h"
#include "lib/acp/regsmp.h"

#define APP_NAME regsmp
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif
#define CONFIG_FILE "" CONF_DIR "config.tsv"

#define WAIT_RESP_TIMEOUT 3

#define MODE_SIZE 3

#define FLOAT_NUM "%.2f"

struct channel_st {
    int id;
    RegPIDOnfHC prog;
    int save;
    uint32_t error_code;
    
    int sock_fd;
    struct timespec cycle_duration;
    pthread_t thread;
    Mutex mutex;
    struct channel_st *next;
};

typedef struct channel_st Channel;

DEC_LLIST(Channel)

typedef struct {
    sqlite3 *db_data;
    EMList *em_list;
    SensorFTSList *sensor_list;
    Channel *channel;
    ChannelList *channel_list;
    Mutex * channel_list_mutex;
} ChannelData;

extern int readSettings() ;

extern void initApp() ;

extern int initData() ;

extern void serverRun(int *state, int init_state) ;

extern void progControl(Channel *item) ;

extern void *threadFunction(void *arg) ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;

#endif 

