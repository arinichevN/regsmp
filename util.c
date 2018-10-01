
#include "main.h"


extern int getProgByIdFDB(int prog_id, Channel *item, EMList *em_list, SensorFTSList *sensor_list, sqlite3 *dbl, const char *db_path);

void freeChannel(Channel*item) {
    freeSocketFd(&item->sock_fd);
    freeMutex(&item->mutex);
    free(item);
}

void freeChannelList(ChannelList *list) {
    Channel *item = list->top, *temp;
    while (item != NULL) {
        temp = item;
        item = item->next;
        freeChannel(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

int checkProg(const Channel *item) {
    return 1;
}


void secure() {
    FOREACH_CHANNEL{
    regpidonfhc_turnOff(&item->prog);
}
}

struct timespec getTimeRestChange(const Channel *item) {
    return getTimeRestTmr(item->prog.change_gap, item->prog.tmr);
}

int bufCatProgRuntime(Channel *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        char *state = reg_getStateStr(item->prog.state);
        char *state_r = reg_getStateStr(item->prog.state_r);
        struct timespec tm_rest = getTimeRestChange(item);
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
                item->id,
                state,
                state_r,
                item->prog.heater.output,
                item->prog.cooler.output,
                tm_rest.tv_sec,
                item->prog.sensor.value.value,
                item->prog.sensor.value.state
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgInit(Channel *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        RegPIDOnfHC * data=(RegPIDOnfHC *) &item->prog;
        char *heater_mode = reg_getStateStr(data->heater.mode);
        char *cooler_mode = reg_getStateStr(data->cooler.mode);
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_ROW_STR,
                item->id,
                data->goal,
                data->change_gap.tv_sec,
                heater_mode,
                data->heater.use,
                data->heater.em.pwm_rsl,
                data->heater.delta,
                data->heater.pid.kp,
                data->heater.pid.ki,
                data->heater.pid.kd,
                cooler_mode,
                data->cooler.use,
                data->cooler.em.pwm_rsl,
                data->cooler.delta,
                data->cooler.pid.kp,
                data->cooler.pid.ki,
                data->cooler.pid.kd
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgError(Channel *item, ACPResponse *response) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%u" ACP_DELIMITER_ROW_STR, item->id, item->error_code);
    return acp_responseStrCat(response, q);
}

int bufCatProgGoal(Channel *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
         RegPIDOnfHC * data=(RegPIDOnfHC *) &item->prog;
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR FLOAT_NUM ACP_DELIMITER_ROW_STR,
                item->id,
                data->goal
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgFTS(Channel *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        RegPIDOnfHC * data=(RegPIDOnfHC *) &item->prog;
        int r = acp_responseFTSCat(item->id, data->sensor.value.value, data->sensor.value.tm, data->sensor.value.state, response);
        unlockMutex(&item->mutex);
        return r;
    }
    return 0;
}

int bufCatProgEnabled(Channel *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        RegPIDOnfHC * data=(RegPIDOnfHC *) &item->prog;
        int enabled = regpidonfhc_getEnabled(data);
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
                item->id,
                enabled
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

void printData(ACPResponse *response) {
    ChannelList *list = &channel_list;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    SEND_STR(q)
    snprintf(q, sizeof q, "db_data_path: %s\n", db_data_path);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());
    SEND_STR(q)
    snprintf(q, sizeof q, "prog_list length: %d\n", list->length);
    SEND_STR(q)
    SEND_STR("+-----------------------------------------------------------------------------------------------------------------------------------------------+\n")
    SEND_STR("|                                                                 Program                                                                       |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|    id     |    goal   |  delta_h  |  delta_c  | change_gap|change_rest|   state   |  state_r  | state_onf | out_heater| out_cooler| error_code|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")

    FOREACH_CHANNEL{
            char *state = reg_getStateStr(item->prog.state);
    char *state_r = reg_getStateStr(item->prog.state_r);
    char *state_onf = reg_getStateStr(item->prog.state_onf);
    struct timespec tm1 = getTimeRestChange(item);
    snprintf(q, sizeof q, "|%11d|%11.3f|%11.3f|%11.3f|%11ld|%11ld|%11s|%11s|%11s|%11.3f|%11.3f|%11u|\n",
            item->id,
            item->prog.goal,
            item->prog.heater.delta,
            item->prog.cooler.delta,
            item->prog.change_gap.tv_sec,
            tm1.tv_sec,
            state,
            state_r,
            state_onf,
            item->prog.heater.output,
            item->prog.cooler.output,
            item->error_code
            );
    SEND_STR(q)
}
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------------------------------------------------------------------+\n")
    SEND_STR("|                               Prog secure                                         |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|  prog_id  | secure_id | timeout_s |heater_out |cooler_out |  active   |   done    |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    FOREACH_CHANNEL{
    snprintf(q, sizeof q, "|%11d|%11d|%11ld|%11.3f|%11.3f|%11d|%11d|\n",
            item->id,
            item->prog.secure_out.id,
            item->prog.secure_out.timeout.tv_sec,
            item->prog.secure_out.heater_duty_cycle,
            item->prog.secure_out.cooler_duty_cycle,
            item->prog.secure_out.active,
            item->prog.secure_out.done
            );
    SEND_STR(q)
}
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------------------------------+\n")
    SEND_STR("|              Prog green light                 |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|  prog_id  |   active  |   value   |green_value|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    FOREACH_CHANNEL{
    snprintf(q, sizeof q, "|%11d|%11d|%11.3f|%11.3f|\n",
            item->id,
            item->prog.green_light.active,
            item->prog.green_light.sensor.value.value,
            item->prog.green_light.green_value
            );
    SEND_STR(q)
}
    SEND_STR("+-----------+-----------+-----------+-----------+\n")

    acp_sendPeerListInfo(&peer_list, response, &peer_client);

    SEND_STR("+-----------------------------------------------------------------------------------------------------------+\n")
    SEND_STR("|                                                    Prog EM                                                |\n")
    SEND_STR("+-----------+-----------------------------------------------+-----------------------------------------------+\n")
    SEND_STR("|           |                   EM heater                   |                  EM cooler                    |\n")
    SEND_STR("+           +-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|  prog_id  |     id    | remote_id |  pwm_rsl  |  peer_id  |     id    | remote_id |  pwm_rsl  |  peer_id  |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
        FOREACH_CHANNEL{
    snprintf(q, sizeof q, "|%11d|%11d|%11d|%11.3f|%11s|%11d|%11d|%11.3f|%11s|\n",
            item->id,

            item->prog.heater.em.id,
            item->prog.heater.em.remote_id,
            item->prog.heater.em.pwm_rsl,
            item->prog.heater.em.peer.id,

            item->prog.cooler.em.id,
            item->prog.cooler.em.remote_id,
            item->prog.cooler.em.pwm_rsl,
            item->prog.cooler.em.peer.id
            );
    SEND_STR(q)
}
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------+------------------------------------------------------------------------------+\n")
    SEND_STR("|    Prog   |                                   Sensor                                     |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+------------------------------------------+\n")
    SEND_STR("|           |           |           |           |                   value                  |\n")
    SEND_STR("|           |           |           |           |-----------+-----------+-----------+------+\n")
    SEND_STR("|    id     |    id     | remote_id |  peer_id  |   value   |    sec    |   nsec    | state|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+------+\n")
    FOREACH_CHANNEL{
    snprintf(q, sizeof q, "|%11d|%11d|%11d|%11s|%11.3f|%11ld|%11ld|%6d|\n",
            item->id,
            item->prog.sensor.id,
            item->prog.sensor.remote_id,
            item->prog.sensor.peer.id,
            item->prog.sensor.value.value,
            item->prog.sensor.value.tm.tv_sec,
            item->prog.sensor.value.tm.tv_nsec,
            item->prog.sensor.value.state
            );
    SEND_STR(q)
}
    SEND_STR_L("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+------+\n")

}

void printHelp(ACPResponse *response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tload prog into RAM and start its execution; program id expected\n", ACP_CMD_PROG_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM; program id expected\n", ACP_CMD_PROG_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM, after that load it; program id expected\n", ACP_CMD_PROG_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tenable running program; program id expected\n", ACP_CMD_PROG_ENABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tdisable running program; program id expected\n", ACP_CMD_PROG_DISABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog state (1-enabled, 0-disabled); program id expected\n", ACP_CMD_PROG_GET_ENABLED);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tsave data to database or not; program id and value (1 || 0) expected\n", ACP_CMD_PROG_SET_SAVE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog goal; program id expected\n", ACP_CMD_REG_PROG_GET_GOAL);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog error code; program id expected\n", ACP_CMD_PROG_GET_ERROR);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog sensor value; program id expected\n", ACP_CMD_GET_FTS);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset heater power; program id and value expected\n", ACP_CMD_REG_PROG_SET_HEATER_POWER);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset cooler power; program id and value expected\n", ACP_CMD_REG_PROG_SET_COOLER_POWER);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset goal; program id and value expected\n", ACP_CMD_REG_PROG_SET_GOAL);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset regulator EM mode (cooler or heater or both); program id and value expected\n", ACP_CMD_REG_PROG_SET_EM_MODE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset regulator heater mode (pid or onf); program id and value expected\n", ACP_CMD_REG_PROG_SET_HEATER_MODE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset regulator cooler mode (pid or onf); program id and value expected\n", ACP_CMD_REG_PROG_SET_COOLER_MODE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset heater delta; program id and value expected\n", ACP_CMD_REGONF_PROG_SET_HEATER_DELTA);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset heater kp; program id and value expected\n", ACP_CMD_REGSMP_PROG_SET_HEATER_KP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset heater ki; program id and value expected\n", ACP_CMD_REGSMP_PROG_SET_HEATER_KI);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset heater kd; program id and value expected\n", ACP_CMD_REGSMP_PROG_SET_HEATER_KD);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset cooler delta; program id and value expected\n", ACP_CMD_REGONF_PROG_SET_COOLER_DELTA);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset cooler kp; program id and value expected\n", ACP_CMD_REGSMP_PROG_SET_COOLER_KP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset cooler ki; program id and value expected\n", ACP_CMD_REGSMP_PROG_SET_COOLER_KI);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset cooler kd; program id and value expected\n", ACP_CMD_REGSMP_PROG_SET_COOLER_KD);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog runtime data in format:  progId\\tstate\\tstateEM\\toutputHeater\\toutputCooler\\ttimeRestSecToEMSwap\\tsensorValue\\tsensorState; program id expected\n", ACP_CMD_PROG_GET_DATA_RUNTIME);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog initial data in format;  progId\\tsetPoint\\thangeGap\\tmode\\theaterDelta\\theaterKp\\theaterKi\\theaterKd\\tcoolerDelta\\tcoolerKp\\tcoolerKi\\tcoolerKd; program id expected\n", ACP_CMD_PROG_GET_DATA_INIT);
    SEND_STR_L(q)
}
