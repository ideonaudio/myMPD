/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>

#include <mpd/client.h>

#include "../dist/src/sds/sds.h"
#include "sds_extras.h"
#include "log.h"
#include "list.h"
#include "config_defs.h"
#include "tiny_queue.h"
#include "api.h"
#include "global.h"
#include "utility.h"
#include "lua_mympd_state.h"
#include "mpd_shared/mpd_shared_typedefs.h"
#include "mpd_shared/mpd_shared_tags.h"
#include "mpd_shared.h"
#include "mpd_shared/mpd_shared_sticker.h"
#include "mpd_client/mpd_client_utility.h"
#include "mpd_client/mpd_client_api.h"
#include "mpd_client/mpd_client_browse.h"
#include "mpd_client/mpd_client_jukebox.h"
#include "mpd_client/mpd_client_playlists.h"
#include "mpd_client/mpd_client_stats.h"
#include "mpd_client/mpd_client_state.h"
#include "mpd_client/mpd_client_features.h"
#include "mpd_client/mpd_client_queue.h"
#include "mpd_client/mpd_client_features.h"
#include "mpd_client/mpd_client_sticker.h"
#include "mpd_client/mpd_client_timer.h"
#include "mpd_client/mpd_client_trigger.h"
#include "mpd_client.h"

//private definitions
static void mpd_client_idle(t_config *config, t_mpd_client_state *mpd_client_state);
static void mpd_client_parse_idle(t_config *config, t_mpd_client_state *mpd_client_state, const int idle_bitmask);

//public functions
void *mpd_client_loop(void *arg_config) {
    thread_logname = sdsreplace(thread_logname, "mpdclient");
    t_config *config = (t_config *) arg_config;
    //State of mpd connection
    t_mpd_client_state *mpd_client_state = (t_mpd_client_state *)malloc(sizeof(t_mpd_client_state));
    assert(mpd_client_state);
    default_mpd_client_state(mpd_client_state);
    triggerfile_read(config, mpd_client_state);
    //wait for initial settings
    while (s_signal_received == 0) {
        t_work_request *request = tiny_queue_shift(mpd_client_queue, 50, 0);
        if (request != NULL) {
            if (request->cmd_id == MYMPD_API_SETTINGS_SET) {
                LOG_DEBUG("Got initial settings from mympd_api");
                mpd_client_api(config, mpd_client_state, request);
                break;
            }
            //create response struct
            if (request->conn_id > -1) {
                t_work_result *response = create_result(request);
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "MPD disconnected", true);
                LOG_DEBUG("Send http response to connection %lu: %s", request->conn_id, response->data);
                tiny_queue_push(web_server_queue, response, 0);
            }
            LOG_DEBUG("mpd_client not initialized, discarding message");
            free_request(request);
        }
    }

    LOG_INFO("Starting mpd_client");
    trigger_execute(mpd_client_state, TRIGGER_MYMPD_START);
    //On startup connect instantly
    mpd_client_state->mpd_state->conn_state = MPD_DISCONNECTED;
    while (s_signal_received == 0) {
        mpd_client_idle(config, mpd_client_state);
    }
    trigger_execute(mpd_client_state, TRIGGER_MYMPD_STOP);
    //Cleanup
    mpd_shared_mpd_disconnect(mpd_client_state->mpd_state);
    mpd_client_last_played_list_save(config, mpd_client_state);
    triggerfile_save(config, mpd_client_state);
    sticker_cache_free(&mpd_client_state->sticker_cache);
    album_cache_free(&mpd_client_state->album_cache);
    free_trigerlist_arguments(mpd_client_state);
    free_mpd_client_state(mpd_client_state);
    sdsfree(thread_logname);
    return NULL;
}

