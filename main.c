/*
 * regsmp
 */

#include "main.h"

char pid_path[LINE_SIZE];
char app_class[NAME_SIZE];

char db_conninfo_settings[LINE_SIZE];
char db_conninfo_data[LINE_SIZE];
char db_conninfo_public[LINE_SIZE];

int app_state = APP_INIT;


PGconn *db_conn_data = NULL;

int pid_file = -1;
int proc_id;
int udp_port = -1;
size_t udp_buf_size = 0;
int udp_fd = -1;
int udp_fd_tf = -1;
Peer peer_client = {.fd = &udp_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
pthread_t thread;
char thread_cmd;
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};
Peer *peer_lock = NULL;
I1List i1l = {NULL, 0};
I1F1List i1f1l = {NULL, 0};
F1List f1l = {NULL, 0};
PeerList peer_list = {NULL, 0};
ProgList prog_list = {NULL, NULL, 0};

#include "util.c"

int readSettings() {
    PGresult *r;
    char q[LINE_SIZE];
    PGconn *db_conn_settings = NULL;
    memset(pid_path, 0, sizeof pid_path);
    memset(db_conninfo_data, 0, sizeof db_conninfo_data);

    if (!initDB(&db_conn_settings, db_conninfo_settings)) {
        return 0;
    }
    snprintf(q, sizeof q, "select db_public, udp_port, udp_buf_size, pid_path, db_data, cycle_duration_us, peer_lock from " APP_NAME_STR ".config where app_class='%s'", app_class);
    if ((r = dbGetDataT(db_conn_settings, q, q)) == NULL) {
        freeDB(db_conn_settings);
        return 0;
    }
    if (PQntuples(r) != 1) {
        PQclear(r);
        freeDB(db_conn_settings);
        fputs("readSettings: ERROR: need only one tuple\n", stderr);
        return 0;
    }
    char db_conninfo_public[LINE_SIZE];
    PGconn *db_conn_public = NULL;
    memset(db_conninfo_public, 0, sizeof db_conninfo_public);
    memcpy(db_conninfo_public, PQgetvalue(r, 0, 0), sizeof db_conninfo_public);
    if (dbConninfoEq(db_conninfo_public, db_conninfo_settings)) {
        db_conn_public = db_conn_settings;
    } else {
        if (!initDB(&db_conn_public, db_conninfo_public)) {
            PQclear(r);
            freeDB(db_conn_settings);
            return 0;
        }
    }
    int done = 1;
    done = done && config_getUDPPort(db_conn_public, PQgetvalue(r, 0, 1), &udp_port);
    done = done && config_getBufSize(db_conn_public, PQgetvalue(r, 0, 2), &udp_buf_size);
    done = done && config_getPidPath(db_conn_public, PQgetvalue(r, 0, 3), pid_path, LINE_SIZE);
    done = done && config_getDbConninfo(db_conn_public, PQgetvalue(r, 0, 4), db_conninfo_data, LINE_SIZE);
    done = done && config_getCycleDurationUs(db_conn_public, PQgetvalue(r, 0, 5), &cycle_duration);
    done = done && config_getPeerList(db_conn_public, &peer_list, &udp_fd_tf);
    peer_lock = getPeerById(PQgetvalue(r, 0, 6), &peer_list);
    if (peer_lock == NULL) {
        done = done && 0;
    }
    PQclear(r);
    freeDB(db_conn_public);
    freeDB(db_conn_settings);
    if (!done) {
        fputs("readSettings: ERROR: failed to read some fields\n", stderr);
        return 0;
    }
    return 1;
}

void initApp() {
    if (!readConf(CONFIG_FILE_DB, db_conninfo_settings, app_class)) {
        exit_nicely_e("initApp: failed to read configuration file\n");
    }
#ifdef MODE_DEBUG
    printf("initApp: db_conninfo_settings: %s\n", db_conninfo_settings);
    printf("initApp: app_class: %s\n", app_class);
#endif
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }

    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
#ifdef MODE_DEBUG
    printf("initApp: PID: %d\n", proc_id);
    printf("initApp: udp_port: %d\n", udp_port);
    printf("initApp: udp_buf_size: %d\n", udp_buf_size);
    printf("initApp: pid_path: %s\n", pid_path);
    printf("initApp: db_conninfo_data: %s\n", db_conninfo_data);
    printf("initApp: cycle_duration: %ld(sec) %ld(nsec)\n", cycle_duration.tv_sec, cycle_duration.tv_nsec);
