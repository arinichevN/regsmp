
#include "lib/pid.h"
#include "main.h"

Peer *getPeerById(char *id, const PeerList *list) {
    LIST_GET_BY_IDSTR
}

SensorFTS * getSensorById(int id, const SensorFTSList *list) {
    LIST_GET_BY_ID
}

Prog * getProgById(int id, const ProgList *list) {
    LLIST_GET_BY_ID(Prog)
}

EM * getEMById(int id, const EMList *list) {
    LIST_GET_BY_ID
}

int lockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_lock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProgList: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_trylock(&(progl_mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_unlock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProgList: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

int lockProg(Prog *item) {
    if (pthread_mutex_lock(&(item->mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProg: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProg(Prog *item) {
    if (pthread_mutex_trylock(&(item->mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProg(Prog *item) {
    if (pthread_mutex_unlock(&(item->mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProg: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

void saveProgActive(Prog *item, int active) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update " APP_NAME_STR ".prog set active=%d where id=%d", active, item->id);
    if ((r = dbGetDataC(db_conn_data, q, q)) != NULL) {
        PQclear(r);
    }
}

int checkSensor(const SensorFTS *item) {
    if (item->source == NULL) {
        fprintf(stderr, "checkSensor: no data source where id = %d\n", item->id);
        return 0;
    }
    return 1;
}

int checkEM(const EMList *list) {
    size_t i, j;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].source == NULL) {
            fprintf(stderr, "checkEm: no data source where id = %d\n", list->item[i].id);
            return 0;
        }
    }
    //unique id
    for (i = 0; i < list->length; i++) {
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i].id == list->item[j].id) {
                fprintf(stderr, "checkEm: id is not unique where id = %d\n", list->item[i].id);
                return 0;
            }
        }
    }
    return 1;
}

int checkProg(const Prog *item, const ProgList *list) {
    if (item->sensor.source == NULL) {
        fprintf(stderr, "checkProg: no sensor attached to prog with id = %d\n", item->id);
        return 0;
    }
    /*
        if (item->em_c == NULL) {
            fprintf(stderr, "checkProg: no cooler EM attached to prog with id = %d\n", item->id);
            return 0;
        }
        if (item->em_h == NULL) {
            fprintf(stderr, "checkProg: no heater EM attached to prog with id = %d\n", item->id);
            return 0;
        }
     */
    switch (item->pid_h.mode) {
        case PID_MODE_HEATER:
        case PID_MODE_COOLER:
            break;
        default:
            fprintf(stderr, "checkProg: bad mode for prog with id = %d\n", item->id);
            return 0;
    }
    switch (item->pid_c.mode) {
        case PID_MODE_HEATER:
        case PID_MODE_COOLER:
            break;
        default:
            fprintf(stderr, "checkProg: bad mode for prog with id = %d\n", item->id);
            return 0;
    }
    if (item->delta < 0) {
        fprintf(stderr, "checkProg: bad delta (delta>=0 expected) where prog_id = %d\n", item->id);
        return 0;
    }
    //unique id
    if (getProgById(item->id, list) != NULL) {
        fprintf(stderr, "checkProg: prog with id = %d is already running\n", item->id);
        return 0;
    }
    return 1;
}

int getEMByIdFdb(EM *item, int id, const PeerList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select peer_id, remote_id, pwm_rsl from " APP_NAME_STR ".em_mapping where app_class='%s' and em_id=%d", app_class, id);
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
#ifdef MODE_DEBUG
        fputs("getEMByIdFdb: only one tuple expected\n", stderr);
#endif
        PQclear(r);
        return 0;
    }
    if (!initMutex(&item->mutex)) {
        PQclear(r);
        return 0;
    }
    item->id = id;
    item->source = getPeerById(PQgetvalue(r, 0, 0), &peer_list);
    item->remote_id = atoi(PQgetvalue(r, 0, 1));
    item->pwm_rsl = atof(PQgetvalue(r, 0, 2));
    item->last_output = 0.0f;

    PQclear(r);
    return 1;
}

int getPIDByIdFdb(PID *item, int id) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select kp, ki, kd from " APP_NAME_STR ".pid where id=%d", id);
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
#ifdef MODE_DEBUG
        fputs("getPIDByIdFdb: only one tuple expected\n", stderr);
#endif
        PQclear(r);
        return 0;
    }
    item->kp = atof(PQgetvalue(r, 0, 0));
    item->ki = atof(PQgetvalue(r, 0, 1));
    item->kd = atof(PQgetvalue(r, 0, 2));
    PQclear(r);
    return 1;
}

int getDeltaByIdFdb(float *item, int id) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select delta from " APP_NAME_STR ".onf where id=%d", id);
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
#ifdef MODE_DEBUG
        fputs("getDeltaByIdFdb: only one tuple expected\n", stderr);
#endif
        PQclear(r);
        return 0;
    }
    *item = atof(PQgetvalue(r, 0, 0));
    PQclear(r);
    return 1;
}

int getGapByIdFdb(struct timespec *item, int id) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select extract(epoch from amount) amount from " APP_NAME_STR ".gap where id=%d", id);
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
#ifdef MODE_DEBUG
        fputs("getGapByIdFdb: only one tuple expected\n", stderr);
#endif
        PQclear(r);
        return 0;
    }
    item->tv_sec = (int) atof(PQgetvalue(r, 0, 0));
    item->tv_nsec = 0;
    PQclear(r);
    return 1;
}

