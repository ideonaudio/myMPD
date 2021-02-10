/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <string.h>
#include <mpd/client.h>

#include "../../dist/src/sds/sds.h"
#include "../sds_extras.h"
#include "../api.h"
#include "../list.h"
#include "config_defs.h"
#include "../utility.h"
#include "../log.h"
#include "../mpd_shared/mpd_shared_typedefs.h"
#include "../mpd_shared/mpd_shared_tags.h"
#include "../mpd_shared.h"
#include "../random.h"
#include "mpd_shared_playlists.h"

unsigned long mpd_shared_get_db_mtime(t_mpd_state *mpd_state) {
    struct mpd_stats *stats = mpd_run_stats(mpd_state->conn);
    if (stats == NULL) {
        check_error_and_recover(mpd_state, NULL, NULL, 0);
        return 0;
    }
    unsigned long mtime = mpd_stats_get_db_update_time(stats);
    mpd_stats_free(stats);
    return mtime;
}

unsigned long mpd_shared_get_smartpls_mtime(t_config *config, const char *playlist) {
    sds plpath = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, playlist);
    struct stat attr;
    if (stat(plpath, &attr) != 0) {
        LOG_ERROR("Error getting mtime for %s: %s", plpath, strerror(errno));
        sdsfree(plpath);
        return 0;
    }
    sdsfree(plpath);
    return attr.st_mtime;
}

unsigned long mpd_shared_get_playlist_mtime(t_mpd_state *mpd_state, const char *playlist) {
    bool rc = mpd_send_list_playlists(mpd_state->conn);
    if (check_rc_error_and_recover(mpd_state, NULL, NULL, 0, false, rc, "mpd_send_list_playlists") == false) {
        return 0;
    }
    unsigned long mtime = 0;
    struct mpd_playlist *pl;
    while ((pl = mpd_recv_playlist(mpd_state->conn)) != NULL) {
        const char *plpath = mpd_playlist_get_path(pl);
        if (strcmp(plpath, playlist) == 0) {
            mtime = mpd_playlist_get_last_modified(pl);
            mpd_playlist_free(pl);
            break;
        }
        mpd_playlist_free(pl);
    }
    mpd_response_finish(mpd_state->conn);
    if (check_error_and_recover2(mpd_state, NULL, NULL, 0, false) == false) {
        return 0;
    }

    return mtime;
}

