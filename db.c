
#include "main.h"

int getChannel_callback ( void *data, int argc, char **argv, char **azColName ) {
    struct ds {
        void *p1;
        void *p2;
    } *d;
    d=data;
    sqlite3 *db=d->p2;
    Channel *item = d->p1;
    int load = 0, enable = 0;

    int c = 0;
    DB_FOREACH_COLUMN {
        if ( DB_COLUMN_IS ( "id" ) ) {
            item->id = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "sensor_remote_channel_id" ) ) {
            if ( !config_getRChannel ( &item->prog.sensor.remote_channel, DB_CVI, db, NULL ) ) {
                return EXIT_FAILURE;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "heater_remote_channel_id" ) ) {
            if ( !config_getRChannel ( &item->prog.heater.remote_channel, DB_CVI, db, NULL ) ) {
                return EXIT_FAILURE;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_remote_channel_id" ) ) {
            if ( !config_getRChannel ( &item->prog.cooler.remote_channel, DB_CVI, db, NULL ) ) {
                return EXIT_FAILURE;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "em_mode" ) ) {
            if ( strcmp ( REG_EM_MODE_COOLER_STR, DB_COLUMN_VALUE ) == 0 ) {
                item->prog.cooler.use = 1;
                item->prog.heater.use = 0;
            } else if ( strcmp ( REG_EM_MODE_HEATER_STR, DB_COLUMN_VALUE ) == 0 ) {
                item->prog.cooler.use = 0;
                item->prog.heater.use = 1;
            } else if ( strcmp ( REG_EM_MODE_BOTH_STR, DB_COLUMN_VALUE ) == 0 ) {
                item->prog.cooler.use = 1;
                item->prog.heater.use = 1;
            } else {
                item->prog.cooler.use = 0;
                item->prog.heater.use = 0;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "heater_mode" ) ) {
            if ( strncmp ( DB_COLUMN_VALUE, REG_MODE_PID_STR, 3 ) == 0 ) {
                item->prog.heater.mode = REG_MODE_PID;
            } else if ( strncmp ( DB_COLUMN_VALUE, REG_MODE_ONF_STR, 3 ) == 0 ) {
                item->prog.heater.mode = REG_MODE_ONF;
            } else {
                item->prog.heater.mode = REG_OFF;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_mode" ) ) {
            if ( strncmp ( DB_COLUMN_VALUE, REG_MODE_PID_STR, 3 ) == 0 ) {
                item->prog.cooler.mode = REG_MODE_PID;
            } else if ( strncmp ( DB_COLUMN_VALUE, REG_MODE_ONF_STR, 3 ) == 0 ) {
                item->prog.cooler.mode = REG_MODE_ONF;
            } else {
                item->prog.cooler.mode = REG_OFF;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "goal" ) ) {
            item->prog.goal = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "heater_delta" ) ) {
            item->prog.heater.delta = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "heater_output_min" ) ) {
            item->prog.heater.output_min = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "heater_output_max" ) ) {
            item->prog.heater.output_max = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "heater_kp" ) ) {
            item->prog.heater.pid.kp = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "heater_ki" ) ) {
            item->prog.heater.pid.ki = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "heater_kd" ) ) {
            item->prog.heater.pid.kd = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_kp" ) ) {
            item->prog.cooler.pid.kp = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_ki" ) ) {
            item->prog.cooler.pid.ki = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_kd" ) ) {
            item->prog.cooler.pid.kd = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_delta" ) ) {
            item->prog.cooler.delta = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_output_min" ) ) {
            item->prog.cooler.output_min = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_output_max" ) ) {
            item->prog.cooler.output_max = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "change_gap_sec" ) ) {
            item->prog.change_gap.tv_sec = DB_CVI;
            item->prog.change_gap.tv_nsec = 0;
            c++;
        } else if ( DB_COLUMN_IS ( "secure_id" ) ) {
            if ( !reg_getSecureFDB ( &item->prog.secure_out, DB_CVI, db, NULL ) ) {
                item->prog.secure_out.active = 0;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "green_light_id" ) ) {
            if ( !config_getGreenLight ( &item->prog.green_light, DB_CVI, db, NULL ) ) {
                item->prog.green_light.active=0;
            }
            c++;
        } else if ( DB_COLUMN_IS ( "cycle_duration_sec" ) ) {
            item->cycle_duration.tv_sec=DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "cycle_duration_nsec" ) ) {
            item->cycle_duration.tv_nsec=DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "save" ) ) {
            item->save = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "enable" ) ) {
            enable = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "load" ) ) {
            load = DB_CVI;
            c++;
        } else {
            printde ( "unknown column (we will skip it): %s\n", DB_COLUMN_NAME );
        }
    }
#define N 28
    if ( c != N ) {
        printde ( "required %d columns but %d found\n", N, c );
        return EXIT_FAILURE;
    }
#undef N
    if ( enable ) {
        regpidonfhc_enable ( &item->prog );
    } else {
        regpidonfhc_disable ( &item->prog );
    }
    if ( !load ) {
        db_saveTableFieldInt ( "channel", "load", item->id, 1, db, NULL );
    }
    return EXIT_SUCCESS;
}

int getChannelByIdFromDB ( Channel *item,int id, sqlite3 *dbl, const char *db_path ) {
    int close=0;
    sqlite3 *db=db_openRAlt ( dbl, db_path, &close );
    if ( db==NULL ) {
        putsde ( " failed\n" );
        return 0;
    }
    char q[LINE_SIZE];
    struct ds {
        void *p1;
        void *p2;
    } data= {.p1=item, .p2=db};
    snprintf ( q, sizeof q, "select * from channel where id=%d", id );
    if ( !db_exec ( db, q, getChannel_callback, &data ) ) {
        putsde ( " failed\n" );
        if ( close ) db_close ( db );
        return 0;
    }
    if ( close ) db_close ( db );
    return 1;
}