int getSensorByIdFdb(SensorFTS *item, int sensor_id, const PeerList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    snprintf(q, sizeof q, "select peer_id, remote_id from " APP_NAME_STR ".sensor_mapping where app_class='%s' and sensor_id=%d", app_class, sensor_id);
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
        PQclear(r);
        return 0;
    }
    if (!initMutex(&item->mutex)) {
        PQclear(r);
        return 0;
    }
    item->id = sensor_id;
    item->source = getPeerById(PQgetvalue(r, 0, 0), pl);
    item->remote_id = atoi(PQgetvalue(r, 0, 1));
    memset(&item->value, 0, sizeof item->value);

    PQclear(r);
    if (!checkSensor(item)) {
        return 0;
    }
    return 1;
}

Prog *getProgFromDbr(PGresult *r, int ind, const PeerList *pl) {
    Prog *item = (Prog *) malloc(sizeof *(item));
    if (item == NULL) {
#ifdef MODE_DEBUG
        fputs("getProgByIdFdb: failed to allocate memory\n", stderr);
#endif
        return NULL;
    }
    memset(item, 0, sizeof *item);
    item->id = atoi(PQgetvalue(r, ind, 0));
    if (!getSensorByIdFdb(&item->sensor, atoi(PQgetvalue(r, ind, 1)), pl)) {
        free(item);
        return NULL;
    }
    if (!getEMByIdFdb(&item->em_h, atoi(PQgetvalue(r, ind, 2)), pl)) {
        free(item);
        return NULL;
    }
    if (!getEMByIdFdb(&item->em_c, atoi(PQgetvalue(r, ind, 3)), pl)) {
        free(item);
        return NULL;
    }
    item->goal = atof(PQgetvalue(r, ind, 4));
    char *mode = PQgetvalue(r, ind, 5);
    if (strncmp(mode, MODE_PID_STR, MODE_SIZE) == 0) {
        item->mode = CNT_PID;
    } else if (strncmp(mode, MODE_ONF_STR, MODE_SIZE) == 0) {
        item->mode = CNT_ONF;
    } else {
#ifdef MODE_DEBUG
        fputs("getProgByIdFdb: bad mode\n", stderr);
#endif
        free(item);
        return NULL;
    }
    if (!getPIDByIdFdb(&item->pid_h, atoi(PQgetvalue(r, ind, 6)))) {
        free(item);
        return NULL;
    }
    item->pid_h.min_output = 0.0f;
    item->pid_h.mode = PID_MODE_HEATER;
    if (!getPIDByIdFdb(&item->pid_c, atoi(PQgetvalue(r, ind, 7)))) {
        free(item);
        return NULL;
    }
    item->pid_c.min_output = 0.0f;
    item->pid_c.mode = PID_MODE_COOLER;
    if (!getDeltaByIdFdb(&item->delta, atoi(PQgetvalue(r, ind, 8)))) {
        free(item);
        return NULL;
    }
    int active = atoi(PQgetvalue(r, ind, 9));
    if (!getGapByIdFdb(&item->change_gap, atoi(PQgetvalue(r, ind, 10)))) {
        free(item);
        return NULL;
    }
    if (!active) {
        saveProgActive(item, 1);
    }
    item->state = INIT;
    item->output = 0.0f;
    item->state_r = OFF;
    item->next = NULL;
    return item;
}

