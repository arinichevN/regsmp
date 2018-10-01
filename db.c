
#include "main.h"

int getProg_callback(void *d, int argc, char **argv, char **azColName) {
    ChannelData * data = d;
    Channel *item = data->channel;
    int load = 0, enable = 0;

    int c = 0;
    DB_FOREACH_COLUMN {
        if (DB_COLUMN_IS("id")) {
            item->id = DB_CVI;
            c++;
        } else if (DB_COLUMN_IS("sensor_id")) {
            SensorFTS *sensor;
            LIST_GETBYID(sensor, data->sensor_list, DB_CVI)
            if (sensor == NULL) {
                return EXIT_FAILURE;
            }
            item->prog.sensor = *sensor;
            c++;
        } else if (DB_COLUMN_IS("heater_em_id")) {
            EM *em;
            LIST_GETBYID(em, data->em_list, DB_CVI)
            if (em == NULL) {
                return EXIT_FAILURE;
            }
            item->prog.heater.em = *em;
            c++;
        } else if (DB_COLUMN_IS("cooler_em_id")) {
            EM *em;
            LIST_GETBYID(em, data->em_list, DB_CVI)
            if (em == NULL) {
                return EXIT_FAILURE;
            }
            item->prog.cooler.em = *em;
            c++;
        } else if (DB_COLUMN_IS("em_mode")) {
            if (strcmp(REG_EM_MODE_COOLER_STR, DB_COLUMN_VALUE) == 0) {
                item->prog.cooler.use = 1;
                item->prog.heater.use = 0;
            } else if (strcmp(REG_EM_MODE_HEATER_STR, DB_COLUMN_VALUE) == 0) {
                item->prog.cooler.use = 0;
                item->prog.heater.use = 1;
            } else if (strcmp(REG_EM_MODE_BOTH_STR, DB_COLUMN_VALUE) == 0) {
                item->prog.cooler.use = 1;
                item->prog.heater.use = 1;
            } else {
                item->prog.cooler.use = 0;
                item->prog.heater.use = 0;
            }
            c++;
        } else if (DB_COLUMN_IS("heater_mode")) {
            if (strncmp(DB_COLUMN_VALUE, REG_MODE_PID_STR, 3) == 0) {
                item->prog.heater.mode = REG_MODE_PID;
            } else if (strncmp(DB_COLUMN_VALUE, REG_MODE_ONF_STR, 3) == 0) {
                item->prog.heater.mode = REG_MODE_ONF;
            } else {
                item->prog.heater.mode = REG_OFF;
            }
            c++;
        } else if (DB_COLUMN_IS("cooler_mode")) {
            if (strncmp(DB_COLUMN_VALUE, REG_MODE_PID_STR, 3) == 0) {
                item->prog.cooler.mode = REG_MODE_PID;
            } else if (strncmp(DB_COLUMN_VALUE, REG_MODE_ONF_STR, 3) == 0) {
                item->prog.cooler.mode = REG_MODE_ONF;
            } else {
                item->prog.cooler.mode = REG_OFF;
            }
            c++;
        } else if (DB_COLUMN_IS("goal")) {
            item->prog.goal = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("heater_delta")) {
            item->prog.heater.delta = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("heater_kp")) {
            item->prog.heater.pid.kp = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("heater_ki")) {
            item->prog.heater.pid.ki = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("heater_kd")) {
            item->prog.heater.pid.kd = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("cooler_kp")) {
            item->prog.cooler.pid.kp = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("cooler_ki")) {
            item->prog.cooler.pid.ki = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("cooler_kd")) {
            item->prog.cooler.pid.kd = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("cooler_delta")) {
            item->prog.cooler.delta = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("change_gap")) {
            item->prog.change_gap.tv_nsec = 0;
            item->prog.change_gap.tv_sec = DB_CVI;
            c++;
        } else if (DB_COLUMN_IS("secure_id")) {
            if (!reg_getSecureFDB(&item->prog.secure_out, DB_CVI, data->db_data, NULL)) {
                item->prog.secure_out.active = 0;
            }
            c++;
        } else if (DB_COLUMN_IS("green_light_sensor_id")) {
            SensorFTS *sensor;
            LIST_GETBYID(sensor, data->sensor_list, DB_CVI)
            if (sensor == NULL) {
                item->prog.green_light.active = 0;
            } else {
                item->prog.green_light.sensor = *sensor;
                item->prog.green_light.active = 1;
            }
            c++;
        } else if (DB_COLUMN_IS("green_value")) {
            item->prog.green_light.green_value = DB_CVF;
            c++;
        } else if (DB_COLUMN_IS("save")) {
            item->save = DB_CVI;
            c++;
        } else if (DB_COLUMN_IS("enable")) {
            enable = DB_CVI;
            c++;
        } else if (DB_COLUMN_IS("load")) {
            load = DB_CVI;
            c++;
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column (we will skip it): %s\n", F, DB_COLUMN_NAME);
#endif
        }
    }
#define N 23
    if (c != N) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): required %d columns but %d found\n", F, N, c);