//private functions
static void mpd_client_parse_idle(t_config *config, t_mpd_client_state *mpd_client_state, int idle_bitmask) {
    for (unsigned j = 0;; j++) {
        enum mpd_idle idle_event = 1 << j;
        const char *idle_name = mpd_idle_name(idle_event);
        if (idle_name == NULL) {
            break;
        }
        if (idle_bitmask & idle_event) {
            LOG_VERBOSE("MPD idle event: %s", idle_name);
            sds buffer = sdsempty();
            switch(idle_event) {
                case MPD_IDLE_DATABASE:
                    buffer = jsonrpc_notify(buffer, "update_database");
                    sticker_cache_init(config, mpd_client_state);
                    break;
                case MPD_IDLE_STORED_PLAYLIST:
                    buffer = jsonrpc_notify(buffer, "update_stored_playlist");
                    break;
                case MPD_IDLE_QUEUE:
                    buffer = mpd_client_get_queue_state(mpd_client_state, buffer);
                    //jukebox enabled
                    if (mpd_client_state->jukebox_mode != JUKEBOX_OFF && mpd_client_state->queue_length < mpd_client_state->jukebox_queue_length) {
                        mpd_client_jukebox(config, mpd_client_state, 0);
                    }
                    //autoPlay enabled
                    if (mpd_client_state->auto_play == true && mpd_client_state->queue_length > 1) {
                        if (mpd_client_state->mpd_state->state != MPD_STATE_PLAY) {
                            LOG_VERBOSE("AutoPlay enabled, start playing");
                            if (!mpd_run_play(mpd_client_state->mpd_state->conn)) {
                                check_error_and_recover(mpd_client_state->mpd_state, NULL, NULL, 0);
                            }
                        }
                    }
                    break;
                case MPD_IDLE_PLAYER:
                    //get and put mpd state                
                    buffer = mpd_client_put_state(config, mpd_client_state, buffer, NULL, 0);
                    //song has changed
                    if (mpd_client_state->song_id != mpd_client_state->last_song_id && mpd_client_state->last_skipped_id != mpd_client_state->last_song_id 
                        && mpd_client_state->last_song_uri != NULL)
                    {
                        time_t now = time(NULL);
                        if (mpd_client_state->feat_sticker && mpd_client_state->last_song_end_time > now) {
                            //last song skipped
                            time_t elapsed = now - mpd_client_state->last_song_start_time;
                            if (elapsed > 10 && mpd_client_state->last_song_start_time > 0) {
                                LOG_DEBUG("Song %s skipped", mpd_client_state->last_song_uri);
                                mpd_client_sticker_inc_skip_count(mpd_client_state, mpd_client_state->last_song_uri);
                                mpd_client_sticker_last_skipped(mpd_client_state, mpd_client_state->last_song_uri);
                                mpd_client_state->last_skipped_id = mpd_client_state->last_song_id;
                            }
                        }
                    }
                    break;
                case MPD_IDLE_MIXER:
                    buffer = mpd_client_put_volume(mpd_client_state, buffer, NULL, 0);
                    break;
                case MPD_IDLE_OUTPUT:
                    buffer = jsonrpc_notify(buffer, "update_outputs");
                    break;
                case MPD_IDLE_OPTIONS:
                    mpd_client_get_queue_state(mpd_client_state, NULL);
                    buffer = jsonrpc_notify(buffer, "update_options");
                    break;
                case MPD_IDLE_UPDATE:
                    buffer = mpd_client_get_updatedb_state(mpd_client_state, buffer);
                    break;
                case MPD_IDLE_SUBSCRIPTION:
                    if (mpd_client_state->love == true) {
                        bool old_love = mpd_client_state->feat_love;
                        mpd_client_feature_love(mpd_client_state);
                        if (old_love != mpd_client_state->feat_love) {
                            buffer = jsonrpc_notify(buffer, "update_options");
                        }
                    }
                    break;
                case MPD_IDLE_PARTITION:
                    //todo: check list of partitions and create new mpd_client threads
                    break;
                default: {
                    //other idle events not used
                }
            }
            if (config->scripting == true) {
                trigger_execute(mpd_client_state, (enum trigger_events)idle_event);
            }
            if (sdslen(buffer) > 0) {
                ws_notify(buffer);
            }
            sdsfree(buffer);
        }
    }
}