Prog * getProgByIdFdb(int id, PeerList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select " PROG_FIELDS " from " APP_NAME_STR ".prog where id=%d", id);
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return NULL;
    }
    if (PQntuples(r) != 1) {
#ifdef MODE_DEBUG
        fputs("getProgByIdFdb: only one tuple expected\n", stderr);
#endif
        PQclear(r);
        return NULL;
    }

    Prog *item = getProgFromDbr(r, 0, pl);
    PQclear(r);
    if (item == NULL) {
        return NULL;
    }
    if (!initMutex(&item->mutex)) {
        free(item);
        return NULL;
    }
    if (!checkProg(item, &prog_list)) {
        free(item);
        return NULL;
    }
    return item;
}

int loadActiveProg(ProgList *list, PeerList *pl) {
    PGresult *r;
    char *q = "select " PROG_FIELDS " from " APP_NAME_STR ".prog where active=1";
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return 0;
    }
    int n = PQntuples(r);
    int i;
    for (i = 0; i < n; i++) {
        Prog *item = getProgFromDbr(r, i, pl);
        if (item == NULL) {
            continue;
        }
        if (!initMutex(&item->mutex)) {
            free(item);
            continue;
        }
        if (!checkProg(item, list)) {
            free(item);
            continue;
        }
        if (!addProg(item, list)) {
            free(item);
            continue;
        }
    }
    PQclear(r);
    return 1;
}

void loadAllProg(ProgList *list, PeerList *pl) {
    PGresult *r;
    char *q = "select " PROG_FIELDS " from " APP_NAME_STR ".prog";
    if ((r = dbGetDataT(db_conn_data, q, q)) == NULL) {
        return;
    }
    int n = PQntuples(r);
    int i;
    for (i = 0; i < n; i++) {
        Prog *item = getProgFromDbr(r, i, pl);
        if (item == NULL) {
            continue;
        }
        if (!initMutex(&item->mutex)) {
            free(item);
            continue;
        }
        if (!checkProg(item, list)) {
            free(item);
            continue;
        }
        if (!addProg(item, list)) {
            free(item);
            continue;
        }
    }
    PQclear(r);

}

struct timespec getTimeRestChange(const Prog *item) {
    struct timespec out = {-1, -1};
    int value_is_out = 0;
    switch (item->state_r) {
        case HEATER:
            if (VAL_IS_OUT_H) {
                value_is_out = 1;
            }
            break;
        case COOLER:
            if (VAL_IS_OUT_C) {
                value_is_out = 1;
            }
            break;
    }
    if (value_is_out) {
        out = getTimeRest_ts(item->change_gap, item->tmr.start);
    }
    return out;
}

char * getStateStr(char state) {
    switch (state) {
        case OFF:
            return "OFF";
            break;
        case INIT:
            return "INIT";
            break;
        case REG:
            return "REG";
            break;
        case NOREG:
            return "NOREG";
            break;
        case COOLER:
            return "COOLER";
            break;
        case HEATER:
            return "HEATER";
            break;
        case CNT_PID:
            return "PID";
            break;
        case CNT_ONF:
            return "ONF";
            break;
    }
    return "\0";
}