#endif
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize prog mutex\n");
    }

    if (!initUDPServer(&udp_fd, udp_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }

    if (!initUDPClient(&udp_fd_tf, WAIT_RESP_TIMEOUT)) {
        exit_nicely_e("initApp: failed to initialize udp client\n");
    }
    char cmd_unlock[1] = {ACP_CMD_LCK_UNLOCK};
    char cmd_check[1] = {ACP_CMD_LCK_GET_DATA};
    acp_waitUnlock(peer_lock, cmd_unlock, cmd_check, LOCK_COM_INTERVAL, udp_buf_size);
}

int initData() {
    if (!initDB(&db_conn_data, db_conninfo_data)) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to initialize data db connection\n", stderr);
#endif
        return 0;
    }
    if (!loadActiveProg(&prog_list, &peer_list)) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to load active programs\n", stderr);
#endif
        freeProg(&prog_list);
        freeDB(db_conn_data);
        return 0;
    }
    i1l.item = (int *) malloc(udp_buf_size * sizeof *(i1l.item));
    if (i1l.item == NULL) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to allocate memory for i1l\n", stderr);
#endif
        freeProg(&prog_list);
        freeDB(db_conn_data);
        return 0;
    }
    i1f1l.item = (I1F1 *) malloc(udp_buf_size * sizeof *(i1f1l.item));
    if (i1f1l.item == NULL) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to allocate memory for i1f1l\n", stderr);
#endif
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        freeDB(db_conn_data);
        return 0;
    }
    f1l.item = (float *) malloc(udp_buf_size * sizeof *(f1l.item));
    if (f1l.item == NULL) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to allocate memory for f1l\n", stderr);
#endif
        FREE_LIST(&i1f1l);
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        freeDB(db_conn_data);
        return 0;
    }
    if (!createThread_ctl()) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to create thread\n", stderr);
#endif
        FREE_LIST(&f1l);
        FREE_LIST(&i1f1l);
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        freeDB(db_conn_data);
        return 0;
    }
    return 1;
}

