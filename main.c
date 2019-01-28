#include "main.h"

int app_state = APP_INIT;

TSVresult config_tsv = TSVRESULT_INITIALIZER;
char * db_path;
char * peer_id;

int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
Mutex channel_list_mutex = MUTEX_INITIALIZER;

ChannelLList channel_list = LLIST_INITIALIZER;

#include "util.c"
#include "db.c"

int readSettings ( TSVresult* r,char *config_path, char **peer_id, char **db_path ) {
    if ( !TSVinit ( r, config_path ) ) {
        return 0;
    }
    char *_peer_id = TSVgetvalues ( r, 0, "peer_id" );
    char *_db_path = TSVgetvalues ( r, 0, "db_path" );
    if ( TSVnullreturned ( r ) ) {
        return 0;
    }
    *peer_id = _peer_id;
    *db_path = _db_path;
    return 1;
}

int initApp() {
    if ( !readSettings ( &config_tsv, CONFIG_FILE, &peer_id, &db_path ) ) {
        putsde ( "failed to read settings\n" );
        return 0;
    }
    if ( !initMutex ( &channel_list_mutex ) ) {
        TSVclear ( &config_tsv );
        putsde ( "failed to initialize channel mutex\n" );
        return 0;
    }
    if ( !config_getPort ( &sock_port, peer_id, NULL, db_path ) ) {
        freeMutex ( &channel_list_mutex );
        TSVclear ( &config_tsv );
        putsde ( "failed to read port\n" );
        return 0;
    }
    if ( !initServer ( &sock_fd, sock_port ) ) {
        freeMutex ( &channel_list_mutex );
        TSVclear ( &config_tsv );
        putsde ( "failed to initialize udp server\n" );
        return 0;
    }
    printdo ( "initApp:\n\tsock_port=%d\n\tdb_path=%s\n",sock_port, db_path );
    return 1;
}

int initData() {
    if ( !loadActiveChannel ( &channel_list, &channel_list_mutex,  db_path ) ) {
        freeChannelList ( &channel_list );
        return 0;
    }
    return 1;
}