sds mpd_shared_playlist_shuffle_sort(t_mpd_state *mpd_state, sds buffer, sds method, long request_id, const char *uri, const char *tagstr) {
    t_tags sort_tags;
    
    sort_tags.len = 1;
    sort_tags.tags[0] = mpd_tag_name_parse(tagstr);

    bool rc = false;
    
    if (strcmp(tagstr, "shuffle") == 0) {
        LOG_VERBOSE("Shuffling playlist %s", uri);
        rc = mpd_send_list_playlist(mpd_state->conn, uri);
    }
    else if (strcmp(tagstr, "filename") == 0) {
        LOG_VERBOSE("Sorting playlist %s by filename", uri);
        rc = mpd_send_list_playlist(mpd_state->conn, uri);
    } 
    else if (sort_tags.tags[0] != MPD_TAG_UNKNOWN) {
        LOG_VERBOSE("Sorting playlist %s by tag %s", uri, tagstr);
        enable_mpd_tags(mpd_state, sort_tags);
        rc = mpd_send_list_playlist_meta(mpd_state->conn, uri);
    }
    else {
        if (buffer != NULL) {
            buffer = jsonrpc_respond_message(buffer, method, request_id, "Leaving playlist as it is", true);
        }
        return buffer;
    }
    if (check_rc_error_and_recover(mpd_state, &buffer, method, request_id, false, rc, "mpd_send_list_playlist") == false) {
        return buffer;
    }

    struct list plist;
    list_init(&plist);
    struct mpd_song *song;
    while ((song = mpd_recv_song(mpd_state->conn)) != NULL) {
        const char *tag_value = NULL;
        if (sort_tags.tags[0] != MPD_TAG_UNKNOWN) {
            tag_value = mpd_song_get_tag(song, sort_tags.tags[0], 0);
        }
        list_push(&plist, mpd_song_get_uri(song), 0, tag_value, NULL);
        mpd_song_free(song);
    }
    mpd_response_finish(mpd_state->conn);
    if (check_error_and_recover2(mpd_state, &buffer, method, request_id, false) == false) {
        list_free(&plist);
        return buffer;
    }
    if (sort_tags.tags[0] == MPD_TAG_UNKNOWN) {
        if (list_shuffle(&plist) == false) {
            if (buffer != NULL) {
                buffer = jsonrpc_respond_message(buffer, method, request_id, "Playlist is too small to shuffle", true);
            }
            list_free(&plist);
            enable_mpd_tags(mpd_state, mpd_state->mympd_tag_types);
            return buffer;
        }
    }
    else {
        if (mpd_state->feat_tags == false || strcmp(tagstr, "filename") == 0) {
            if (list_sort_by_key(&plist, true) == false) {
                if (buffer != NULL) {
                    buffer = jsonrpc_respond_message(buffer, method, request_id, "Playlist is too small to sort", true);
                }
                list_free(&plist);
                enable_mpd_tags(mpd_state, mpd_state->mympd_tag_types);
                return buffer;
            }
        }
        else {
            if (list_sort_by_value_p(&plist, true) == false) {
                if (buffer != NULL) {
                    buffer = jsonrpc_respond_message(buffer, method, request_id, "Playlist is too small to sort", true);
                }
                list_free(&plist);
                enable_mpd_tags(mpd_state, mpd_state->mympd_tag_types);
                return buffer;
            }
        }
    }
    
    unsigned int randnr = randrange(100000,999999);
    sds uri_tmp = sdscatprintf(sdsempty(), "%u-tmp-%s", randnr, uri);
    sds uri_old = sdscatprintf(sdsempty(), "%u-old-%s", randnr, uri);
    
    //add sorted/shuffled songs to a new playlist
    if (mpd_command_list_begin(mpd_state->conn, false) == true) {
        struct list_node *current = plist.head;
        while (current != NULL) {
            rc = mpd_send_playlist_add(mpd_state->conn, uri_tmp, current->key);
            if (rc == false) {
                LOG_ERROR("Error adding command to command list mpd_send_playlist_add");
                break;
            }
            current = current->next;
        }
        if (mpd_command_list_end(mpd_state->conn)) {
            mpd_response_finish(mpd_state->conn);
        }
    }
    list_free(&plist);
    if (check_error_and_recover2(mpd_state, &buffer, method, request_id, false) == false) {
        rc = mpd_run_rm(mpd_state->conn, uri_tmp);
        check_rc_error_and_recover(mpd_state, NULL, method, request_id, false, rc, "mpd_run_rm");
        sdsfree(uri_tmp);
        sdsfree(uri_old);
        return buffer;
    }

    //rename original playlist to old playlist
    rc = mpd_run_rename(mpd_state->conn, uri, uri_old);
    if (check_rc_error_and_recover(mpd_state, &buffer, method, request_id, false, rc, "mpd_run_rename") == false) {
        sdsfree(uri_tmp);
        sdsfree(uri_old);
        return buffer;
    }
    //rename new playlist to orginal playlist
    rc = mpd_run_rename(mpd_state->conn, uri_tmp, uri);
    if (check_rc_error_and_recover(mpd_state, &buffer, method, request_id, false, rc, "mpd_run_rename") == false) {
        //restore original playlist
        rc = mpd_run_rename(mpd_state->conn, uri_old, uri);
        check_rc_error_and_recover(mpd_state, NULL, method, request_id, false, rc, "mpd_run_rename");
        sdsfree(uri_tmp);
        sdsfree(uri_old);
        return buffer;
    }
    //delete old playlist
    rc = mpd_run_rm(mpd_state->conn, uri_old);
    if (check_rc_error_and_recover(mpd_state, &buffer, method, request_id, false, rc, "mpd_run_rm") == false) {
        sdsfree(uri_tmp);
        sdsfree(uri_old);
        return buffer;
    }
    
    sdsfree(uri_tmp);
    sdsfree(uri_old);
    
    if (sort_tags.tags[0] != MPD_TAG_UNKNOWN) {
        enable_mpd_tags(mpd_state, mpd_state->mympd_tag_types);
    }
    if (buffer != NULL) {
        if (strcmp(tagstr, "shuffle") == 0) {
            buffer = jsonrpc_respond_message(buffer, method, request_id, "Shuffled playlist succesfully", false);
        }
        else {
            buffer = jsonrpc_respond_message(buffer, method, request_id, "Sorted playlist succesfully", false);
        }
    }
    return buffer;
}

bool mpd_shared_smartpls_save(t_config *config, const char *smartpltype, const char *playlist, 
                              const char *tag, const char *searchstr, const int maxentries, const int timerange, const char *sort)
{
    if (validate_string_not_dir(playlist) == false) {
        return false;
    }
    
    sds tmp_file = sdscatfmt(sdsempty(), "%s/smartpls/%s.XXXXXX", config->varlibdir, playlist);
    int fd = mkstemp(tmp_file);
    if (fd < 0 ) {
        LOG_ERROR("Can not open file \"%s\" for write: %s", tmp_file, strerror(errno));
        sdsfree(tmp_file);
        return false;
    }
    FILE *fp = fdopen(fd, "w");

    sds line = sdscat(sdsempty(), "{");
    line = tojson_char(line, "type", smartpltype, true);
    if (strcmp(smartpltype, "sticker") == 0) {
        line = tojson_char(line, "sticker", tag, true);
        line = tojson_long(line, "maxentries", maxentries, true);
        line = tojson_long(line, "minvalue", timerange, true);
    }
    else if (strcmp(smartpltype, "newest") == 0) {
        line = tojson_long(line, "timerange", timerange, true);
    }
    else if (strcmp(smartpltype, "search") == 0) {
        line = tojson_char(line, "tag", tag, true);
        line = tojson_char(line, "searchstr", searchstr, true);
    }
    line = tojson_char(line, "sort", sort, false);
    line = sdscat(line, "}");
    int rc = fputs(line, fp);
    sdsfree(line);
    if (rc < 0) {
        LOG_ERROR("Can't write to file %s", tmp_file);
    }
    fclose(fp);
    sds pl_file = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, playlist);
    rc = rename(tmp_file, pl_file);
    if (rc == -1) {
        LOG_ERROR("Renaming file from %s to %s failed: %s", tmp_file, pl_file, strerror(errno));
        sdsfree(tmp_file);
        sdsfree(pl_file);
        return false;
    }
    sdsfree(tmp_file);
    sdsfree(pl_file);
    return true;
}