int addProg(Prog *item, ProgList *list) {
    if (list->length >= INT_MAX) {
#ifdef MODE_DEBUG
        fprintf(stderr, "addProg: ERROR: can not load prog with id=%d - list length exceeded\n", item->id);
#endif
        return 0;
    }
    if (list->top == NULL) {
        lockProgList();
        list->top = item;
        unlockProgList();
    } else {
        lockProg(list->last);
        list->last->next = item;
        unlockProg(list->last);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("addProg: prog with id=%d loaded\n", item->id);
#endif
    return 1;
}

int addProgById(int id, ProgList *list) {
#ifdef MODE_DEBUG
    printf("prog to add: %d\n", id);
#endif
    Prog *p = getProgByIdFdb(id, &peer_list);
    if (p == NULL) {
        return 0;
    }
    if (!addProg(p, list)) {
        free(p);
        return 0;
    }
    return 1;
}

int deleteProgById(int id, ProgList *list) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            saveProgActive(curr, 0);
            if (prev != NULL) {
                lockProg(prev);
                prev->next = curr->next;
                unlockProg(prev);
            } else {//curr=top
                lockProgList();
                list->top = curr->next;
                unlockProgList();
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            //we will wait other threads to finish curr program and then we will free it
            lockProg(curr);
            unlockProg(curr);
            free(curr);
#ifdef MODE_DEBUG
            printf("prog with id: %d deleted from prog_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    return done;
}

int reloadProgById(int id, ProgList *list) {
    if (deleteProgById(id, list)) {
        return 1;
    }
    return addProgById(id, list);
}

int sensorRead(SensorFTS *item) {
    return acp_readSensorFTS(item, udp_buf_size);
}

int controlEM(EM *item, float output) {
    if (item == NULL) {
        return 0;
    }
    return acp_setEMDutyCycleR(item, output, udp_buf_size);
}

int saveTune(int id, float kp, float ki, float kd) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update " APP_NAME_STR ".prog set kp=%0.3f, ki=%0.3f, kd=%0.3f where app_class='%s' and id=%d", kp, ki, kd, app_class, id);
    r = dbGetDataC(db_conn_data, q, "save pid tune: update prog: ");
    if (r == NULL) {
        return 0;
    }
    PQclear(r);
    return 1;
}

void serverRun(int *state, int init_state) {
    char buf_in[udp_buf_size];
    char buf_out[udp_buf_size];
    uint8_t crc;
    int i, j;
    char q[LINE_SIZE];
    crc = 0;
    memset(buf_in, 0, sizeof buf_in);
    acp_initBuf(buf_out, sizeof buf_out);
    if (recvfrom(udp_fd, buf_in, sizeof buf_in, 0, (struct sockaddr*) (&(peer_client.addr)), &(peer_client.addr_size)) < 0) {
#ifdef MODE_DEBUG
        perror("serverRun: recvfrom() error");
#endif
    }
#ifdef MODE_DEBUG
    dumpBuf(buf_in, sizeof buf_in);
#endif    
    if (!crc_check(buf_in, sizeof buf_in)) {
#ifdef MODE_DEBUG
        fputs("WARNING: serverRun: crc check failed\n", stderr);
#endif
        return;
    }
    switch (buf_in[1]) {
        case ACP_CMD_APP_START:
            if (!init_state) {
                *state = APP_INIT_DATA;
            }
            return;
        case ACP_CMD_APP_STOP:
            if (init_state) {
                waitThread_ctl(buf_in[1]);
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            waitThread_ctl(buf_in[1]);
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            waitThread_ctl(buf_in[1]);
            *state = APP_EXIT;
            return;
        case ACP_CMD_APP_PING:
            if (init_state) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_BUSY);
            } else {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_IDLE);
            }
            return;
        case ACP_CMD_APP_PRINT:
            printAll(&prog_list, &peer_list);
            return;
        case ACP_CMD_APP_HELP:
            printHelp();
            return;
        default:
            if (!init_state) {
                return;
            }
            break;
    }

    switch (buf_in[0]) {
        case ACP_QUANTIFIER_BROADCAST:
        case ACP_QUANTIFIER_SPECIFIC:
            break;
        default:
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_STOP:
        case ACP_CMD_START:
        case ACP_CMD_RESET:
        case ACP_CMD_REGSMP_PROG_SWITCH_STATE:
        case ACP_CMD_REGSMP_PROG_GET_DATA_RUNTIME:
        case ACP_CMD_REGSMP_PROG_GET_DATA_INIT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &i1l, udp_buf_size);
                    if (i1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        case ACP_CMD_REGSMP_PROG_SET_HEATER_POWER:
        case ACP_CMD_REGSMP_PROG_SET_COOLER_POWER:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    acp_parsePackF1(buf_in, &f1l, udp_buf_size);
                    if (f1l.length <= 0) {
                        return;
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1F1(buf_in, &i1f1l, udp_buf_size);
                    if (i1f1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        default:
            return;

    }

    switch (buf_in[1]) {
        case ACP_CMD_STOP:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    curr->state = OFF;
                    controlEM(&curr->em_h, 0.0f);
                    controlEM(&curr->em_c, 0.0f);
                    deleteProgById(curr->id, &prog_list);
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            curr->state = OFF;
                            controlEM(&curr->em_h, 0.0f);
                            controlEM(&curr->em_c, 0.0f);
                            deleteProgById(i1l.item[i], &prog_list);
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_START:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {

                    loadAllProg(&prog_list, &peer_list);

                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:

                    for (i = 0; i < i1l.length; i++) {
                        addProgById(i1l.item[i], &prog_list);
                    }

                    break;
            }
            return;
        case ACP_CMD_RESET:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {

                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    curr->state = OFF;
                    controlEM(&curr->em_h, 0.0f);
                    controlEM(&curr->em_c, 0.0f);
                    deleteProgById(curr->id, &prog_list);
                    PROG_LIST_LOOP_SP
                    loadAllProg(&prog_list, &peer_list);

                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:

                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            curr->state = OFF;
                            controlEM(&curr->em_h, 0.0f);
                            controlEM(&curr->em_c, 0.0f);
                            deleteProgById(i1l.item[i], &prog_list);
                        }
                    }
                    for (i = 0; i < i1l.length; i++) {
                        addProgById(i1l.item[i], &prog_list);
                    }

                    break;
            }
            return;
        case ACP_CMD_REGSMP_PROG_SWITCH_STATE:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        if (curr->state == REG) {
                            curr->state = NOREG;
                        } else if (curr->state == NOREG) {
                            curr->state = REG;
                        }
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                if (curr->state == REG) {
                                    curr->state = NOREG;
                                } else if (curr->state == NOREG) {
                                    curr->state = REG;
                                }
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_REGSMP_PROG_GET_DATA_RUNTIME:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        if (!bufCatProgRuntime(curr, buf_out, udp_buf_size)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                if (!bufCatProgRuntime(curr, buf_out, udp_buf_size)) {
                                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                    return;
                                }
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_REGSMP_PROG_GET_DATA_INIT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        if (!bufCatProgInit(curr, buf_out, udp_buf_size)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                if (!bufCatProgInit(curr, buf_out, udp_buf_size)) {
                                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                    return;
                                }
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_REGSMP_PROG_SET_HEATER_POWER:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        controlEM(&curr->em_h, f1l.item[0]);
                        curr->output_heater = f1l.item[0];
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1f1l.item[i].p0, &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                controlEM(&curr->em_h, i1f1l.item[i].p1);
                                curr->output_heater = i1f1l.item[i].p1;
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_REGSMP_PROG_SET_COOLER_POWER:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        controlEM(&curr->em_c, f1l.item[0]);
                        curr->output_cooler = f1l.item[0];
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1f1l.item[i].p0, &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                controlEM(&curr->em_c, i1f1l.item[i].p1);
                                curr->output_cooler = i1f1l.item[i].p1;
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_REGSMP_PROG_GET_DATA_RUNTIME:
        case ACP_CMD_REGSMP_PROG_GET_DATA_INIT:
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;

    }

}

void progControl(Prog *item) {
    switch (item->state) {
        case INIT:
            item->pid_h.reset = 1;
            item->pid_c.reset = 1;
            item->tmr.ready = 0;
            item->state_r = HEATER;
            controlEM(&item->em_h, 0.0f);
            controlEM(&item->em_c, 0.0f);
            item->output_heater = 0.0f;
            item->output_cooler = 0.0f;
            if (sensorRead(&item->sensor)) {
                if (SNSR_VAL > item->goal) {
                    item->state_r = COOLER;
                }
                item->state = REG;

            }
            break;
        case REG:
        {
            if (sensorRead(&item->sensor)) {
                int value_is_out = 0;
                char other_em;
                EM *em_turn_off;
                switch (item->state_r) {
                    case HEATER:
                        if (VAL_IS_OUT_H) {
                            value_is_out = 1;
                        }
                        other_em = COOLER;
                        em_turn_off = &item->em_h;
                        break;
                    case COOLER:
                        if (VAL_IS_OUT_C) {
                            value_is_out = 1;
                        }
                        other_em = HEATER;
                        em_turn_off = &item->em_c;
                        break;
                }
                if (value_is_out) {
                    if (ton_ts(item->change_gap, &item->tmr)) {
#ifdef MODE_DEBUG
                        char *state1 = getStateStr(item->state_r);
                        char *state2 = getStateStr(other_em);
                        printf("prog_id=%d: state_r switched from %s to %s\n", item->id, state1, state2);
                        //  printf("pid_mode=%c pid_ie=%.1f\n", item->pid.mode, item->pid.integral_error);
#endif
                        item->state_r = other_em;
                        controlEM(em_turn_off, 0.0f);
                        if (em_turn_off == &item->em_h) {
                            item->output_heater = 0.0f;
                        } else if (em_turn_off == &item->em_c) {
                            item->output_cooler = 0.0f;
                        }
                    }
                } else {
                    item->tmr.ready = 0;
                }
                PID *pidp = NULL;
                EM *em = NULL;
                EM *em_other = NULL;
                switch (item->state_r) {
                    case HEATER:
                        item->pid_h.max_output = item->em_h.pwm_rsl;
                        pidp = &item->pid_h;
                        em = &item->em_h;
                        em_other = &item->em_c;
                        break;
                    case COOLER:
                        item->pid_c.max_output = item->em_c.pwm_rsl;
                        pidp = &item->pid_c;
                        em = &item->em_c;
                        em_other = &item->em_h;
                        break;
                }
                switch (item->mode) {
                    case CNT_PID:
                        item->output = pidwt_mx(pidp, item->goal, SNSR_VAL, SNSR_TM);
                        break;
                    case CNT_ONF:
                        item->output = 0.0f;
                        switch (item->state_r) {
                            case HEATER:
                                if (SNSR_VAL < item->goal - item->delta) {
                                    item->output = em->pwm_rsl;
                                }
                                break;
                            case COOLER:
                                if (SNSR_VAL > item->goal + item->delta) {
                                    item->output = em->pwm_rsl;
                                }
                                break;
                        }
                        break;
                }
                controlEM(em, item->output);
                controlEM(em_other, 0.0f);
                if (em == &item->em_h) {
                    item->output_heater = item->output;
                } else if (em == &item->em_c) {
                    item->output_cooler = item->output;
                }
                if (em_other == &item->em_h) {
                    item->output_heater = 0.0f;
                } else if (em_other == &item->em_c) {
                    item->output_cooler = 0.0f;
                }
#ifdef MODE_DEBUG
                char *state_r = getStateStr(item->state_r);
                char *mode = getStateStr(item->mode);
                printf("prog_id=%d: mode=%s EM_state=%s goal=%.1f real=%.1f out=%.1f\n", item->id, mode, state_r, item->goal, SNSR_VAL, item->output);
#endif
            } else {
                controlEM(&item->em_h, 0.0f);
                controlEM(&item->em_c, 0.0f);
                item->output_heater = 0.0f;
                item->output_cooler = 0.0f;
#ifdef MODE_DEBUG
                puts("reading from sensor failed, EM turned off");
#endif
            }
            break;
        }
        case NOREG:
            sensorRead(&item->sensor);
            break;
        case OFF:
            break;

        default:
            item->state = INIT;
            break;
    }
}

void *threadFunction(void *arg) {
    char *cmd = (char *) arg;
#ifndef MODE_DEBUG
    puts("threadFunction: running...");
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();

        lockProgList();
        Prog *curr = prog_list.top;
        unlockProgList();
        while (1) {
            if (curr == NULL) {
                break;
            }
            if (tryLockProg(curr)) {
                progControl(curr);
                Prog *temp = curr;
                curr = curr->next;
                unlockProg(temp);
            }


            switch (*cmd) {
                case ACP_CMD_APP_STOP:
                case ACP_CMD_APP_RESET:
                case ACP_CMD_APP_EXIT:
                    *cmd = ACP_CMD_APP_NO;
                    return (EXIT_SUCCESS);
                default:
                    break;
            }
        }
        switch (*cmd) {
            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                *cmd = ACP_CMD_APP_NO;
                return (EXIT_SUCCESS);
            default:
                break;
        }
        sleepRest(cycle_duration, t1);
    }
}

