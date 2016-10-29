
#include "lib/pid.h"
#include "main.h"

Peer *getPeerById(char *id, const PeerList *list) {
    LIST_GET_BY_IDSTR
}

Sensor * getSensorById(int id, const SensorList *list) {
    LIST_GET_BY_ID
}

Prog * getProgById(int id, const ProgList *list) {
    LLIST_GET_BY_ID(Prog)
}

EM * getEMById(int id, const EMList *list) {
    LIST_GET_BY_ID
}

void saveProg(Prog *item) {
    if (item->goal != item->sv.goal || item->pid.mode != item->sv.mode || item->pid.kp != item->sv.kp || item->pid.ki != item->sv.ki || item->pid.kd != item->sv.kd) {
        PGresult *r;
        char q[LINE_SIZE];
        char mode[NAME_SIZE];
        switch (item->pid.mode) {
            case PID_MODE_HEATER:
                strcpy(mode, MODE_STR_HEATER);
                break;
            case PID_MODE_COOLER:
                strcpy(mode, MODE_STR_COOLER);
                break;
            default:
                strcpy(mode, UNKNOWN_STR);
        }
        snprintf(q, sizeof q, "update %s.prog set goal=%f, mode='%s', kp=%f, ki=%f, kd=%f where app_class='%s' and id=%d", APP_NAME, item->goal, mode, item->pid.kp, item->pid.ki, item->pid.kd, app_class, item->id);
        if ((r = dbGetDataC(*db_connp_data, q, "backupDelProgById: delete from backup: ")) != NULL) {
            item->sv.goal = item->goal;
            item->sv.mode = item->pid.mode;
            item->sv.kp = item->pid.kp;
            item->sv.ki = item->pid.ki;
            item->sv.kd = item->pid.kd;
            PQclear(r);
        }
    }
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

int bufCatProg(const Prog *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    char mode[NAME_SIZE];
    char state[NAME_SIZE];
    switch (item->pid.mode) {
        case PID_MODE_HEATER:
            strcpy(mode, MODE_STR_HEATER);
            break;
        case PID_MODE_COOLER:
            strcpy(mode, MODE_STR_COOLER);
            break;
        default:
            strcpy(mode, UNKNOWN_STR);
    }
    switch (item->state) {
        case INIT:
            strcpy(state, STATE_STR_INIT);
            break;
        case REG:
            strcpy(state, STATE_STR_REG);
            break;
        case TUNE:
            strcpy(state, STATE_STR_TUNE);
            break;
        default:
            strcpy(state, UNKNOWN_STR);
    }
    snprintf(q, sizeof q, "%d_%f_%s_%s_%f_%f_%f\n", item->id, item->goal, mode, state, item->pid.kp, item->pid.ki, item->pid.kd);
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

void printAll(ProgList *list, PeerList *pl, EMList *el, SensorList *sl) {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    size_t i;
    sendStr("+----------------------------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                    Program                                                           |\n", &crc);
    sendStr("+-----------+------+-----------+--------------------+------+-----------------------------------------------------------+\n", &crc);
    sendStr("|           |      |           |                    |      |                            PID                            |\n", &crc);
    sendStr("|           |      |           |                    |      |-----------+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|    id     | mode |    goal   |        output      | state|    kp     |    ki     |    kd     |  ierr     |  preverr  |\n", &crc);
    sendStr("+-----------+------+-----------+--------------------+------+-----------+-----------+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%6c|%11.1f|%20.1f|%6hhd|%11.1f|%11.1f|%11.1f|%11.1f|%11.1f|\n",
            curr->id,
            curr->pid.mode,
            curr->goal,
            curr->output,
            curr->state,
            curr->pid.kp,
            curr->pid.ki,
            curr->pid.kd,
            curr->pid.integral_error,
            curr->pid.previous_error
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+------+-----------+--------------------+------+-----------+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                  Peer                                   |\n", &crc);
    sendStr("+--------------------------------+-----------+----------------+-----------+\n", &crc);
    sendStr("|               id               |  udp_port |      addr      |     fd    |\n", &crc);
    sendStr("+--------------------------------+-----------+----------------+-----------+\n", &crc);
    for (i = 0; i < pl->length; i++) {
        snprintf(q, sizeof q, "|%32s|%11u|%16u|%11d|\n",
                pl->item[i].id,
                pl->item[i].addr.sin_port,
                pl->item[i].addr.sin_addr.s_addr,
                *pl->item[i].fd
                );
        sendStr(q, &crc);
    }
    sendStr("+--------------------------------+-----------+----------------+-----------+\n", &crc);

    sendStr("+--------------------------------------------------------+\n", &crc);
    sendStr("|                          EM                            |\n", &crc);
    sendStr("+-----------+-----------+--------------------------------+\n", &crc);
    sendStr("|     id    | remote_id |             peer_id            |\n", &crc);
    sendStr("+-----------+-----------+--------------------------------+\n", &crc);
    for (i = 0; i < el->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|%32s|\n",
                el->item[i].id,
                el->item[i].remote_id,
                el->item[i].source->id
                );
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+--------------------------------+\n", &crc);

    sendStr("+---------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                     Sensor                                        |\n", &crc);
    sendStr("+-----------+-----------+--------------------------------+------------------------------------------+\n", &crc);
    sendStr("|           |           |                                |                   value                  |\n", &crc);
    sendStr("|           |           |                                |-----------+-----------+-----------+------+\n", &crc);
    sendStr("|     id    | remote_id |             peer_id            |   temp    |   sec     |   nsec    | state|\n", &crc);
    sendStr("+-----------+-----------+--------------------------------+-----------+-----------+-----------+------+\n", &crc);
    for (i = 0; i < sl->length; i++) {
        snprintf(q, sizeof q, "|%11d|%11d|%32s|%11f|%11ld|%11ld|%6d|\n",
                sl->item[i].id,
                sl->item[i].remote_id,
                sl->item[i].source->id,
                sl->item[i].value.temp,
                sl->item[i].value.tm.tv_sec,
                sl->item[i].value.tm.tv_nsec,
                sl->item[i].value.state
                );
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+--------------------------------+-----------+-----------+-----------+------+\n", &crc);
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
    snprintf(q, sizeof q, "%c\tget prog data in format: progId_goal_mode_state_Kp_Ki_Kd; program id expected if '.' quantifier is used\n", ACP_CMD_REGSMP_PROG_GET_DATA);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset program state; state expected: 'r' - PID regulator, 't' - PID autotune; program id expected if '.' quantifier is used\n", ACP_CMD_REGSMP_PROG_SET_STATE);
    sendStr(q, &crc);
    sendFooter(crc);
}
