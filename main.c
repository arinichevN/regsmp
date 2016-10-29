/*
 * regsmp
 */


#include "main.h"

char pid_path[LINE_SIZE];
char app_class[NAME_SIZE];
char db_conninfo_settings[LINE_SIZE];

int app_state = APP_INIT;
PGconn *db_conn_settings = NULL;
PGconn *db_conn_public = NULL;
PGconn **db_connp_public = NULL;

PGconn *db_conn_data = NULL;
PGconn **db_connp_data = NULL; //pointer to db_conn_settings or to db_conn_data

int pid_file = -1;
int proc_id;
int udp_port = -1;
size_t udp_buf_size = 0;
int udp_fd = -1;
int udp_fd_tf = -1;
Peer peer_client = {.fd = &udp_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
int data_initialized = 0;
ThreadData thread_data = {.cmd = ACP_CMD_APP_NO, .on = 0, .created = 0, .attr_initialized = 0,
    .i1l =
    {NULL, 0},
    .f1l =
    {NULL, 0},
    .i1f1l =
    {NULL, 0},
    .s1l =
    {NULL, 0},
    .i1s1l =
    {NULL, 0}};
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};
PeerList peer_list = {NULL, 0};
EMList em_list = {NULL, 0};
SensorList sensor_list = {NULL, 0};
ProgList prog_list = {NULL, NULL, 0};

#include "util.c"

int readSettings() {
    PGresult *r;
    char q[LINE_SIZE];
    char db_conninfo_data[LINE_SIZE];
    char db_conninfo_public[LINE_SIZE];
    memset(pid_path, 0, sizeof pid_path);
    memset(db_conninfo_data, 0, sizeof db_conninfo_data);
    if (!initDB(&db_conn_settings, db_conninfo_settings)) {
        return 0;
    }
    snprintf(q, sizeof q, "select db_public from %s.config where app_class='%s'", APP_NAME, app_class);
    if ((r = dbGetDataT(db_conn_settings, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
        PQclear(r);
        fputs("readSettings: need only one tuple (1)\n", stderr);
        return 0;
    }


    memcpy(db_conninfo_public, PQgetvalue(r, 0, 0), LINE_SIZE);
    PQclear(r);
    if (dbConninfoEq(db_conninfo_public, db_conninfo_settings)) {
        db_connp_public = &db_conn_settings;
    } else {
        if (!initDB(&db_conn_public, db_conninfo_public)) {
            return 0;
        }
        db_connp_public = &db_conn_public;
    }

    udp_buf_size = 0;
    udp_port = -1;
    snprintf(q, sizeof q, "select udp_port, udp_buf_size, pid_path, db_data, cycle_duration_us from %s.config where app_class='%s'", APP_NAME, app_class);
    if ((r = dbGetDataT(db_conn_settings, q, q)) == NULL) {
        return 0;
    }
    if (PQntuples(r) == 1) {
        int done = 1;
        done = done && config_getUDPPort(*db_connp_public, PQgetvalue(r, 0, 0), &udp_port);
        done = done && config_getBufSize(*db_connp_public, PQgetvalue(r, 0, 1), &udp_buf_size);
        done = done && config_getPidPath(*db_connp_public, PQgetvalue(r, 0, 2), pid_path, LINE_SIZE);
        done = done && config_getDbConninfo(*db_connp_public, PQgetvalue(r, 0, 3), db_conninfo_data, LINE_SIZE);
        done = done && config_getCycleDurationUs(*db_connp_public, PQgetvalue(r, 0, 4), &cycle_duration);
        if (!done) {
            PQclear(r);
            freeDB(&db_conn_public);
            fputs("readSettings: failed to read some fields\n", stderr);
            return 0;
        }
    } else {
        PQclear(r);
        freeDB(&db_conn_public);
        fputs("readSettings: one tuple expected\n", stderr);
        return 0;
    }
    PQclear(r);


    if (dbConninfoEq(db_conninfo_data, db_conninfo_settings)) {
        db_connp_data = &db_conn_settings;
    } else if (dbConninfoEq(db_conninfo_data, db_conninfo_public)) {
        db_connp_data = &db_conn_public;
    } else {
        if (!initDB(&db_conn_data, db_conninfo_data)) {
            return 0;
        }
        db_connp_data = &db_conn_data;
    }
    if (!(dbConninfoEq(db_conninfo_data, db_conninfo_settings) || dbConninfoEq(db_conninfo_public, db_conninfo_settings))) {
        freeDB(&db_conn_settings);
    }
    return 1;
}

void initApp() {
    if (!readConf(CONF_FILE, db_conninfo_settings, app_class)) {
        exit_nicely_e("initApp: failed to read configuration file\n");
    }

    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }

    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }

    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize mutex\n");
    }

    if (!initUDPServer(&udp_fd, udp_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }

    if (!initUDPClient(&udp_fd_tf, WAIT_RESP_TIMEOUT)) {
        exit_nicely_e("initApp: failed to initialize udp client\n");
    }
}

