
#ifndef REGSMP_MAIN_H
#define REGSMP_MAIN_H


#include "lib/db.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/gpio.h"
#include "lib/pid.h"
#include "lib/app.h"
#include "lib/config.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/regsmp.h"
#include "lib/acp/lck.h"

#define APP_NAME regsmp
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifndef MODE_DEBUG
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifdef MODE_DEBUG
#define CONF_DIR "./"
#endif

#define CONFIG_FILE_DB "" CONF_DIR "main.conf"

#define PROG_FIELDS "id, sensor_id, em_heater_id, em_cooler_id, goal, mode, heater_pid_id, cooler_pid_id, onf_id, active, change_gap_id"

#define WAIT_RESP_TIMEOUT 3
#define LOCK_COM_INTERVAL 1000000U

#define MODE_PID_STR "pid"
#define MODE_ONF_STR "onf"
#define MODE_SIZE 3

#define PROG_LIST_LOOP_DF Prog *curr = prog_list.top;
#define PROG_LIST_LOOP_ST while (curr != NULL) {
#define PROG_LIST_LOOP_SP curr = curr->next; } curr = prog_list.top;

#define SNSR_VAL item->sensor.value.value
#define SNSR_TM item->sensor.value.tm

#define VAL_IS_OUT_H SNSR_VAL > item->goal + item->delta
#define VAL_IS_OUT_C SNSR_VAL < item->goal - item->delta
enum {
    OFF,
    INIT,
    REG,
    NOREG,
    COOLER,
    HEATER,
    CNT_PID,
    CNT_ONF
} StateAPP;

struct prog_st {
    int id;
    SensorFTS sensor;
    EM em_c;
    EM em_h;
    char mode;
    float goal;
    float delta;
    PID pid_h;
    PID pid_c;
    struct timespec change_gap;
    
    char state;
    char state_r;
    float output;
    
    float output_heater;
    float output_cooler;
    Ton_ts tmr;
    Mutex mutex;
    struct prog_st *next;
};

typedef struct prog_st Prog;

DEF_LLIST(Prog)

extern int readSettings() ;

extern void initApp() ;

extern int initData() ;

extern int addProg(Prog *item, ProgList *list) ;

extern int addProgById(int id, ProgList *list) ;

extern int deleteProgById(int id, ProgList *list) ;

extern int reloadProgById(int id, ProgList *list) ;

extern int sensorRead(SensorFTS *item) ;

extern int controlEM(EM *item, float output) ;

extern int saveTune(int id, float kp, float ki, float kd) ;

extern void serverRun(int *state, int init_state) ;

extern void progControl(Prog *item) ;

extern void *threadFunction(void *arg) ;

extern int createThread_ctl() ;

extern void freeProg(ProgList * list) ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;


#endif 