int bufCatProgRuntime(const Prog *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    char *state = getStateStr(item->state);
    char *state_r = getStateStr(item->state_r);
    struct timespec tm_rest = getTimeRestChange(item);
    snprintf(q, sizeof q, "%d_%s_%s_%f_%f_%ld_%f_%d\n",
            item->id,
            state,
            state_r,
            item->output_heater,
            item->output_cooler,
            tm_rest.tv_sec,
            item->sensor.value.value,
            item->sensor.value.state
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int bufCatProgInit(const Prog *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    char *mode = getStateStr(item->mode);
    struct timespec tm_rest = getTimeRestChange(item);
    snprintf(q, sizeof q, "%d_%f_%s_%f_%f_%f_%f_%f_%f_%f_%ld\n",
            item->id,
            item->goal,
            mode,
            item->delta,

            item->pid_h.kp,
            item->pid_h.ki,
            item->pid_h.kd,

            item->pid_c.kp,
            item->pid_c.ki,
            item->pid_c.kd,

            item->change_gap.tv_sec
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int sendStrPack(char qnf, const char *cmd) {
    extern size_t udp_buf_size;
    extern Peer peer_client;
    return acp_sendStrPack(qnf, cmd, udp_buf_size, &peer_client);
}

int sendBufPack(char *buf, char qnf, const char *cmd_str) {
    extern size_t udp_buf_size;
    extern Peer peer_client;
    return acp_sendBufPack(buf, qnf, cmd_str, udp_buf_size, &peer_client);
}

void sendStr(const char *s, uint8_t *crc) {
    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {
    acp_sendFooter(crc, &peer_client);
}

void waitThread_ctl(char cmd) {
    thread_cmd = cmd;
    pthread_join(thread, NULL);
}

void printAll(ProgList *list, PeerList *pl) {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    size_t i;
    snprintf(q, sizeof q, "prog_list length: %ld\n", list->length);
    sendStr(q, &crc);
    sendStr("+------------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                  Program                                             |\n", &crc);
    sendStr("+-----------+------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|    id     | mode |    goal   |   delta   | change_gap|   state   | state_r   | out_heater| out_cooler|\n", &crc);
    sendStr("+-----------+------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_DF
    PROG_LIST_LOOP_ST
            char *mode = getStateStr(curr->mode);
    char *state = getStateStr(curr->state);
    char *state_r = getStateStr(curr->state_r);
    snprintf(q, sizeof q, "|%11d|%6s|%11.3f|%11.3f|%11ld|%11s|%11s|%11.3f|%11.3f|\n",
            curr->id,
            mode,
            curr->goal,
            curr->delta,
            curr->change_gap.tv_sec,
            state,
            state_r,
            curr->output_heater,
            curr->output_cooler
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-----------+------------------------------------------------------------------+------------------------------------------------------------------+\n", &crc);
    sendStr("|   Prog    |                             PID heater                           |                             PID cooler                           |\n", &crc);
    sendStr("|-----------+------+-----------+-----------+-----------+-----------+-----------+------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|    id     | mode |    kp     |    ki     |    kd     |  ierr     |  preverr  | mode |    kp     |    ki     |    kd     |  ierr     |  preverr  |\n", &crc);
    sendStr("+-----------+------+-----------+-----------+-----------+-----------+-----------+------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%6c|%11.3f|%11.3f|%11.3f|%11.1f|%11.1f|%6c|%11.3f|%11.3f|%11.3f|%11.1f|%11.1f|\n",
            curr->id,

            curr->pid_h.mode,
            curr->pid_h.kp,
            curr->pid_h.ki,
            curr->pid_h.kd,
            curr->pid_h.integral_error,
            curr->pid_h.previous_error,

            curr->pid_c.mode,
            curr->pid_c.kp,
            curr->pid_c.ki,
            curr->pid_c.kd,
            curr->pid_c.integral_error,
            curr->pid_c.previous_error
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+------+-----------+-----------+-----------+-----------+-----------+------+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                       Peer                                          |\n", &crc);
    sendStr("+--------------------------------+-----------+-----------+----------------+-----------+\n", &crc);
    sendStr("|               id               |   link    |  udp_port |      addr      |     fd    |\n", &crc);
    sendStr("+--------------------------------+-----------+-----------+----------------+-----------+\n", &crc);
    for (i = 0; i < pl->length; i++) {
        snprintf(q, sizeof q, "|%32s|%11p|%11u|%16u|%11d|\n",
                pl->item[i].id,
                &pl->item[i],
                pl->item[i].addr.sin_port,
                pl->item[i].addr.sin_addr.s_addr,
                *pl->item[i].fd
                );
        sendStr(q, &crc);
    }
    sendStr("+--------------------------------+-----------+-----------+----------------+-----------+\n", &crc);

    sendStr("+-----------------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                    Prog EM                                                |\n", &crc);
    sendStr("+-----------+-----------------------------------------------+-----------------------------------------------+\n", &crc);
    sendStr("|           |                   EM heater                   |                  EM cooler                    |\n", &crc);
    sendStr("+           +-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|  prog_id  |     id    | remote_id |  pwm_rsl  | peer_link |     id    | remote_id |  pwm_rsl  | peer_link |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%11d|%11d|%11f|%11p|%11d|%11d|%11f|%11p|\n",
            curr->id,

            curr->em_c.id,
            curr->em_c.remote_id,
            curr->em_c.pwm_rsl,
            curr->em_c.source,

            curr->em_h.id,
            curr->em_h.remote_id,
            curr->em_h.pwm_rsl,
            curr->em_h.source
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|    Prog   |                                   Sensor                                     |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+------------------------------------------+\n", &crc);
    sendStr("|           |           |           |           |                   value                  |\n", &crc);
    sendStr("|           |           |           |           |-----------+-----------+-----------+------+\n", &crc);
    sendStr("|    id     |    id     | remote_id | peer_link |   temp    |    sec    |   nsec    | state|\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+------+\n", &crc);
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%11d|%11d|%11p|%11f|%11ld|%11ld|%6d|\n",
            curr->id,
            curr->sensor.id,
            curr->sensor.remote_id,
            curr->sensor.source,
            curr->sensor.value.value,
            curr->sensor.value.tm.tv_sec,
            curr->sensor.value.tm.tv_nsec,
            curr->sensor.value.state
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+------+\n", &crc);
    sendFooter(crc);
}

void printHelp() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("COMMAND LIST\n", &crc);
    snprintf(q, sizeof q, "%c\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tterminate process\n", ACP_CMD_APP_EXIT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tload prog into RAM and start its execution; program id expected if '.' quantifier is used\n", ACP_CMD_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tunload program from RAM; program id expected if '.' quantifier is used\n", ACP_CMD_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tunload program from RAM, after that load it; program id expected if '.' quantifier is used\n", ACP_CMD_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tswitch prog state between REG and NOREG; program id expected if '.' quantifier is used\n", ACP_CMD_REGSMP_PROG_SWITCH_STATE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset heater power; program id expected if '.' quantifier is used\n", ACP_CMD_REGSMP_PROG_SET_HEATER_POWER);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset cooler power; program id expected if '.' quantifier is used\n", ACP_CMD_REGSMP_PROG_SET_COOLER_POWER);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget prog runtime data in format:  progId_state_stateEM_output_timeRestSecToEMSwap; program id expected if '.' quantifier is used\n", ACP_CMD_REGSMP_PROG_GET_DATA_RUNTIME);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget prog initial data in format;  progId_setPoint_mode_ONFdelta_PIDheaterKp_PIDheaterKi_PIDheaterKd_PIDcoolerKp_PIDcoolerKi_PIDcoolerKd; program id expected if '.' quantifier is used\n", ACP_CMD_REGSMP_PROG_GET_DATA_INIT);
    sendStr(q, &crc);
    sendFooter(crc);
}