#endif
        return EXIT_FAILURE;
    }
#undef N
    if (enable) {
        regpidonfhc_enable(&item->prog);
    } else {
        regpidonfhc_disable(&item->prog);
    }
    if (!load) {
        db_saveTableFieldInt("prog", "load", item->id, 1, data->db_data, NULL);
    }
    return EXIT_SUCCESS;
}

int getProgByIdFDB(int prog_id, Channel *item, EMList *em_list, SensorFTSList *sensor_list, sqlite3 *dbl, const char *db_path) {
    if (dbl != NULL && db_path != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): dbl xor db_path expected\n", F);
#endif
        return 0;
    }
    sqlite3 *db;
    int close = 0;
    if (db_path != NULL) {
        if (!db_open(db_path, &db)) {
            return 0;
        }
        close = 1;
    } else {
        db = dbl;
    }
    char q[LINE_SIZE];
    ChannelData data = {.em_list = em_list, .sensor_list = sensor_list, .channel = item, .db_data = db};
    snprintf(q, sizeof q, "select * from prog where id=%d", prog_id);
    if (!db_exec(db, q, getProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): query failed\n", F);
#endif
        if (close)sqlite3_close(db);
        return 0;
    }
    if (close)sqlite3_close(db);
    return 1;
}

int addChannel(Channel *item, ChannelList *list, Mutex *list_mutex) {
    if (list->length >= INT_MAX) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): can not load prog with id=%d - list length exceeded\n", F, item->id);
#endif
        return 0;
    }
    if (list->top == NULL) {
        lockMutex(list_mutex);
        list->top = item;
        unlockMutex(list_mutex);
    } else {
        lockMutex(&list->last->mutex);
        list->last->next = item;
        unlockMutex(&list->last->mutex);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("%s(): prog with id=%d loaded\n", F, item->id);
#endif
    return 1;
}

int addChannelById(int prog_id, ChannelList *list, EMList *em_list, SensorFTSList *sensor_list, sqlite3 *db_data, const char *db_data_path, Mutex *list_mutex) {
    extern struct timespec cycle_duration;
    Channel *rprog;
    LLIST_GETBYID(rprog,list,prog_id)
    if (rprog != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): program with id = %d is being controlled by program\n", F, rprog->id);
#endif
        return 0;
    }

    Channel *item = malloc(sizeof *(item));
    if (item == NULL) {
        fprintf(stderr, "%s(): failed to allocate memory\n", F);
        return 0;
    }
    memset(item, 0, sizeof *item);
    item->id = prog_id;
    item->next = NULL;
    item->cycle_duration = cycle_duration;
    if (!initMutex(&item->mutex)) {
        free(item);
        return 0;
    }
    if (!initClient(&item->sock_fd, WAIT_RESP_TIMEOUT)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!getProgByIdFDB(item->id, item, em_list, sensor_list, db_data, db_data_path)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    item->prog.sensor.peer.fd = &item->sock_fd;
    item->prog.green_light.sensor.peer.fd = &item->sock_fd;
    item->prog.heater.em.peer.fd = &item->sock_fd;
    item->prog.cooler.em.peer.fd = &item->sock_fd;
    item->prog.secure_out.error_code=&item->error_code;
    if (!checkProg(item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!addChannel(item, list, list_mutex)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!createMThread(&item->thread, &threadFunction, item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    return 1;
}

int deleteChannelById(int id, ChannelList *list, const char* db_path, Mutex *list_mutex) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Channel *prev = NULL;
    int done = 0;
    FOREACH_LLIST(curr,list,Channel) {
        if (curr->id == id) {
            if (prev != NULL) {
                lockMutex(&prev->mutex);
                prev->next = curr->next;
                unlockMutex(&prev->mutex);
            } else {//curr=top
                lockMutex(list_mutex);
                list->top = curr->next;
                unlockMutex(list_mutex);
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            STOP_CHANNEL_THREAD(curr);
            db_saveTableFieldInt("prog", "load", curr->id, 0, NULL, db_data_path);
            freeChannel(curr);
#ifdef MODE_DEBUG
            printf("channel with id: %d has been deleted from channel_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
    }

    return done;
}

int loadActiveProg_callback(void *d, int argc, char **argv, char **azColName) {
    ChannelData *data = d;
    DB_FOREACH_COLUMN {
        if (DB_COLUMN_IS("id")) {
            int id = DB_CVI;
            addChannelById(id, data->channel_list, data->em_list, data->sensor_list, data->db_data, NULL,data->channel_list_mutex);
        } else {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): unknown column (we will skip it): %s\n", F, DB_COLUMN_NAME);
#endif
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveProg(ChannelList *list, EMList *em_list, SensorFTSList *sensor_list, char *db_path, Mutex *list_mutex) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ChannelData data = {.channel_list = list, .em_list = em_list, .sensor_list = sensor_list, .db_data = db, .channel_list_mutex=list_mutex};
    char *q = "select id from prog where load=1";
    if (!db_exec(db, q, loadActiveProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): query failed\n", F);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}
