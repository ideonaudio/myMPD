/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <mpd/client.h>

#include "../../dist/src/sds/sds.h"
#include "../sds_extras.h"
#include "../../dist/src/frozen/frozen.h"
#include "../list.h"
#include "config_defs.h"
#include "../utility.h"
#include "../api.h"
#include "../log.h"
#include "../tiny_queue.h"
#include "../global.h"
#include "mpd_client_utility.h"
#include "mpd_client_browse.h"
#include "mpd_client_cover.h" 
#include "mpd_client_features.h"
#include "mpd_client_jukebox.h"
#include "mpd_client_playlists.h"
#include "mpd_client_queue.h"
#include "mpd_client_search.h"
#include "mpd_client_state.h"
#include "mpd_client_stats.h"
#include "mpd_client_settings.h"
#include "mpd_client_sticker.h"
#include "mpd_client_timer.h"
#include "mpd_client_mounts.h"
#include "mpd_client_api.h"

void mpd_client_api(t_config *config, t_mpd_state *mpd_state, void *arg_request) {
    t_work_request *request = (t_work_request*) arg_request;
    unsigned int uint_buf1;
    unsigned int uint_buf2;
    int je;
    int int_buf1;
    int int_buf2; 
    bool bool_buf;
    bool rc;
    float float_buf;
    char *p_charbuf1 = NULL;
    char *p_charbuf2 = NULL;
    char *p_charbuf3 = NULL;
    char *p_charbuf4 = NULL;
    char *p_charbuf5 = NULL;

    LOG_VERBOSE("MPD CLIENT API request (%d)(%d) %s: %s", request->conn_id, request->id, request->method, request->data);
    //create response struct
    t_work_result *response = create_result(request);
    
    switch(request->cmd_id) {
        case MPD_API_LOVE:
            if (mpd_run_send_message(mpd_state->conn, mpd_state->love_channel, mpd_state->love_message) == true) {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Scrobbled love", false);
            }
            else {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Failed to send love message to channel", true);
            }
            check_error_and_recover2(mpd_state, &response->data, request->method, request->id, false);
        break;
        case MPD_API_LIKE:
            if (mpd_state->feat_sticker) {
                je = json_scanf(request->data, sdslen(request->data), "{params: {uri: %Q, like: %d}}", &p_charbuf1, &uint_buf1);
                if (je == 2 && strlen(p_charbuf1) > 0) {        
                    response->data = mpd_client_like_song_uri(mpd_state, response->data, request->method, request->id, p_charbuf1, uint_buf1);
                }
            } 
            else {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "MPD stickers are disabled", true);
                LOG_ERROR("MPD stickers are disabled");
            }
            break;
        case MPD_API_PLAYER_STATE:
            response->data = mpd_client_put_state(config, mpd_state, response->data, request->method, request->id);
            break;
        case MYMPD_API_SETTINGS_SET: {
            void *h = NULL;
            struct json_token key;
            struct json_token val;
            rc = true;
            bool mpd_host_changed = false;
            bool jukebox_changed = false;
            bool check_mpd_error = false;
            sds notify_buffer = sdsempty();
            while ((h = json_next_key(request->data, sdslen(request->data), h, ".params", &key, &val)) != NULL) {
                rc = mpd_api_settings_set(config, mpd_state, &key, &val, &mpd_host_changed, &jukebox_changed, &check_mpd_error);
                if ((check_mpd_error == true && check_error_and_recover2(mpd_state, &notify_buffer, request->method, request->id, true) == false)
                    || rc == false)
                {
                    if (sdslen(notify_buffer) > 0) {
                        ws_notify(notify_buffer);
                    }
                    break;
                }
            }
            sdsfree(notify_buffer);
            if (rc == true) {
                if (mpd_host_changed == true) {
                    //reconnect with new settings
                    mpd_state->conn_state = MPD_DISCONNECT;
                }
                if (mpd_state->conn_state == MPD_CONNECTED) {
                    //feature detection
                    mpd_client_mpd_features(config, mpd_state);
                    
                    if (jukebox_changed == true) {
                        list_free(&mpd_state->jukebox_queue);
                    }
                    if (mpd_state->jukebox_mode != JUKEBOX_OFF) {
                        //enable jukebox
                        mpd_client_jukebox(config, mpd_state);
                    }
                }
                response->data = jsonrpc_respond_ok(response->data, request->method, request->id);
            }
            else {
                response->data = jsonrpc_start_phrase(response->data, request->method, request->id, "Can't save setting %{setting}", true);
                response->data = tojson_char_len(response->data, "setting", val.ptr, val.len, false);
                response->data = jsonrpc_end_phrase(response->data);
            }
            break;
        }
        case MPD_API_DATABASE_UPDATE:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri: %Q}}", &p_charbuf1);
            if (je == 1) {
                if (strcmp(p_charbuf1, "") == 0) {
                    FREE_PTR(p_charbuf1);
                }
                uint_buf1 = mpd_run_update(mpd_state->conn, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, (uint_buf1 == 0 ? false : true), "mpd_run_update");
            }
            break;
        case MPD_API_DATABASE_RESCAN:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri: %Q}}", &p_charbuf1);
            if (je == 1) {
                if (strcmp(p_charbuf1, "") == 0) {
                    FREE_PTR(p_charbuf1);
                }
                uint_buf1 = mpd_run_rescan(mpd_state->conn, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, (uint_buf1 == 0 ? false : true), "mpd_run_rescan");
            }
            break;
        case MPD_API_SMARTPLS_UPDATE_ALL:
            rc = mpd_client_smartpls_update_all(config, mpd_state);
            if (rc == true) {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Smart playlists updated", false);
            }
            else {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Smart playlists update failed", true);
            }
            break;
        case MPD_API_SMARTPLS_UPDATE:
            je = json_scanf(request->data, sdslen(request->data), "{params: {playlist: %Q}}", &p_charbuf1);
            if (je == 1) {
                rc = mpd_client_smartpls_update(config, mpd_state, p_charbuf1);
                if (rc == true) {
                    response->data = jsonrpc_start_phrase(response->data, request->method, request->id, "Smart playlist %{playlist} updated", false);
                    response->data = tojson_char(response->data, "playlist", p_charbuf1, false);
                    response->data = jsonrpc_end_phrase(response->data);
                }
                else {
                    response->data = jsonrpc_start_phrase(response->data, request->method, request->id, "Updating of smart playlist %{playlist} failed", true);
                    response->data = tojson_char(response->data, "playlist", p_charbuf1, false);
                    response->data = jsonrpc_end_phrase(response->data);
                }
            }
            break;
        case MPD_API_SMARTPLS_SAVE:
            je = json_scanf(request->data, sdslen(request->data), "{params: {type: %Q}}", &p_charbuf1);
            rc = false;
            if (je == 1) {
                if (strcmp(p_charbuf1, "sticker") == 0) {
                    je = json_scanf(request->data, sdslen(request->data), "{params: {playlist: %Q, sticker: %Q, maxentries: %d, minvalue: %d, sort: %Q}}", &p_charbuf2, &p_charbuf3, &int_buf1, &int_buf2, &p_charbuf5);
                    if (je == 5) {
                        rc = mpd_client_smartpls_save(config, mpd_state, p_charbuf1, p_charbuf2, p_charbuf3, NULL, int_buf1, int_buf2, p_charbuf5);
                    }
                }
                else if (strcmp(p_charbuf1, "newest") == 0) {
                    je = json_scanf(request->data, sdslen(request->data), "{params: {playlist: %Q, timerange: %d, sort: %Q}}", &p_charbuf2, &int_buf1, &p_charbuf5);
                    if (je == 3) {
                        rc = mpd_client_smartpls_save(config, mpd_state, p_charbuf1, p_charbuf2, NULL, NULL, 0, int_buf1, p_charbuf5);
                    }
                }            
                else if (strcmp(p_charbuf1, "search") == 0) {
                    je = json_scanf(request->data, sdslen(request->data), "{params: {playlist: %Q, tag: %Q, searchstr: %Q, sort: %Q}}", &p_charbuf2, &p_charbuf3, &p_charbuf4, &p_charbuf5);
                    if (je == 4) {
                        rc = mpd_client_smartpls_save(config, mpd_state, p_charbuf1, p_charbuf2, p_charbuf3, p_charbuf4, 0, 0, p_charbuf5);
                    }
                }
            }
            if (rc == true) {
                response->data = jsonrpc_respond_ok(response->data, request->method, request->id);
                mpd_client_smartpls_update(config, mpd_state, p_charbuf2);
            }
            else {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Failed to save playlist", true);
            }
            break;
        case MPD_API_SMARTPLS_GET:
            je = json_scanf(request->data, sdslen(request->data), "{params: {playlist: %Q}}", &p_charbuf1);
            if (je == 1) {
                response->data = mpd_client_smartpls_put(config, response->data, request->method, request->id, p_charbuf1);
            }
            break;
        case MPD_API_PLAYER_PAUSE:
            rc = mpd_run_toggle_pause(mpd_state->conn);
            response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_toggle_pause");
            break;
        case MPD_API_PLAYER_PREV:
            rc = mpd_run_previous(mpd_state->conn);
            response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_previous");
            break;
        case MPD_API_PLAYER_NEXT:
            rc = mpd_run_next(mpd_state->conn);
            response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_next");
            break;
        case MPD_API_PLAYER_PLAY:
            rc = mpd_run_play(mpd_state->conn);
            response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_play");
            break;
        case MPD_API_PLAYER_STOP:
            rc = mpd_run_stop(mpd_state->conn);
            response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_stop");
            break;
        case MPD_API_QUEUE_CLEAR:
            rc = mpd_run_clear(mpd_state->conn);
            response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_clear");
            break;
        case MPD_API_QUEUE_CROP:
            response->data = mpd_client_crop_queue(mpd_state, response->data, request->method, request->id, false);
            break;
        case MPD_API_QUEUE_CROP_OR_CLEAR:
            response->data = mpd_client_crop_queue(mpd_state, response->data, request->method, request->id, true);
            break;
        case MPD_API_QUEUE_RM_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {track:%u}}", &uint_buf1);
            if (je == 1) {
                rc = mpd_run_delete_id(mpd_state->conn, uint_buf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_delete_id");
            }
            break;
        case MPD_API_QUEUE_RM_RANGE:
            je = json_scanf(request->data, sdslen(request->data), "{params: {start: %u, end: %u}}", &uint_buf1, &uint_buf2);
            if (je == 2) {
                rc = mpd_run_delete_range(mpd_state->conn, uint_buf1, uint_buf2);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_delete_range");
            }
            break;
        case MPD_API_QUEUE_MOVE_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {from: %u, to: %u}}", &uint_buf1, &uint_buf2);
            if (je == 2) {
                uint_buf1--;
                uint_buf2--;
                if (uint_buf1 < uint_buf2) {
                    uint_buf2--;
                }
                rc = mpd_run_move(mpd_state->conn, uint_buf1, uint_buf2);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_move");
            }
            break;
        case MPD_API_PLAYLIST_MOVE_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {plist: %Q, from: %u, to: %u }}", &p_charbuf1, &uint_buf1, &uint_buf2);
            if (je == 3) {
                uint_buf1--;
                uint_buf2--;
                if (uint_buf1 < uint_buf2) {
                    uint_buf2--;
                }
                rc = mpd_run_playlist_move(mpd_state->conn, p_charbuf1, uint_buf1, uint_buf2);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_playlist_move");
            }
            break;
        case MPD_API_PLAYER_PLAY_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: { track:%u}}", &uint_buf1);
            if (je == 1) {
                rc = mpd_run_play_id(mpd_state->conn, uint_buf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_play_id");
            }
            break;
        case MPD_API_PLAYER_OUTPUT_LIST:
            response->data = mpd_client_put_outputs(mpd_state, response->data, request->method, request->id);
            break;
        case MPD_API_PLAYER_TOGGLE_OUTPUT:
            je = json_scanf(request->data, sdslen(request->data), "{params: {output: %u, state: %u}}", &uint_buf1, &uint_buf2);
            if (je == 2) {
                if (uint_buf2 == 1) {
                    rc = mpd_run_enable_output(mpd_state->conn, uint_buf1);
                    response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_enable_output");
                }
                else {
                    rc = mpd_run_disable_output(mpd_state->conn, uint_buf1);
                    response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_disable_output");
                }
            }
            break;
        case MPD_API_PLAYER_VOLUME_SET:
            je = json_scanf(request->data, sdslen(request->data), "{params: {volume:%u}}", &uint_buf1);
            if (je == 1) {
                rc = mpd_run_set_volume(mpd_state->conn, uint_buf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_set_volume");
            }
            break;
        case MPD_API_PLAYER_VOLUME_GET:
            response->data = mpd_client_put_volume(mpd_state, response->data, request->method, request->id);
            break;            
        case MPD_API_PLAYER_SEEK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {songid: %u, seek: %u}}", &uint_buf1, &uint_buf2);
            if (je == 2) {
                rc = mpd_run_seek_id(mpd_state->conn, uint_buf1, uint_buf2);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_seek_id");
            }
            break;
        case MPD_API_PLAYER_SEEK_CURRENT:
            je = json_scanf(request->data, sdslen(request->data), "{params: {seek: %f, relative: %B}}", &float_buf, &bool_buf);
            if (je == 2) {
                rc = mpd_run_seek_current(mpd_state->conn, float_buf, bool_buf);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_seek_current");
            }
            break;
        case MPD_API_QUEUE_LIST: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset: %u, cols: %M}}", &uint_buf1, json_to_tags, tagcols);
            if (je == 2) {
                response->data = mpd_client_put_queue(mpd_state, response->data, request->method, request->id, uint_buf1, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_QUEUE_LAST_PLAYED: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset: %u, cols: %M}}", &uint_buf1, json_to_tags, tagcols);
            if (je == 2) {
                response->data = mpd_client_put_last_played_songs(config, mpd_state, response->data, request->method, request->id, uint_buf1, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_PLAYER_CURRENT_SONG: {
            response->data = mpd_client_put_current_song(mpd_state, response->data, request->method, request->id);
            break;
        }
        case MPD_API_DATABASE_SONGDETAILS:
            je = json_scanf(request->data, sdslen(request->data), "{params: { uri: %Q}}", &p_charbuf1);
            if (je == 1 && strlen(p_charbuf1) > 0) {
                response->data = mpd_client_put_songdetails(mpd_state, response->data, request->method, request->id, p_charbuf1);
            }
            else {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Invalid API request", true);
            }
            break;
        case MPD_API_DATABASE_FINGERPRINT:
            if (mpd_state->feat_fingerprint == true) {
                je = json_scanf(request->data, sdslen(request->data), "{params: { uri: %Q}}", &p_charbuf1);
                if (je == 1 && strlen(p_charbuf1) > 0) {
                    response->data = mpd_client_put_fingerprint(mpd_state, response->data, request->method, request->id, p_charbuf1);
                }
            }
            else {
                response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Fingerprint command not supported", true);
            }
            break;
        case MPD_API_DATABASE_TAG_LIST:
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset: %u, filter: %Q, tag: %Q}}", &uint_buf1, &p_charbuf1, &p_charbuf2);
            if (je == 3) {
                response->data = mpd_client_put_db_tag(mpd_state, response->data, request->method, request->id, uint_buf1, p_charbuf2, "", "", p_charbuf1);
            }
            break;
        case MPD_API_DATABASE_TAG_ALBUM_LIST:
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset: %u, filter: %Q, search: %Q, tag: %Q}}", 
                &uint_buf1, &p_charbuf1, &p_charbuf2, &p_charbuf3);
            if (je == 4) {
                response->data = mpd_client_put_db_tag(mpd_state, response->data, request->method, request->id, uint_buf1, "Album", p_charbuf3, p_charbuf2, p_charbuf1);
            }
            break;
        case MPD_API_DATABASE_TAG_ALBUM_TITLE_LIST: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {album: %Q, search: %Q, tag: %Q, cols: %M}}", 
                &p_charbuf1, &p_charbuf2, &p_charbuf3, json_to_tags, tagcols);
            if (je == 4) {
                response->data = mpd_client_put_songs_in_album(mpd_state, response->data, request->method, request->id, p_charbuf1, p_charbuf2, p_charbuf3, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_PLAYLIST_RENAME:
            je = json_scanf(request->data, sdslen(request->data), "{params: {from: %Q, to: %Q}}", &p_charbuf1, &p_charbuf2);
            if (je == 2) {
                response->data = mpd_client_playlist_rename(config, mpd_state, response->data, request->method, request->id, p_charbuf1, p_charbuf2);
            }
            break;            
        case MPD_API_PLAYLIST_LIST:
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset: %u, filter: %Q}}", &uint_buf1, &p_charbuf1);
            if (je == 2) {
                response->data = mpd_client_put_playlists(config, mpd_state, response->data, request->method, request->id, uint_buf1, p_charbuf1);
            }
            break;
        case MPD_API_PLAYLIST_CONTENT_LIST: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri: %Q, offset:%u, filter:%Q, cols: %M}}", 
                &p_charbuf1, &uint_buf1, &p_charbuf2, json_to_tags, tagcols);
            if (je == 4) {
                response->data = mpd_client_put_playlist_list(config, mpd_state, response->data, request->method, request->id, p_charbuf1, uint_buf1, p_charbuf2, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_PLAYLIST_ADD_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {plist: %Q, uri: %Q}}", &p_charbuf1, &p_charbuf2);
            if (je == 2) {
                rc = mpd_run_playlist_add(mpd_state->conn, p_charbuf1, p_charbuf2);
                if (check_error_and_recover2(mpd_state, &response->data, request->method, request->id, false) == true && rc == true) {
                    response->data = jsonrpc_start_phrase(response->data, request->method, request->id, "Added %{uri} to playlist %{playlist}", false);
                    response->data = tojson_char(response->data, "uri", p_charbuf2, true);
                    response->data = tojson_char(response->data, "playlist", p_charbuf1, false);
                    response->data = jsonrpc_end_phrase(response->data);
                }
                else if (rc == false) {
                    response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_playlist_add");
                }
            }
            break;
        case MPD_API_PLAYLIST_CLEAR:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri: %Q}}", &p_charbuf1);
            if (je == 1) {
                rc = mpd_run_playlist_clear(mpd_state->conn, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_playlist_clear");
            }
            break;
        case MPD_API_PLAYLIST_RM_ALL:
            je = json_scanf(request->data, sdslen(request->data), "{params: {type: %Q}}", &p_charbuf1);
            if (je == 1) {
                response->data = mpd_client_playlist_delete_all(config, mpd_state, response->data, request->method, request->id, p_charbuf1);
            }
            break;
        case MPD_API_PLAYLIST_RM_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri:%Q, track:%u}}", &p_charbuf1, &uint_buf1);
            if (je == 2) {
                rc = mpd_run_playlist_delete(mpd_state->conn, p_charbuf1, uint_buf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_playlist_delete");
            }
            break;
        case MPD_API_PLAYLIST_SHUFFLE:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri: %Q}}", &p_charbuf1);
            if (je == 1) {
                response->data = mpd_client_playlist_shuffle_sort(mpd_state, response->data, request->method, request->id, p_charbuf1, "shuffle");
            }
            break;
        case MPD_API_PLAYLIST_SORT:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri: %Q, tag:%Q}}", &p_charbuf1, &p_charbuf2);
            if (je == 2) {
                response->data = mpd_client_playlist_shuffle_sort(mpd_state, response->data, request->method, request->id, p_charbuf1, p_charbuf2);
            }
            break;
        case MPD_API_DATABASE_FILESYSTEM_LIST: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset:%u, filter:%Q, path:%Q, cols: %M}}", 
                &uint_buf1, &p_charbuf1, &p_charbuf2, json_to_tags, tagcols);
            if (je == 4) {
                response->data = mpd_client_put_filesystem(config, mpd_state, response->data, request->method, request->id, p_charbuf2, uint_buf1, p_charbuf1, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_QUEUE_ADD_TRACK_AFTER:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri:%Q, to:%d}}", &p_charbuf1, &int_buf1);
            if (je == 2) {
                rc = mpd_run_add_id_to(mpd_state->conn, p_charbuf1, int_buf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_add_id_to");
            }
            break;
        case MPD_API_QUEUE_REPLACE_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri:%Q }}", &p_charbuf1);
            if (je == 1 && strlen(p_charbuf1) > 0) {
                rc = mpd_client_queue_replace_with_song(mpd_state, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_client_queue_replace_with_song");
            }
            break;
        case MPD_API_QUEUE_ADD_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri:%Q}}", &p_charbuf1);
            if (je == 1 && strlen(p_charbuf1) > 0) {
                rc = mpd_run_add(mpd_state->conn, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_add");
            }
            break;
        case MPD_API_QUEUE_ADD_PLAY_TRACK:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri:%Q}}", &p_charbuf1);
            if (je == 1) {
                int_buf1 = mpd_run_add_id(mpd_state->conn, p_charbuf1);
                if (int_buf1 != -1) {
                    rc = mpd_run_play_id(mpd_state->conn, int_buf1);
                }
                else {
                    rc = false;
                }
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_add_id");
            }
            break;
        case MPD_API_QUEUE_REPLACE_PLAYLIST:
            je = json_scanf(request->data, sdslen(request->data), "{params: {plist:%Q}}", &p_charbuf1);
            if (je == 1) {
                rc = mpd_client_queue_replace_with_playlist(mpd_state, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_client_queue_replace_with_playlist");
            }
            break;
        case MPD_API_QUEUE_ADD_RANDOM:
            je = json_scanf(request->data, sdslen(request->data), "{params: {mode:%u, playlist:%Q, quantity:%d}}", &uint_buf1, &p_charbuf1, &int_buf1);
            if (je == 3) {
                rc = mpd_client_jukebox_add_to_queue(config, mpd_state, int_buf1, uint_buf1, p_charbuf1, true);
                if (rc == true) {
                    response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Sucessfully added random songs to queue", false);
                }
                else {
                    response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Adding random songs to queue failed", true);
                }
            }
            break;
        case MPD_API_QUEUE_ADD_PLAYLIST:
            je = json_scanf(request->data, sdslen(request->data), "{params: {plist:%Q}}", &p_charbuf1);
            if (je == 1) {
                rc = mpd_run_load(mpd_state->conn, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_load");
            }
            break;
        case MPD_API_QUEUE_SAVE:
            je = json_scanf(request->data, sdslen(request->data), "{ params: {plist:%Q}}", &p_charbuf1);
            if (je == 1) {
                rc = mpd_run_save(mpd_state->conn, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_save");
            }
            break;
        case MPD_API_QUEUE_SEARCH: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset:%u, filter:%Q, searchstr:%Q, cols: %M}}", 
                &uint_buf1, &p_charbuf1, &p_charbuf2, json_to_tags, tagcols);
            if (je == 4) {
                response->data = mpd_client_search_queue(mpd_state, response->data, request->method, request->id, p_charbuf1, uint_buf1, p_charbuf2, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_DATABASE_SEARCH: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {searchstr:%Q, filter:%Q, plist:%Q, offset:%u, cols: %M, replace:%B}}", 
                &p_charbuf1, &p_charbuf2, &p_charbuf3, &uint_buf1, json_to_tags, tagcols, &bool_buf);
            if (je == 6) {
                if (bool_buf == true) {
                    rc = mpd_run_clear(mpd_state->conn);
                    if (rc == false) {
                        LOG_ERROR("Clearing queue failed");
                    }
                    check_error_and_recover(mpd_state, NULL, NULL, 0);
                }
                response->data = mpd_client_search(mpd_state, response->data, request->method, request->id, p_charbuf1, p_charbuf2, p_charbuf3, uint_buf1, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_DATABASE_SEARCH_ADV: {
            t_tags *tagcols = (t_tags *)malloc(sizeof(t_tags));
            assert(tagcols);
            je = json_scanf(request->data, sdslen(request->data), "{params: {expression:%Q, sort:%Q, sortdesc:%B, plist:%Q, offset:%u, cols: %M, replace:%B}}", 
                &p_charbuf1, &p_charbuf2, &bool_buf, &p_charbuf3, &uint_buf1, json_to_tags, tagcols, &bool_buf);
            if (je == 7) {
                if (bool_buf == true) {
                    rc = mpd_run_clear(mpd_state->conn);
                    if (rc == false) {
                        LOG_ERROR("Clearing queue failed");
                    }
                    check_error_and_recover(mpd_state, NULL, NULL, 0);
                }
                response->data = mpd_client_search_adv(mpd_state, response->data, request->method, request->id, p_charbuf1, p_charbuf2, bool_buf, NULL, p_charbuf3, uint_buf1, tagcols);
            }
            free(tagcols);
            break;
        }
        case MPD_API_QUEUE_SHUFFLE:
            rc = mpd_run_shuffle(mpd_state->conn);
            response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_shuffle");
            break;
        case MPD_API_PLAYLIST_RM:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri:%Q}}", &p_charbuf1);
            if (je == 1) {
                response->data = mpd_client_playlist_delete(config, mpd_state, response->data, request->method, request->id, p_charbuf1);
            }
            break;
        case MPD_API_SETTINGS_GET:
            response->data = mpd_client_put_settings(mpd_state, response->data, request->method, request->id);
            break;
        case MPD_API_DATABASE_STATS:
            response->data = mpd_client_put_stats(config, mpd_state, response->data, request->method, request->id);
            break;
        case MPD_API_ALBUMART:
            je = json_scanf(request->data, sdslen(request->data), "{params: {uri:%Q}}", &p_charbuf1);
            if (je == 1) {
                response->data = mpd_client_getcover(config, mpd_state, response->data, request->method, request->id, p_charbuf1, &response->binary);
            }
            break;
        case MPD_API_DATABASE_GET_ALBUMS:
            je = json_scanf(request->data, sdslen(request->data), "{params: {offset:%u, searchstr:%Q, tag:%Q, sort:%Q, sortdesc:%B}}", 
                &uint_buf1, &p_charbuf1, &p_charbuf2, &p_charbuf3, &bool_buf);
            if (je == 5) {
                response->data = mpd_client_put_firstsong_in_albums(config, mpd_state, response->data, request->method, request->id, 
                    p_charbuf1, p_charbuf2, p_charbuf3, bool_buf, uint_buf1);
            }
            break;
        case MPD_API_TIMER_STARTPLAY:
            je = json_scanf(request->data, sdslen(request->data), "{params: {volume:%u, playlist:%Q, jukeboxMode:%u}}", &uint_buf1, &p_charbuf1, &uint_buf2);
            if (je == 3) {
                response->data = mpd_client_timer_startplay(mpd_state, response->data, request->method, request->id, uint_buf1, p_charbuf1, uint_buf2);
            }
            break;
        case MPD_API_URLHANDLERS:
            response->data = mpd_client_put_urlhandlers(mpd_state, response->data, request->method, request->id);
            break;
        case MPD_API_MOUNT_LIST:
            response->data = mpd_client_put_mounts(mpd_state, response->data, request->method, request->id);
            break;
        case MPD_API_MOUNT_NEIGHBOR_LIST:
            response->data = mpd_client_put_neighbors(mpd_state, response->data, request->method, request->id);
            break;
        case MPD_API_MOUNT_MOUNT:
            je = json_scanf(request->data, sdslen(request->data), "{params: {mountUrl: %Q, mountPoint: %Q}}", &p_charbuf1, &p_charbuf2);
            if (je == 2) {
                rc = mpd_run_mount(mpd_state->conn, p_charbuf2, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_mount");
            }
            break;
        case MPD_API_MOUNT_UNMOUNT:
            je = json_scanf(request->data, sdslen(request->data), "{params: {mountPoint: %Q}}", &p_charbuf1);
            if (je == 1) {
                rc = mpd_run_unmount(mpd_state->conn, p_charbuf1);
                response->data = respond_with_mpd_error_or_ok(mpd_state, response->data, request->method, request->id, rc, "mpd_run_unmount");
            }
            break;
        default:
            response->data = jsonrpc_respond_message(response->data, request->method, request->id, "Unknown request", true);
            LOG_ERROR("Unknown API request: %.*s", sdslen(request->data), request->data);
    }
    FREE_PTR(p_charbuf1);
    FREE_PTR(p_charbuf2);
    FREE_PTR(p_charbuf3);                    
    FREE_PTR(p_charbuf4);
    FREE_PTR(p_charbuf5);

    if (sdslen(response->data) == 0) {
        response->data = jsonrpc_start_phrase(response->data, request->method, request->id, "No response for method %{method}", true);
        response->data = tojson_char(response->data, "method", request->method, false);
        response->data = jsonrpc_end_phrase(response->data);
        LOG_ERROR("No response for cmd_id %u", request->cmd_id);
    }
    if (request->conn_id > -1) {
        LOG_DEBUG("Push response to queue for connection %lu: %s", request->conn_id, response->data);
        tiny_queue_push(web_server_queue, response);
    }
    else {
        free_result(response);
    }
    free_request(request);
}