void initData() {
    data_initialized = 0;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select peer_id from %s.em_mapping where app_class='%s' union distinct select peer_id from %s.sensor_mapping where app_class='%s'", APP_NAME, app_class, APP_NAME, app_class);
    if (!config_getPeerList(*db_connp_data, *db_connp_public, q, &peer_list, &udp_fd_tf)) {
        freePeer(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return;
    }
    if (!initSensor(&sensor_list, &peer_list)) {
        FREE_LIST(&sensor_list);
        freePeer(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return;
    }
    if (!initEM(&em_list, &peer_list)) {
        FREE_LIST(&em_list);
        FREE_LIST(&sensor_list);
        freePeer(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return;
    }
    if (!createThread(&thread_data, udp_buf_size)) {
        freeProg(&prog_list);
        FREE_LIST(&em_list);
        FREE_LIST(&sensor_list);
        freePeer(&peer_list);
        freeDB(&db_conn_data);
        freeDB(&db_conn_public);
        return;
    }
    data_initialized = 1;
}

int initSensor(SensorList *list, const PeerList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    list->length = 0;
    list->item = NULL;
    snprintf(q, sizeof q, "select sensor_id, remote_id, peer_id from %s.sensor_mapping where app_class='%s'", APP_NAME, app_class);
    if ((r = dbGetDataT(*db_connp_data, q, "initSensor: select pin: ")) == NULL) {
        return 0;
    }
    list->length = PQntuples(r);
    if (list->length > 0) {
        list->item = (Sensor *) malloc(list->length * sizeof *(list->item));
        if (list->item == NULL) {
            list->length = 0;
            fputs("initSensor: failed to allocate memory\n", stderr);
            PQclear(r);
            return 0;
        }
        for (i = 0; i < list->length; i++) {
            list->item[i].id = atoi(PQgetvalue(r, i, 0));
            list->item[i].remote_id = atoi(PQgetvalue(r, i, 1));
            list->item[i].source = getPeerById(PQgetvalue(r, i, 2), pl);
        }
    }
    PQclear(r);
    if (!checkSensor(list)) {
        return 0;
    }
    return 1;
}

int initEM(EMList *list, const PeerList *pl) {
    PGresult *r;
    char q[LINE_SIZE];
    size_t i;
    list->length = 0;
    list->item = NULL;
    snprintf(q, sizeof q, "select em_id, remote_id, peer_id from %s.em_mapping where app_class='%s'", APP_NAME, app_class);
    if ((r = dbGetDataT(*db_connp_data, q, "initEm: select pin: ")) == NULL) {
        return 0;
    }
    list->length = PQntuples(r);
    if (list->length > 0) {
        list->item = (EM *) malloc(list->length * sizeof *(list->item));
        if (list->item == NULL) {
            list->length = 0;
            fputs("initEm: failed to allocate memory\n", stderr);
            PQclear(r);
            return 0;
        }
        for (i = 0; i < list->length; i++) {
            list->item[i].id = atoi(PQgetvalue(r, i, 0));
            list->item[i].remote_id = atoi(PQgetvalue(r, i, 1));
            list->item[i].source = getPeerById(PQgetvalue(r, i, 2), &peer_list);
            list->item[i].last_output = 0.0f;
        }
    }
    PQclear(r);
    if (!checkEM(list)) {
        return 0;
    }
    return 1;
}

Prog * getProgByIdFdb(int id, const SensorList *slist, EMList *elist) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select sensor_id, em_id, goal, mode, kp, ki, kd from %s.prog where app_class='%s' and id=%d", APP_NAME, app_class, id);
    if ((r = dbGetDataT(*db_connp_data, q, "initProg: select pin: ")) == NULL) {
        return 0;
    }
    if (PQntuples(r) != 1) {
#ifdef MODE_DEBUG
        fputs("getProgByIdFdb: only one tuple expected\n", stderr);
#endif
        PQclear(r);
        return NULL;
    }

    Prog *item = (Prog *) malloc(sizeof *(item));
    if (item == NULL) {
#ifdef MODE_DEBUG
        fputs("getProgByIdFdb: failed to allocate memory\n", stderr);
#endif
        PQclear(r);
        return NULL;
    }

    item->sensor = getSensorById(atoi(PQgetvalue(r, 0, 0)), slist);
    item->em = getEMById(atoi(PQgetvalue(r, 0, 1)), elist);
    item->goal = atof(PQgetvalue(r, 0, 2));
    memcpy(&item->pid.mode, PQgetvalue(r, 0, 3), sizeof item->pid.mode);
    item->pid.kp = atof(PQgetvalue(r, 0, 4));
    item->pid.ki = atof(PQgetvalue(r, 0, 5));
    item->pid.kd = atof(PQgetvalue(r, 0, 6));
    PQclear(r);

    item->id = id;
    item->state = INIT;
    item->next = NULL;
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

int addProg(Prog *item, ProgList *list) {
    puts("addProg");
    if (list->length >= INT_MAX) {
        return 0;
    }
    if (list->top == NULL) {
        list->top = item;
    } else {
        list->last->next = item;
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("prog with id=%d loaded\n", item->id);
#endif
    return 1;
}

int addProgById(int id, ProgList *list) {
    printf("prog to add: %d\n", id);
    Prog *p = getProgByIdFdb(id, &sensor_list, &em_list);
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
    printf("prog to delete: %d\n", id);
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            if (prev != NULL) {
                prev->next = curr->next;
            } else {//curr=top
                list->top = curr->next;
            }
            if (curr == list->last) {
                list->last = prev;
            }
            free(curr);
            list->length--;
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

int switchProgById(int id, ProgList *list) {
    if (deleteProgById(id, list)) {
        return 1;
    }
    return addProgById(id, list);
}

void loadAllProg(ProgList *list, const SensorList *slist, EMList *elist) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select sensor_id, em_id, goal, mode, kp, ki, kd, id from %s.prog where app_class='%s'", APP_NAME, app_class);
    if ((r = dbGetDataT(*db_connp_data, q, "loadAllProg: select pin: ")) == NULL) {
        return;
    }
    int n = PQntuples(r);
    int i;
    for (i = 0; i < n; i++) {
        Prog *item = (Prog *) malloc(sizeof *(item));
        if (item == NULL) {
#ifdef MODE_DEBUG
            fputs("getProgByIdFdb: failed to allocate memory\n", stderr);
#endif
            break;
        }
        item->sensor = getSensorById(atoi(PQgetvalue(r, i, 0)), slist);
        item->em = getEMById(atoi(PQgetvalue(r, i, 1)), elist);
        item->goal = atof(PQgetvalue(r, i, 2));
        memcpy(&item->pid.mode, PQgetvalue(r, i, 3), sizeof item->pid.mode);
        item->pid.kp = atof(PQgetvalue(r, i, 4));
        item->pid.ki = atof(PQgetvalue(r, i, 5));
        item->pid.kd = atof(PQgetvalue(r, i, 6));
        item->id = atoi(PQgetvalue(r, i, 7));
        item->state = INIT;
        item->next = NULL;
        if (!initMutex(&item->mutex)) {
            free(item);
            break;
        }
        if (!checkProg(item, list)) {
            free(item);
            break;
        }
        if (!addProg(item, list)) {
            free(item);
            break;
        }
    }
    PQclear(r);
}

int checkSensor(const SensorList *list) {
    size_t i, j;
    for (i = 0; i < list->length; i++) {
        if (list->item[i].source == NULL) {
            fprintf(stderr, "checkSensorList: no data source where id = %d\n", list->item[i].id);
            return 0;
        }
    }
    //unique id
    for (i = 0; i < list->length; i++) {
        for (j = i + 1; j < list->length; j++) {
            if (list->item[i].id == list->item[j].id) {
                fprintf(stderr, "checkSensorList: id is not unique where id = %d\n", list->item[i].id);
                return 0;
            }
        }
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
    if (item->sensor == NULL) {
        fprintf(stderr, "checkProg: no sensor attached to prog with id = %d\n", item->id);
        return 0;
    }
    if (item->em == NULL) {
        fprintf(stderr, "checkProg: no EM attached to prog with id = %d\n", item->id);
        return 0;
    }
    switch (item->pid.mode) {
        case PID_MODE_HEATER:
        case PID_MODE_COOLER:
            break;
        default:
            fprintf(stderr, "checkProg: bad mode for prog with id = %d\n", item->id);
            return 0;
    }
    //unique id
    if (getProgById(item->id, list) != NULL) {
        fprintf(stderr, "checkProg: prog with id = %d is already running\n", item->id);
        return 0;
    }
    return 1;
}

int sensorRead(Sensor *s) {
    if (s == NULL) {
        return 0;
    }
    int di[1];
    di[0] = s->remote_id;
    I1List data = {di, 1};

    if (!acp_sendBufArrPackI1List(ACP_CMD_GET_FTS, udp_buf_size, &data, s->source)) {
#ifdef MODE_DEBUG
        fputs("sensorRead: acp_sendBufArrPackI1List failed\n", stderr);
#endif
        s->value.state = 0;
        return 0;
    }
    //waiting for response...
    FTS td[1];
    FTSList tl = {td, 0};

    if (!acp_recvFTS(&tl, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED, udp_buf_size, 1, *(s->source->fd))) {
#ifdef MODE_DEBUG
        fputs("sensorRead: acp_recvFTS() error\n", stderr);
#endif
        s->value.state = 0;
        return 0;
    }
    if (tl.item[0].state == 1 && tl.item[0].id == s->id) {
        s->value = tl.item[0];
        return 1;
    } else {
#ifdef MODE_DEBUG
        fputs("sensorRead: response: temperature sensor state is bad or id is not requested one\n", stderr);
#endif
        s->value.state = 0;

        return 0;
    }
    return 1;
}

int controlEM(EM *em, float output) {
    if (em == NULL) {
        return 0;
    }
    if (output == em->last_output) {
        return 0;
    }
    I2 di[1];
    di[0].p0 = em->remote_id;
    di[0].p1 = (int) output;
    I2List data = {di, 1};
    if (!acp_sendBufArrPackI2List(ACP_CMD_SET_DUTY_CYCLE_PWM, udp_buf_size, &data, em->source)) {
#ifdef MODE_DEBUG
        fputs("controlEM: failed to send request\n", stderr);
#endif
        return 0;
    }
    em->last_output = output;
    return 1;
}

int saveTune(int id, float kp, float ki, float kd) {
    PGresult *r;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update %s.prog set kp=%0.3f, ki=%0.3f, kd=%0.3f where app_class='%s' and id=%d", APP_NAME, kp, ki, kd, app_class, id);
    r = dbGetDataC(*db_connp_data, q, "save pid tune: update prog: ");
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
        fputs("serverRun: crc check failed\n", stderr);
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
                if (thread_data.on) {
                    waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
                }
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
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
            printAll(&prog_list, &peer_list, &em_list, &sensor_list);
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
        case ACP_CMD_REGSMP_PROG_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &thread_data.i1l, udp_buf_size);
                    if (thread_data.i1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        case ACP_CMD_REGSMP_PROG_SET_STATE://reg or tune
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    acp_parsePackS1(buf_in, &thread_data.s1l, udp_buf_size);
                    if (thread_data.s1l.length <= 0) {
                        return;
                    }
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1S1(buf_in, &thread_data.i1s1l, udp_buf_size);
                    if (thread_data.i1s1l.length <= 0) {
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
        case ACP_CMD_START:
        case ACP_CMD_REGSMP_PROG_SET_STATE:
            if (thread_data.on) {
                waitThreadCmd(&thread_data.cmd, &thread_data.qfr, buf_in);
            }
            break;
        case ACP_CMD_REGSMP_PROG_GET_DATA:
            break;
    }

    switch (buf_in[1]) {
        case ACP_CMD_STOP:
        case ACP_CMD_START:
        case ACP_CMD_REGSMP_PROG_SET_STATE:
            return;
        case ACP_CMD_REGSMP_PROG_GET_DATA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        if (!bufCatProg(curr, buf_out, udp_buf_size)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < thread_data.i1l.length; i++) {
                        Prog *curr = getProgById(thread_data.i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                if (!bufCatProg(curr, buf_out, udp_buf_size)) {
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
    }

    switch (buf_in[1]) {
        case ACP_CMD_REGSMP_PROG_GET_DATA:
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;

    }

}

void secure() {
    size_t i;
    for (i = 0; i < em_list.length; i++) {
        controlEM(&em_list.item[i], 0.0f);
    }
}

void progControl(Prog *item) {
    switch (item->state) {
        case INIT:
            item->pid.reset = 1;
            item->state = REG;
            break;
        case REG:
            if (sensorRead(item->sensor)) {
                item->output = pid(&item->pid, item->goal, item->sensor->value.temp);
                //item->output = pidwt(&item->pid, item->goal, item->sensor->value.temp, item->sensor->value.tm);
#ifdef MODE_DEBUG
                printf("prog_id=%d: state=REG goal=%.1f real=%.1f out=%.1f\n", item->id, item->goal, item->sensor->value.temp, item->output);
                //  printf("pid_mode=%c pid_ie=%.1f\n", item->pid.mode, item->pid.integral_error);
#endif
                controlEM(item->em, item->output);
            }
            break;
        case TUNE:
            if (sensorRead(item->sensor)) {
                if (pidAutoTune(&item->pid_at, &item->pid, item->sensor->value.temp, &item->output)) {
                    saveTune(item->id, item->pid.kp, item->pid.ki, item->pid.kd);
                    item->state = REG;
                }
#ifdef MODE_DEBUG
                printf("prog_id=%d: state=TUNE goal=%.1f real=%.1f out=%.1f\n", item->id, item->goal, item->sensor->value.temp, item->output);
                // printf("pid_mode=%c pid_ie=%.1f\n", item->pid.mode, item->pid.integral_error);
#endif
                controlEM(item->em, item->output);
            }
            break;
        case OFF:
            break;
        default:
            item->state = INIT;
            break;
    }
}

void *threadFunction(void *arg) {
    ThreadData *data = (ThreadData *) arg;
    data->on = 1;
    int r;
    if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &r) != 0) {
        perror("threadFunction: pthread_setcancelstate");
    }
    if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &r) != 0) {
        perror("threadFunction: pthread_setcanceltype");
    }

    while (1) {
        size_t i;
        struct timespec t1;
        clock_gettime(LIB_CLOCK, &t1);
        PROG_LIST_LOOP_ST
        if (tryLockProg(curr)) {
            progControl(curr);
            unlockProg(curr);
        }
        PROG_LIST_LOOP_SP

        switch (data->cmd) {
            case ACP_CMD_STOP:
            case ACP_CMD_START:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                    {
                        switch (data->cmd) {
                            case ACP_CMD_STOP:
                            {
                                PROG_LIST_LOOP_ST
                                controlEM(curr->em, 0.0f);
                                PROG_LIST_LOOP_SP
                                freeProg(&prog_list);
                                break;
                            }
                            case ACP_CMD_START:
                                loadAllProg(&prog_list, &sensor_list, &em_list);
                                break;
                        }

                        break;
                    }
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i1l.length; i++) {
                            data->i1l.item[i];
                            switch (data->cmd) {
                                case ACP_CMD_STOP:
                                {
                                    Prog *p = getProgById(data->i1l.item[i], &prog_list);
                                    if (p != NULL) {
                                        controlEM(p->em, 0.0f);
                                    }
                                    deleteProgById(data->i1l.item[i], &prog_list);
                                    break;
                                }
                                case ACP_CMD_START:
                                    addProgById(data->i1l.item[i], &prog_list);
                                    break;
                            }

                        }
                        break;
                }
                break;
            case ACP_CMD_REGSMP_PROG_SET_STATE:
                switch (data->qfr) {
                    case ACP_QUANTIFIER_BROADCAST:
                    {
                        PROG_LIST_LOOP_ST
                        if (strcmp(data->s1l.item, STATE_STR_REG) == 0) {
                            curr->state = REG;
                        } else if (strcmp(data->s1l.item, STATE_STR_TUNE) == 0) {
                            curr->state = TUNE;
                        }
                        PROG_LIST_LOOP_SP
                        break;
                    }
                    case ACP_QUANTIFIER_SPECIFIC:
                        for (i = 0; i < data->i1s1l.length; i++) {
                            Prog *p = getProgById(data->i1l.item[i], &prog_list);
                            if (p != NULL) {
                                if (strcmp(data->i1s1l.item[i].p1, STATE_STR_REG) == 0) {
                                    p->state = REG;
                                } else if (strcmp(data->i1s1l.item[i].p1, STATE_STR_TUNE) == 0) {
                                    p->state = TUNE;
                                }
                            }
                        }
                        break;
                }
                break;
            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                secure();
                data->cmd = ACP_CMD_APP_NO;
                data->on = 0;
                return (EXIT_SUCCESS);

            default:
                break;
        }
        data->cmd = ACP_CMD_APP_NO; //notify main thread that command has been executed
        sleepRest(data->cycle_duration, t1);
    }
}

int createThread(ThreadData * td, size_t buf_len) {
    //set attributes for each thread
    if (pthread_attr_init(&td->thread_attr) != 0) {
        perror("createThreads: pthread_attr_init");
        return 0;
    }
    td->attr_initialized = 1;
    td->cycle_duration = cycle_duration;
    if (pthread_attr_setdetachstate(&td->thread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("createThreads: pthread_attr_setdetachstate");
        return 0;
    }
    td->i1l.item = (int *) malloc(buf_len * sizeof *(td->i1l.item));
    if (td->i1l.item == NULL) {
        perror("createThreads: memory allocation for i1l failed");
        return 0;
    }
    td->f1l.item = (float *) malloc(buf_len * sizeof *(td->f1l.item));
    if (td->f1l.item == NULL) {
        perror("createThreads: memory allocation for f1l failed");
        return 0;
    }
    td->i1f1l.item = (I1F1 *) malloc(buf_len * sizeof *(td->i1f1l.item));
    if (td->i1f1l.item == NULL) {
        perror("createThreads: memory allocation for i1f1l failed");
        return 0;
    }
    td->s1l.item = (char *) malloc(buf_len * NAME_SIZE * sizeof *(td->s1l.item));
    if (td->s1l.item == NULL) {
        perror("createThreads: memory allocation for s1l failed");
        return 0;
    }
    td->i1s1l.item = (I1S1 *) malloc(buf_len * sizeof *(td->i1s1l.item));
    if (td->i1s1l.item == NULL) {
        perror("createThreads: memory allocation for i1s1l failed");
        return 0;
    }
    //create a thread
    if (pthread_create(&td->thread, &td->thread_attr, threadFunction, (void *) td) != 0) {
        perror("createThreads: pthread_create");
        return 0;
    }
    td->created = 1;

    return 1;
}

void freeProg(ProgList *list) {
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

void freeThread() {
    if (thread_data.created) {
        if (thread_data.on) {
            char cmd[2] = {ACP_QUANTIFIER_BROADCAST, ACP_CMD_APP_EXIT};
            waitThreadCmd(&thread_data.cmd, &thread_data.qfr, cmd);
        }
        if (pthread_attr_destroy(&thread_data.thread_attr) != 0) {
            perror("freeThread: pthread_attr_destroy");
        }
    } else {
        if (thread_data.attr_initialized) {
            if (pthread_attr_destroy(&thread_data.thread_attr) != 0) {

                perror("freeThread: pthread_attr_destroy");
            }
        }
    }
    free(thread_data.i1l.item);
    thread_data.i1l.item = NULL;
    free(thread_data.f1l.item);
    thread_data.f1l.item = NULL;
    free(thread_data.i1f1l.item);
    thread_data.i1f1l.item = NULL;
    free(thread_data.s1l.item);
    thread_data.s1l.item = NULL;
    free(thread_data.i1s1l.item);
    thread_data.i1s1l.item = NULL;
    thread_data.on = 0;
    thread_data.cmd = ACP_CMD_APP_NO;
}

void freeData() {
    freeThread();
    freeProg(&prog_list);
    FREE_LIST(&em_list);
    FREE_LIST(&sensor_list);
    freePeer(&peer_list);
    freeDB(&db_conn_data);
    freeDB(&db_conn_public);
    data_initialized = 0;
}

void freeApp() {
    freeData();
    freeSocketFd(&udp_fd);
    freeSocketFd(&udp_fd_tf);
    freeMutex(&progl_mutex);
    freePid(&pid_file, &proc_id, pid_path);
    freeDB(&db_conn_settings);
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
    while (1) {
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                initData();
                app_state = APP_RUN;
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                app_state = APP_INIT;
                break;
            case APP_EXIT:
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