static void mpd_client_idle(t_config *config, t_mpd_client_state *mpd_client_state) {
    struct pollfd fds[1];
    int pollrc;
    sds buffer = sdsempty();
    unsigned mpd_client_queue_length = 0;
    switch (mpd_client_state->mpd_state->conn_state) {
        case MPD_WAIT: {
            time_t now = time(NULL);
            if (now > mpd_client_state->mpd_state->reconnect_time) {
                mpd_client_state->mpd_state->conn_state = MPD_DISCONNECTED;
            }
            //mpd_client_api error response
            mpd_client_queue_length = tiny_queue_length(mpd_client_queue, 50);
            if (mpd_client_queue_length > 0) {
                //Handle request
                LOG_DEBUG("Handle request (mpd disconnected)");
                t_work_request *request = tiny_queue_shift(mpd_client_queue, 50, 0);
                if (request != NULL) {
                    if (request->cmd_id == MYMPD_API_SETTINGS_SET) {
                        //allow to change mpd host
                        mpd_client_api(config, mpd_client_state, request);
                        mpd_client_state->mpd_state->conn_state = MPD_DISCONNECTED;
                    }
                    else {
                        //other requests not allowed
                        if (request->conn_id > -1) {
                            t_work_result *response = create_result(request);
                            response->data = jsonrpc_respond_message(response->data, request->method, request->id, "MPD disconnected", true);
                            LOG_DEBUG("Send http response to connection %lu: %s", request->conn_id, response->data);
                            tiny_queue_push(web_server_queue, response, 0);
                        }
                        free_request(request);
                    }
                }
            }
            if (now < mpd_client_state->mpd_state->reconnect_time) {
                //pause 100ms to prevent high cpu usage
                my_usleep(100000);
            }
            break;
        }
        case MPD_DISCONNECTED:
            /* Try to connect */
            if (strncmp(mpd_client_state->mpd_state->mpd_host, "/", 1) == 0) {
                LOG_INFO("MPD connecting to socket %s", mpd_client_state->mpd_state->mpd_host);
            }
            else {
                LOG_INFO("MPD connecting to %s:%d", mpd_client_state->mpd_state->mpd_host, mpd_client_state->mpd_state->mpd_port);
            }
            mpd_client_state->mpd_state->conn = mpd_connection_new(mpd_client_state->mpd_state->mpd_host, mpd_client_state->mpd_state->mpd_port, mpd_client_state->mpd_state->timeout);
            if (mpd_client_state->mpd_state->conn == NULL) {
                LOG_ERROR("MPD connection to failed: out-of-memory");
                buffer = jsonrpc_notify(buffer, "mpd_disconnected");
                ws_notify(buffer);
                sdsfree(buffer);
                mpd_client_state->mpd_state->conn_state = MPD_FAILURE;
                mpd_connection_free(mpd_client_state->mpd_state->conn);
                return;
            }

            if (mpd_connection_get_error(mpd_client_state->mpd_state->conn) != MPD_ERROR_SUCCESS) {
                LOG_ERROR("MPD connection: %s", mpd_connection_get_error_message(mpd_client_state->mpd_state->conn));
                buffer = jsonrpc_start_phrase_notify(buffer, "MPD connection error: %{error}", true);
                buffer = tojson_char(buffer, "error", mpd_connection_get_error_message(mpd_client_state->mpd_state->conn), false);
                buffer = jsonrpc_end_phrase(buffer);
                ws_notify(buffer);
                sdsfree(buffer);
                mpd_client_state->mpd_state->conn_state = MPD_FAILURE;
                return;
            }

            if (sdslen(mpd_client_state->mpd_state->mpd_pass) > 0 && !mpd_run_password(mpd_client_state->mpd_state->conn, mpd_client_state->mpd_state->mpd_pass)) {
                LOG_ERROR("MPD connection: %s", mpd_connection_get_error_message(mpd_client_state->mpd_state->conn));
                buffer = jsonrpc_start_phrase_notify(buffer, "MPD connection error: %{error}", true);
                buffer = tojson_char(buffer, "error", mpd_connection_get_error_message(mpd_client_state->mpd_state->conn), false);
                buffer = jsonrpc_end_phrase(buffer);
                ws_notify(buffer);
                sdsfree(buffer);
                mpd_client_state->mpd_state->conn_state = MPD_FAILURE;
                return;
            }

            LOG_INFO("MPD connected");
            mpd_connection_set_timeout(mpd_client_state->mpd_state->conn, mpd_client_state->mpd_state->timeout);
            buffer = jsonrpc_notify(buffer, "mpd_connected");
            ws_notify(buffer);
            mpd_client_state->mpd_state->conn_state = MPD_CONNECTED;
            mpd_client_state->mpd_state->reconnect_interval = 0;
            mpd_client_state->mpd_state->reconnect_time = 0;
            //reset list of supported tags
            reset_t_tags(&mpd_client_state->mpd_state->mpd_tag_types);
            //get mpd features
            mpd_client_mpd_features(config, mpd_client_state);
            //set binarylimit
            mpd_client_set_binarylimit(config, mpd_client_state);
            //update sticker cache
            sticker_cache_init(config, mpd_client_state);
            //update album cache
            album_cache_init(mpd_client_state);
            //set timer for smart playlist update
            mpd_client_set_timer(MYMPD_API_TIMER_SET, "MYMPD_API_TIMER_SET", 10, mpd_client_state->smartpls_interval, "timer_handler_smartpls_update");
            //jukebox
            if (mpd_client_state->jukebox_mode != JUKEBOX_OFF) {
                mpd_client_jukebox(config, mpd_client_state, 0);
            }
            if (!mpd_send_idle(mpd_client_state->mpd_state->conn)) {
                LOG_ERROR("Entering idle mode failed");
                mpd_client_state->mpd_state->conn_state = MPD_FAILURE;
            }
            trigger_execute(mpd_client_state, TRIGGER_MYMPD_CONNECTED);
            break;

        case MPD_FAILURE:
            LOG_ERROR("MPD connection failed");
            buffer = jsonrpc_notify(buffer, "mpd_disconnected");
            ws_notify(buffer);
            trigger_execute(mpd_client_state, TRIGGER_MYMPD_DISCONNECTED);
            // fall through
        case MPD_DISCONNECT:
        case MPD_RECONNECT:
            if (mpd_client_state->mpd_state->conn != NULL) {
                mpd_connection_free(mpd_client_state->mpd_state->conn);
            }
            mpd_client_state->mpd_state->conn = NULL;
            mpd_client_state->mpd_state->conn_state = MPD_WAIT;
            if (mpd_client_state->mpd_state->reconnect_interval <= 20) {
                mpd_client_state->mpd_state->reconnect_interval += 2;
            }
            mpd_client_state->mpd_state->reconnect_time = time(NULL) + mpd_client_state->mpd_state->reconnect_interval;
            LOG_VERBOSE("Waiting %u seconds before reconnection", mpd_client_state->mpd_state->reconnect_interval);
            break;

        case MPD_CONNECTED:
            fds[0].fd = mpd_connection_get_fd(mpd_client_state->mpd_state->conn);
            fds[0].events = POLLIN;
            pollrc = poll(fds, 1, 50);
            bool jukebox_add_song = false;
            bool set_played = false;
            mpd_client_queue_length = tiny_queue_length(mpd_client_queue, 50);
            time_t now = time(NULL);
            if (mpd_client_state->mpd_state->state == MPD_STATE_PLAY) {
                //handle jukebox and last played only in mpd play state
                if (now > mpd_client_state->set_song_played_time && mpd_client_state->set_song_played_time > 0 && mpd_client_state->last_last_played_id != mpd_client_state->song_id) {
                    set_played = true;
                }
                if (mpd_client_state->jukebox_mode != JUKEBOX_OFF) {
                    time_t add_time = mpd_client_state->crossfade < mpd_client_state->song_end_time ? mpd_client_state->song_end_time - mpd_client_state->crossfade : mpd_client_state->song_end_time;
                    if (now > add_time && add_time > 0 && mpd_client_state->queue_length <= mpd_client_state->jukebox_queue_length) {
                        jukebox_add_song = true;
                    }
                }
            }
            if (pollrc > 0 || mpd_client_queue_length > 0 || jukebox_add_song == true || set_played == true
                || mpd_client_state->sticker_queue.length > 0) 
            {
                LOG_DEBUG("Leaving mpd idle mode");
                if (!mpd_send_noidle(mpd_client_state->mpd_state->conn)) {
                    check_error_and_recover(mpd_client_state->mpd_state, NULL, NULL, 0);
                    mpd_client_state->mpd_state->conn_state = MPD_FAILURE;
                    break;
                }
                if (pollrc > 0) {
                    //Handle idle events
                    LOG_DEBUG("Checking for idle events");
                    enum mpd_idle idle_bitmask = mpd_recv_idle(mpd_client_state->mpd_state->conn, false);
                    mpd_client_parse_idle(config, mpd_client_state, idle_bitmask);
                } 
                else {
                    mpd_response_finish(mpd_client_state->mpd_state->conn);
                }
                
                if (set_played == true) {
                    mpd_client_state->last_last_played_id = mpd_client_state->song_id;
                    
                    if (mpd_client_state->last_played_count > 0) {
                        mpd_client_add_song_to_last_played_list(config, mpd_client_state, mpd_client_state->song_id);
                    }
                    if (mpd_client_state->feat_sticker == true) {
                        mpd_client_sticker_inc_play_count(mpd_client_state, mpd_client_state->song_uri);
                        mpd_client_sticker_last_played(mpd_client_state, mpd_client_state->song_uri);
                    }
                    trigger_execute(mpd_client_state, TRIGGER_MYMPD_SCROBBLE);
                }
                
                if (jukebox_add_song == true) {
                    mpd_client_jukebox(config, mpd_client_state, 0);
                }
                
                if (mpd_client_queue_length > 0) {
                    //Handle request
                    LOG_DEBUG("Handle request");
                    t_work_request *request = tiny_queue_shift(mpd_client_queue, 50, 0);
                    if (request != NULL) {
                        mpd_client_api(config, mpd_client_state, request);
                    }
                }
                
                if (mpd_client_state->sticker_queue.length > 0) {
                    mpd_client_sticker_dequeue(mpd_client_state);
                }
                
                LOG_DEBUG("Entering mpd idle mode");
                if (!mpd_send_idle(mpd_client_state->mpd_state->conn)) {
                    check_error_and_recover(mpd_client_state->mpd_state, NULL, NULL, 0);
                    mpd_client_state->mpd_state->conn_state = MPD_FAILURE;
                }
            }
            break;
        default:
            LOG_ERROR("Invalid mpd connection state");
    }
    sdsfree(buffer);
}
