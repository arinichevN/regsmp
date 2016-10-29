
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


#define APP_NAME ("regsmp")


#ifndef MODE_DEBUG
#define CONF_FILE "/etc/controller/regsmp.conf"
#endif
#ifdef MODE_DEBUG
#define CONF_FILE "main.conf"
#endif

#define PROG_FIELDS "id, sensor_id, em_id, goal, mode, kp, ki, kd"
#define WAIT_RESP_TIMEOUT 1

#define MODE_STR_HEATER "h"
#define MODE_STR_COOLER "c"
#define STATE_STR_INIT "i"
#define STATE_STR_REG "r"
#define STATE_STR_TUNE "t"
#define UNKNOWN_STR "u"

#define PROG_LIST_LOOP_ST  Prog *curr = prog_list.top; while (curr != NULL) {
#define PROG_LIST_LOOP_SP curr = curr->next; } 
enum {
    ON = 1,
    OFF,
    DO,
    INIT,
    REG,
    TUNE, 
    STOP
} StateAPP;

typedef struct {
    int id;
    int remote_id;
    Peer *source;
    float last_output;//we will keep last output value in order not to repeat the same queries to peers
} EM; //executive mechanism

typedef struct {
    EM *item;
    size_t length;
} EMList;

typedef struct {
    int id;
    int remote_id;
    Peer *source;
    FTS value;
} Sensor;

typedef struct {
    Sensor *item;
    size_t length;
} SensorList;

typedef struct {
    float goal;
    char mode;
    float kp;
    float ki;
    float kd;
} SV; //saved data

struct prog_st {
    int id;
    Sensor *sensor;
    EM *em;
    char state;
    float goal;
    float output;
    PID pid;
    PID_AT pid_at;
    SV sv;
    Mutex mutex;
    struct prog_st *next;
};

typedef struct prog_st Prog;

typedef struct {
    Prog *top;
    Prog *last;
    size_t length;
} ProgList;

typedef struct {
    pthread_attr_t thread_attr;
    pthread_t thread;
    I1List i1l;
    F1List f1l;
    I1F1List i1f1l;
    S1List s1l;
    I1S1List i1s1l;
    char cmd;
    char qfr;
    int on;
    struct timespec cycle_duration;//one cycle minimum duration
    int created;
    int attr_initialized;
} ThreadData;

extern int readSettings() ;

extern void initApp() ;

extern int initSensor(SensorList *list, const PeerList *pl) ;

extern int initEM(EMList *list, const PeerList *pl) ;

extern Prog * getProgByIdFdb(int id, const SensorList *slist, EMList *elist) ;

extern int addProg(Prog *item, ProgList *list);

extern int addProgById(int id, ProgList *list) ;

extern int deleteProgById(int id, ProgList *list) ;

extern int switchProgById(int id, ProgList *list);

extern void loadAllProg(ProgList *list, const SensorList *slist, EMList *elist ) ;

extern int checkSensor(const SensorList *list) ;

extern int checkEM(const EMList *list) ;

extern int checkProg(const Prog *item, const ProgList *list) ;

extern void initData() ;

extern int sensorRead(Sensor *s) ;

extern int controlEM(EM *em, float output) ;

extern int saveTune(int id, float kp, float ki, float kd) ;

extern void serverRun(int *state, int init_state) ;

extern void secure() ;

extern void progControl(Prog *item) ;

extern void *threadFunction(void *arg) ;

extern int createThread(ThreadData * td, size_t buf_len) ;

extern void freeProg(ProgList *list) ;

extern void freeThread() ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;
#endif 