int createThread_ctl() {
    if (pthread_create(&thread, NULL, &threadFunction, (void *) &thread_cmd) != 0) {
        perror("createThreads: pthread_create");
        return 0;
    }
    return 1;
}

void freeProg(ProgList * list) {
    Prog *curr = list->top, *temp;
    while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

void secure() {
    PROG_LIST_LOOP_DF
    PROG_LIST_LOOP_ST
    controlEM(&curr->em_h, 0.0f);
    controlEM(&curr->em_c, 0.0f);
    PROG_LIST_LOOP_SP
}

void freeData() {
    waitThread_ctl(ACP_CMD_APP_EXIT);
    secure();
    freeProg(&prog_list);
    FREE_LIST(&f1l);
    FREE_LIST(&i1f1l);
    FREE_LIST(&i1l);
    freeDB(db_conn_data);
#ifndef MODE_DEBUG
    puts("freeData: done");
#endif
}

void freeApp() {
    freeData();
    freePeer(&peer_list);
    freeSocketFd(&udp_fd);
    freeSocketFd(&udp_fd_tf);
    freeMutex(&progl_mutex);
    freePid(&pid_file, &proc_id, pid_path);
}

void exit_nicely() {
    freeApp();
    puts("\nBye...");
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
    int data_initialized = 0;
    while (1) {
        switch (app_state) {
            case APP_INIT:
#ifdef MODE_DEBUG
                puts("MAIN: init");
#endif
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
#ifdef MODE_DEBUG
                puts("MAIN: init data");
#endif
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
#ifdef MODE_DEBUG
                puts("MAIN: run");
#endif
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
#ifdef MODE_DEBUG
                puts("MAIN: stop");
#endif
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
#ifdef MODE_DEBUG
                puts("MAIN: reset");
#endif
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
#ifdef MODE_DEBUG
                puts("MAIN: exit");
#endif
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}