int addChannel ( Channel *item, ChannelLList *list, Mutex *list_mutex ) {
    if ( list->length >= INT_MAX ) {
        printde ( "can not load channel with id=%d - list length exceeded\n", item->id );
        return 0;
    }
    if ( list->top == NULL ) {
        lockMutex ( list_mutex );
        list->top = item;
        unlockMutex ( list_mutex );
    } else {
        lockMutex ( &list->last->mutex );
        list->last->next = item;
        unlockMutex ( &list->last->mutex );
    }
    list->last = item;
    list->length++;
    printdo ( "channel with id=%d loaded\n", item->id );
    return 1;
}
//returns deleted channel
Channel * deleteChannel ( int id, ChannelLList *list, Mutex *list_mutex ) {
    Channel *prev = NULL;
    FOREACH_LLIST ( curr,list,Channel ) {
        if ( curr->id == id ) {
            if ( prev != NULL ) {
                lockMutex ( &prev->mutex );
                prev->next = curr->next;
                unlockMutex ( &prev->mutex );
            } else {//curr=top
                lockMutex ( list_mutex );
                list->top = curr->next;
                unlockMutex ( list_mutex );
            }
            if ( curr == list->last ) {
                list->last = prev;
            }
            list->length--;
            return curr;
        }
        prev = curr;
    }
    return NULL;
}
int addChannelById ( int id, ChannelLList *list, Mutex *list_mutex, sqlite3 *dbl, const char *db_path ) {
    {
        Channel *item;
        LLIST_GETBYID ( item,list,id )
        if ( item != NULL ) {
            printde ( "channel with id = %d is being controlled\n", item->id );
            return 0;
        }
    }
    Channel *item = malloc ( sizeof * ( item ) );
    if ( item == NULL ) {
        putsde ( "failed to allocate memory\n" );
        return 0;
    }
    memset ( item, 0, sizeof *item );
    item->id = id;
    item->next = NULL;
    if ( !getChannelByIdFromDB ( item, id, dbl, db_path ) ) {
        free ( item );
        return 0;
    }
    if(!regpidonfhc_check(&item->prog)){
        free ( item );
        return 0;
    }
    if ( !checkChannel ( item ) ) {
        free ( item );
        return 0;
    }
    if ( !initMutex ( &item->mutex ) ) {
        free ( item );
        return 0;
    }
    if ( !initClient ( &item->sock_fd, WAIT_RESP_TIMEOUT ) ) {
        freeMutex ( &item->mutex );
        free ( item );
        return 0;
    }
    if ( !regpidonfhc_init ( &item->prog, &item->sock_fd ) ) {
        freeMutex ( &item->mutex );
        free ( item );
        return 0;
    }
    item->prog.secure_out.error_code=&item->error_code;

    if ( !addChannel ( item, list, list_mutex ) ) {
        freeSocketFd ( &item->sock_fd );
        freeMutex ( &item->mutex );
        free ( item );
        return 0;
    }
    if ( !createMThread ( &item->thread, &threadFunction, item ) ) {
        deleteChannel ( item->id, list, list_mutex );
        freeSocketFd ( &item->sock_fd );
        freeMutex ( &item->mutex );
        free ( item );
        return 0;
    }
    if ( item->save ) db_saveTableFieldInt ( "channel", "load", item->id, 1, dbl, db_path );
    return 1;
}

int deleteChannelById ( int id, ChannelLList *list, Mutex *list_mutex, sqlite3 *dbl, const char *db_path ) {
    printdo ( "channel to delete: %d\n", id );
    Channel *del_channel= deleteChannel ( id, list, list_mutex );
    if ( del_channel==NULL ) {
        putsdo ( "channel to delete not found\n" );
        return 0;
    }
    STOP_CHANNEL_THREAD ( del_channel );
    if ( del_channel->save ) db_saveTableFieldInt ( "channel", "load", del_channel->id, 0, dbl, db_path );
    freeChannel ( del_channel );
    printdo ( "channel with id: %d has been deleted from channel_list\n", id );
    return 1;
}
int loadActiveChannel_callback ( void *data, int argc, char **argv, char **azColName ) {
    struct ds {
        void *p1;
        void *p2;
        void *p3;
    } *d;
    d=data;
    ChannelLList *list=d->p1;
    Mutex *list_mutex=d->p2;
    sqlite3 *db=d->p3;
    DB_FOREACH_COLUMN {
        if ( DB_COLUMN_IS ( "id" ) ) {
            int id = DB_CVI;
            addChannelById ( id, list, list_mutex, db,NULL );
        } else {
            printde ( "unknown column (we will skip it): %s\n", DB_COLUMN_NAME );
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveChannel ( ChannelLList *list, Mutex *list_mutex,  char *db_path ) {
    sqlite3 *db;
    if ( !db_open ( db_path, &db ) ) {
        return 0;
    }
    struct ds {
        void *p1;
        void *p2;
        void *p3;
    };
    struct ds data= {.p1=list, .p2=list_mutex, .p3=db};
    char *q = "select id from channel where load=1";
    if ( !db_exec ( db, q, loadActiveChannel_callback, &data ) ) {
        putsde ( " failed\n" );
        sqlite3_close ( db );
        return 0;
    }
    sqlite3_close ( db );
    return 1;
}