void serverRun ( int *state, int init_state ) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_STOP ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_turnOff ( &item->prog );
                    unlockMutex ( &item->mutex );
                }
                deleteChannelById ( i1l.item[i], &channel_list , &channel_list_mutex, NULL, db_path );
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_START ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            addChannelById ( i1l.item[i], &channel_list, &channel_list_mutex ,  NULL, db_path );
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_RESET ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_turnOff ( &item->prog );
                    unlockMutex ( &item->mutex );
                }
                deleteChannelById ( i1l.item[i], &channel_list, &channel_list_mutex, NULL, db_path );
            }
        }
        FORLISTN(i1l, i) {
            addChannelById ( i1l.item[i], &channel_list, &channel_list_mutex, NULL, db_path );
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_ENABLE ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_enable ( &item->prog );
                    if ( item->save ) db_saveTableFieldInt ( "channel", "enable", item->id, 1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_DISABLE ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_disable ( &item->prog );
                    if ( item->save ) db_saveTableFieldInt ( "channel", "enable", item->id, 0, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_SET_SAVE ) ) {
        SERVER_GET_I2LIST_FROM_REQUEST
        FORLISTN(i2l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i2l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    item->save = i2l.item[i].p1;
                    if ( item->save ) db_saveTableFieldInt ( "channel", "save", item->id, i2l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_GET_DATA_RUNTIME ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( !bufCatChannelRuntime ( item, &response ) ) {
                    return;
                }
            }
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_GET_DATA_INIT ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( !bufCatChannelInit ( item, &response ) ) {
                    return;
                }
            }
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_GET_ENABLED ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( !bufCatChannelEnabled ( item, &response ) ) {
                    return;
                }
            }
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_GET_GOAL ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( !bufCatChannelGoal ( item, &response ) ) {
                    return;
                }
            }
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_GET_ERROR ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( !bufCatChannelError ( item, &response ) ) {
                    return;
                }
            }
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_GET_FTS ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN(i1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] )
            if ( item != NULL ) {
                if ( !bufCatChannelFTS ( item, &response ) ) {
                    return;
                }
            }
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_SET_HEATER_POWER ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setHeaterPower ( &item->prog, i1f1l.item[i].p1 );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_SET_COOLER_POWER ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setCoolerPower ( &item->prog, i1f1l.item[i].p1 );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_SET_HEATER_MODE ) ) {
        SERVER_GET_I1S1LIST_FROM_REQUEST
        FORLISTN(i1s1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1s1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setHeaterMode ( &item->prog, i1s1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldText ( "channel", "heater_mode", i1s1l.item[i].p0, i1s1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_SET_COOLER_MODE ) ) {
        SERVER_GET_I1S1LIST_FROM_REQUEST
        FORLISTN(i1s1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1s1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setCoolerMode ( &item->prog, i1s1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldText ( "channel", "cooler_mode", i1s1l.item[i].p0, i1s1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_SET_EM_MODE ) ) {
        SERVER_GET_I1S1LIST_FROM_REQUEST
        FORLISTN(i1s1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1s1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setEMMode ( &item->prog, i1s1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldText ( "channel", "em_mode", i1s1l.item[i].p0, i1s1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_SET_CHANGE_GAP ) ) {
        SERVER_GET_I2LIST_FROM_REQUEST
        FORLISTN(i2l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i2l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setChangeGap ( &item->prog, i2l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldInt ( "channel", "change_gap", i2l.item[i].p0, i2l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REG_PROG_SET_GOAL ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setGoal ( &item->prog, i1f1l.item[i].p1 );
                    regpidonfhc_secureOutTouch ( &item->prog );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "goal", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGONF_PROG_SET_HEATER_DELTA ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setHeaterDelta ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "heater_delta", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGSMP_PROG_SET_HEATER_KP ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setHeaterKp ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "heater_kp", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGSMP_PROG_SET_HEATER_KI ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setHeaterKi ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "heater_ki", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGSMP_PROG_SET_HEATER_KD ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setHeaterKd ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "heater_kd", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGONF_PROG_SET_COOLER_DELTA ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setCoolerDelta ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "cooler_delta", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGSMP_PROG_SET_COOLER_KP ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setCoolerKp ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "cooler_kp", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGSMP_PROG_SET_COOLER_KI ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setCoolerKi ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "cooler_ki", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_REGSMP_PROG_SET_COOLER_KD ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN(i1f1l, i) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1f1l.item[i].p0 )
            if ( item != NULL ) {
                if ( lockMutex ( &item->mutex ) ) {
                    regpidonfhc_setCoolerKd ( &item->prog, i1f1l.item[i].p1 );
                    if ( item->save ) db_saveTableFieldFloat ( "channel", "cooler_kd", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_path );
                    unlockMutex ( &item->mutex );
                }
            }
        }
        return;
    }

    acp_responseSend ( &response, &peer_client );
}

void cleanup_handler ( void *arg ) {
    Channel *item = arg;
    printf ( "cleaning up thread %d\n", item->id );
}

void *threadFunction ( void *arg ) {
    Channel *item = arg;
    printdo ( "thread for channel with id=%d has been started\n", item->id );

#ifdef MODE_DEBUG
    pthread_cleanup_push ( cleanup_handler, item );
#endif
    while ( 1 ) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if ( threadCancelDisable ( &old_state ) ) {
            if ( lockMutex ( &item->mutex ) ) {
#ifdef MODE_DEBUG
                printf ( "channelId: %d ", item->id );
#endif
                regpidonfhc_control ( &item->prog );
                unlockMutex ( &item->mutex );
            }
            threadSetCancelState ( old_state );
        }
        delayTsIdleRest ( item->cycle_duration, t1 );
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop ( 1 );
#endif
}

void freeData() {
    STOP_ALL_CHANNEL_THREADS ( &channel_list );
    secure();
    freeChannelList ( &channel_list );
}

void freeApp() {
    freeData();
    freeSocketFd ( &sock_fd );
    freeMutex ( &channel_list_mutex );
    TSVclear ( &config_tsv );
}

void exit_nicely ( ) {
    freeApp();
    putsdo ( "\nexiting now...\n" );
    exit ( EXIT_SUCCESS );
}

int main ( int argc, char** argv ) {
#ifndef MODE_DEBUG
    daemon ( 0, 0 );
#endif
    conSig ( &exit_nicely );
    if ( mlockall ( MCL_CURRENT | MCL_FUTURE ) == -1 ) {
        perrorl ( "mlockall()" );
    }
    int data_initialized = 0;
    while ( 1 ) {
#ifdef MODE_DEBUG
        printf ( "%s(): %s %d\n", F, getAppState ( app_state ), data_initialized );
#endif
        switch ( app_state ) {
        case APP_INIT:
            if ( !initApp() ) {
                return ( EXIT_FAILURE );
            }
            app_state = APP_INIT_DATA;
            break;
        case APP_INIT_DATA:
            data_initialized = initData();
            app_state = APP_RUN;
            delayUsIdle ( 1000000 );
            break;
        case APP_RUN:
            serverRun ( &app_state, data_initialized );
            break;
        case APP_STOP:
            freeData();
            data_initialized = 0;
            app_state = APP_RUN;
            break;
        case APP_RESET:
            freeApp();
            delayUsIdle ( 1000000 );
            data_initialized = 0;
            app_state = APP_INIT;
            break;
        case APP_EXIT:
            exit_nicely ( );
            break;
        default:
            freeApp();
            putsde ( "unknown application state\n" );
            return ( EXIT_FAILURE );
        }
    }
    freeApp();
    putsde ( "unexpected while break\n" );
    return ( EXIT_FAILURE );
}
