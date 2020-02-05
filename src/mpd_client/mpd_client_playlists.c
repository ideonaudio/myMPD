/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <inttypes.h>
#include <ctype.h>
#include <mpd/client.h>

#include "../../dist/src/sds/sds.h"
#include "../sds_extras.h"
#include "../../dist/src/frozen/frozen.h"
#include "../api.h"
#include "../list.h"
#include "config_defs.h"
#include "../utility.h"
#include "../log.h"
#include "mpd_client_utility.h"
#include "mpd_client_search.h"
#include "mpd_client_playlists.h"

sds mpd_client_put_playlists(t_config *config, t_mpd_state *mpd_state, sds buffer, sds method, int request_id,
                             const unsigned int offset, const char *filter) 
{
    buffer = jsonrpc_start_result(buffer, method, request_id);
    buffer = sdscat(buffer,",\"data\":[");

    if (mpd_send_list_playlists(mpd_state->conn) == false) {
        buffer = check_error_and_recover(mpd_state, buffer, method, request_id);
        return buffer;
    }

    struct mpd_playlist *pl;
    unsigned entity_count = 0;
    unsigned entities_returned = 0;
    while ((pl = mpd_recv_playlist(mpd_state->conn)) != NULL) {
        entity_count++;
        if (entity_count > offset && entity_count <= offset + mpd_state->max_elements_per_page) {
            const char *plpath = mpd_playlist_get_path(pl);
            if (strncmp(filter, "-", 1) == 0 || strncasecmp(filter, plpath, 1) == 0 ||
               (strncmp(filter, "0", 1) == 0 && isalpha(*plpath) == 0 )) 
            {
                if (entities_returned++) {
                    buffer = sdscat(buffer,",");
                }
                bool smartpls = is_smartpls(config, mpd_state, plpath);
                buffer = sdscat(buffer, "{");
                buffer = tojson_char(buffer, "Type", (smartpls == true ? "smartpls" : "plist"), true);
                buffer = tojson_char(buffer, "uri", plpath, true);
                buffer = tojson_char(buffer, "name", plpath, true);
                buffer = tojson_long(buffer, "last_modified", mpd_playlist_get_last_modified(pl), false);
                buffer = sdscat(buffer, "}");
            } else {
                entity_count--;
            }
        }
        mpd_playlist_free(pl);
    }

    buffer = sdscat(buffer, "],");
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, false);
    buffer = jsonrpc_end_result(buffer);
    
    return buffer;
}

sds mpd_client_put_playlist_list(t_config *config, t_mpd_state *mpd_state, sds buffer, sds method, int request_id,
                                 const char *uri, const unsigned int offset, const char *filter, const t_tags *tagcols)
{
    mpd_send_list_playlist_meta(mpd_state->conn, uri);
    if (check_error_and_recover2(mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    
    buffer = jsonrpc_start_result(buffer, method, request_id);
    buffer = sdscat(buffer,",\"data\":[");

    struct mpd_song *song;
    unsigned entities_returned = 0;
    unsigned entity_count = 0;
    while ((song = mpd_recv_song(mpd_state->conn)) != NULL) {
        entity_count++;
        if (entity_count > offset && entity_count <= offset + mpd_state->max_elements_per_page) {
            const char *entityName = mpd_client_get_tag(song, MPD_TAG_TITLE);
            if (strncmp(filter, "-", 1) == 0 || strncasecmp(filter, entityName, 1) == 0 ||
               (strncmp(filter, "0", 1) == 0 && isalpha(*entityName) == 0))
            {
                if (entities_returned++) {
                    buffer= sdscat(buffer, ",");
                }
                buffer = sdscat(buffer, "{");
                buffer = tojson_char(buffer, "Type", "song", true);
                buffer = tojson_long(buffer, "Pos", entity_count, true);
                buffer = put_song_tags(buffer, mpd_state, tagcols, song);
                buffer = sdscat(buffer, "}");
            }
            else {
                entity_count--;
            }
        }
        mpd_song_free(song);
    }

    if (check_error_and_recover2(mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    
    bool smartpls = is_smartpls(config, mpd_state, uri);

    buffer = sdscat(buffer, "],");
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_char(buffer, "filter", filter, true);
    buffer = tojson_char(buffer, "uri", uri, true);
    buffer = tojson_bool(buffer, "smartpls", smartpls, false);
    buffer = jsonrpc_end_result(buffer);
    
    return buffer;
}

sds mpd_client_playlist_rename(t_config *config, t_mpd_state *mpd_state, sds buffer, sds method, int request_id,
                                const char *old_playlist, const char *new_playlist)
{
    if (validate_string(old_playlist) == false || validate_string(new_playlist) == false) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, "Invalid filename", true);
        return buffer;
    }
    if (mpd_state->feat_smartpls == false) {
        sds old_pl_file = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, old_playlist);
        sds new_pl_file = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, new_playlist);
        if (access(old_pl_file, F_OK ) != -1) { /* Flawfinder: ignore */
            //smart playlist
            if (access(new_pl_file, F_OK ) == -1) { /* Flawfinder: ignore */
                //new playlist doesn't exist
                if (rename(old_pl_file, new_pl_file) == -1) {
                    LOG_ERROR("Renaming smart playlist %s to %s failed", old_pl_file, new_pl_file);
                    buffer = jsonrpc_respond_message(buffer, method, request_id, "Renaming playlist failed", true);
                    sdsfree(old_pl_file);
                    sdsfree(new_pl_file);
                    return buffer;
                }
            } 
        }
        sdsfree(old_pl_file);
        sdsfree(new_pl_file);
    }
    //rename mpd playlist
    if (mpd_run_rename(mpd_state->conn, old_playlist, new_playlist)) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, "Sucessfully renamed playlist", false);
    }
    else { 
        buffer = check_error_and_recover(mpd_state, buffer, method, request_id);
    }
    return buffer;
}

sds mpd_client_playlist_delete(t_config *config, t_mpd_state *mpd_state, sds buffer, sds method, int request_id,
                               const char *playlist) {
    if (validate_string(playlist) == false) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, "Invalid filename", true);
        return buffer;
    }
    //remove smart playlist
    if (mpd_state->feat_smartpls == false) {
        sds pl_file = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, playlist);
        int rc = unlink(pl_file);
        sdsfree(pl_file);
        if (rc == -1 && errno != ENOENT) {
            buffer = jsonrpc_respond_message(buffer, method, request_id, "Deleting smart playlist failed", true);
            LOG_ERROR("Deleting smart playlist \"%s\" failed", playlist);
            return buffer;
        }
    }
    //remove mpd playlist
    if (mpd_run_rm(mpd_state->conn, playlist)) {
        buffer = jsonrpc_respond_ok(buffer, method, request_id);
    }
    else {
        buffer = check_error_and_recover(mpd_state, buffer, method, request_id);
    }
    return buffer;
}

sds mpd_client_smartpls_put(t_config *config, sds buffer, sds method, int request_id,
                            const char *playlist)
{
    if (validate_string(playlist) == false) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, "Can not read smart playlist file", true);
        return buffer;
    }
    char *p_charbuf1 = NULL;
    char *p_charbuf2 = NULL;
    int int_buf1;

    sds pl_file = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, playlist);
    char *content = json_fread(pl_file);
    sdsfree(pl_file);
    if (content == NULL) {
        LOG_ERROR("Can't read smart playlist: %s", playlist);
        buffer = jsonrpc_respond_message(buffer, method, request_id, "Can not read smart playlist file", true);
        return buffer;
    }
    char *smartpltype = NULL;
    int je = json_scanf(content, strlen(content), "{type: %Q }", &smartpltype);
    if (je == 1) {
        buffer = jsonrpc_start_result(buffer, method, request_id);
        buffer = sdscat(buffer, ",");
        buffer = tojson_char(buffer, "playlist", playlist, true);
        buffer = tojson_char(buffer, "type", smartpltype, true);
        bool rc = true;
        if (strcmp(smartpltype, "sticker") == 0) {
            je = json_scanf(content, strlen(content), "{sticker: %Q, maxentries: %d}", &p_charbuf1, &int_buf1);
            if (je == 2) {
                buffer = tojson_char(buffer, "sticker", p_charbuf1, true);
                buffer = tojson_long(buffer, "maxentries", int_buf1, false);
            } else {
                rc = false;
            }
            FREE_PTR(p_charbuf1);
        }
        else if (strcmp(smartpltype, "newest") == 0) {
            je = json_scanf(content, strlen(content), "{timerange: %d}", &int_buf1);
            if (je == 1) {
                buffer = tojson_long(buffer, "timerange", int_buf1, false);
            } else {
                rc = false;
            }
        }
        else if (strcmp(smartpltype, "search") == 0) {
            je = json_scanf(content, strlen(content), "{tag: %Q, searchstr: %Q}", &p_charbuf1, &p_charbuf2);
            if (je == 2) {
                buffer = tojson_char(buffer, "tag", p_charbuf1, true);
                buffer = tojson_char(buffer, "searchstr", p_charbuf2, false);
            } else {
                rc = false;    
            }
            FREE_PTR(p_charbuf1);
            FREE_PTR(p_charbuf2);
        }
        else {
            rc = false;            
        }
        if (rc == true) {
            buffer = jsonrpc_end_result(buffer);
        }
        else {
            buffer = jsonrpc_respond_message(buffer, method, request_id, "Can not parse smart playlist file", true);
            LOG_ERROR("Can't parse smart playlist file: %s", playlist);
        }
        FREE_PTR(smartpltype);        
    }
    else {
        buffer = jsonrpc_respond_message(buffer, method, request_id, "Unknown smart playlist type", true);
        LOG_ERROR("Unknown smart playlist type: %s", playlist);
    }
    FREE_PTR(content);
    return buffer;
}

bool mpd_client_smartpls_save(t_config *config, t_mpd_state *mpd_state, const char *smartpltype, const char *playlist, 
                              const char *tag, const char *searchstr, const int maxentries, const int timerange)
{
    if (mpd_state->feat_smartpls == false) {
        return false;
    }
    else if (validate_string(playlist) == false) {
        return false;
    }
    
    sds tmp_file = sdscatfmt(sdsempty(), "%s/smartpls/%s.XXXXXX", config->varlibdir, playlist);
    int fd = mkstemp(tmp_file);
    if (fd < 0 ) {
        LOG_ERROR("Can't open %s for write", tmp_file);
        sdsfree(tmp_file);
        return false;
    }
    FILE *fp = fdopen(fd, "w");

    sds line = sdscat(sdsempty(), "{");
    line = tojson_char(line, "type", smartpltype, true);
    if (strcmp(smartpltype, "sticker") == 0) {
        line = tojson_char(line, "sticker", tag, true);
        line = tojson_long(line, "maxentries", maxentries, false);
    }
    else if (strcmp(smartpltype, "newest") == 0) {
        line = tojson_long(line, "timerange", timerange, false);
    }
    else if (strcmp(smartpltype, "search") == 0) {
        line = tojson_char(line, "tag", tag, true);
        line = tojson_char(line, "searchstr", searchstr, false);
    }
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
        LOG_ERROR("Renaming file from %s to %s failed", tmp_file, pl_file);
        sdsfree(tmp_file);
        sdsfree(pl_file);
        return false;
    }
    sdsfree(tmp_file);
    sdsfree(pl_file);
    if (mpd_client_smartpls_update(config, mpd_state, playlist) == false) {
        LOG_ERROR("Update of smart playlist %s failed", playlist);
        return false;
    }
    return true;
}

bool mpd_client_smartpls_update_all(t_config *config, t_mpd_state *mpd_state) {
    if (mpd_state->feat_smartpls == false) {
        LOG_WARN("Smart playlists are disabled");
        return true;
    }
    
    sds dirname = sdscatfmt(sdsempty(), "%s/smartpls", config->varlibdir);
    DIR *dir = opendir (dirname);
    if (dir != NULL) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strncmp(ent->d_name, ".", 1) == 0) {
                continue;
            }
            else {
                mpd_client_smartpls_update(config, mpd_state, ent->d_name);
            }
        }
        closedir (dir);
    } else {
        LOG_ERROR("Can't open smart playlist directory %s", dirname);
        sdsfree(dirname);
        return false;
    }
    sdsfree(dirname);
    return true;
}

bool mpd_client_smartpls_update(t_config *config, t_mpd_state *mpd_state, const char *playlist) {
    char *smartpltype = NULL;
    int je;
    bool rc = true;
    char *p_charbuf1 = NULL;
    char *p_charbuf2 = NULL;
    int int_buf1;
    
    if (mpd_state->feat_smartpls == false) {
        LOG_WARN("Smart playlists are disabled");
        return true;
    }
    
    sds filename = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, playlist);
    char *content = json_fread(filename);
    sdsfree(filename);
    if (content == NULL) {
        LOG_ERROR("Cant read smart playlist %s", playlist);
        return false;
    }
    je = json_scanf(content, strlen(content), "{type: %Q }", &smartpltype);
    if (je != 1) {
        LOG_ERROR("Cant read smart playlist type from %s", filename);
        return false;
    }
    if (strcmp(smartpltype, "sticker") == 0) {
        je = json_scanf(content, strlen(content), "{sticker: %Q, maxentries: %d}", &p_charbuf1, &int_buf1);
        if (je == 2) {
            if (mpd_client_smartpls_update_sticker(mpd_state, playlist, p_charbuf1, int_buf1) == false) {
                LOG_ERROR("Update of smart playlist %s failed.", playlist);
                rc = false;
            }
         }
         else {
            LOG_ERROR("Can't parse smart playlist file %s", filename);
            rc = false;
         }
         FREE_PTR(p_charbuf1);
    }
    else if (strcmp(smartpltype, "newest") == 0) {
        je = json_scanf(content, strlen(content), "{timerange: %d}", &int_buf1);
        if (je == 1) {
            if (mpd_client_smartpls_update_newest(mpd_state, playlist, int_buf1) == false) {
                LOG_ERROR("Update of smart playlist %s failed", playlist);
                rc = false;
            }
        }
        else {
            LOG_ERROR("Can't parse smart playlist file %s", filename);
            rc = false;
        }
    }
    else if (strcmp(smartpltype, "search") == 0) {
        je = json_scanf(content, strlen(content), "{tag: %Q, searchstr: %Q}", &p_charbuf1, &p_charbuf2);
        if (je == 2) {
            if (mpd_client_smartpls_update_search(mpd_state, playlist, p_charbuf1, p_charbuf2) == false) {
                LOG_ERROR("Update of smart playlist %s failed", playlist);
                rc = false;
            }

        }
        else {
            LOG_ERROR("Can't parse smart playlist file %s", filename);
            rc = false;
        }
        FREE_PTR(p_charbuf1);
        FREE_PTR(p_charbuf2);
    }
    FREE_PTR(smartpltype);
    FREE_PTR(content);
    return rc;
}

bool mpd_client_smartpls_clear(t_mpd_state *mpd_state, const char *playlist) {
    struct mpd_playlist *pl;
    const char *plpath;
    bool exists = false;
    if (mpd_send_list_playlists(mpd_state->conn) == false) {
        check_error_and_recover(mpd_state, NULL, NULL, 0);
        return 1;
    }
    while ((pl = mpd_recv_playlist(mpd_state->conn)) != NULL) {
        plpath = mpd_playlist_get_path(pl);
        if (strcmp(playlist, plpath) == 0) {
            exists = true;
        }
        mpd_playlist_free(pl);
        if (exists == true) {
            break;
        }
    }
    mpd_response_finish(mpd_state->conn);
    
    if (exists) {
        if (mpd_run_rm(mpd_state->conn, playlist) == false) {
            check_error_and_recover(mpd_state, NULL, NULL, 0);
            return false;
        }
    }
    return true;
}

bool mpd_client_smartpls_update_search(t_mpd_state *mpd_state, const char *playlist, const char *tag, const char *searchstr) {
    sds buffer = sdsempty();
    sds method = sdsempty();
    mpd_client_smartpls_clear(mpd_state, playlist);
    if (mpd_state->feat_advsearch == true && strcmp(tag, "expression") == 0) {
        buffer = mpd_client_search_adv(mpd_state, buffer, method, 0, searchstr, NULL, true, NULL, playlist, 0, NULL);
    }
    else {
        buffer = mpd_client_search(mpd_state, buffer, method, 0, searchstr, tag, playlist, 0, NULL);
    }
    sdsfree(buffer);
    sdsfree(method);
    LOG_INFO("Updated smart playlist %s", playlist);
    return true;
}

bool mpd_client_smartpls_update_sticker(t_mpd_state *mpd_state, const char *playlist, const char *sticker, const int maxentries) {

    if (mpd_send_sticker_find(mpd_state->conn, "song", "", sticker) == false) {
        check_error_and_recover(mpd_state, NULL, NULL, 0);
        return false;    
    }

    struct list add_list;
    list_init(&add_list);

    struct mpd_pair *pair;
    char *uri = NULL;
    int value_max = 0;

    while ((pair = mpd_recv_pair(mpd_state->conn)) != NULL) {
        if (strcmp(pair->name, "file") == 0) {
            FREE_PTR(uri);
            uri = strdup(pair->value);
        } 
        else if (strcmp(pair->name, "sticker") == 0) {
            size_t j;
            const char *p_value = mpd_parse_sticker(pair->value, &j);
            if (p_value != NULL) {
                char *crap;
                int value = strtoimax(p_value, &crap, 10);
                if (value >= 1) {
                    list_push(&add_list, uri, value, NULL, NULL);
                }
                if (value > value_max) {
                    value_max = value;
                }
            }
        }
        mpd_return_pair(mpd_state->conn, pair);
    }
    mpd_response_finish(mpd_state->conn);
    FREE_PTR(uri);

    mpd_client_smartpls_clear(mpd_state, playlist);
     
    if (value_max > 2) {
        value_max = value_max / 2;
    }

    list_sort_by_value_i(&add_list, false);

    struct node *current = add_list.head;
    int i = 0;
    while (current != NULL) {
        if (current->value_i >= value_max) {
            if (mpd_run_playlist_add(mpd_state->conn, playlist, current->key) == false) {
                check_error_and_recover(mpd_state, NULL, NULL, 0);
                list_free(&add_list);
                return false;
            }
            i++;
            if (i >= maxentries) {
                break;
            }
        }
        current = current->next;
    }
    list_free(&add_list);
    LOG_INFO("Updated smart playlist %s with %d songs, minValue: %d", playlist, i, value_max);
    return true;
}

bool mpd_client_smartpls_update_newest(t_mpd_state *mpd_state, const char *playlist, const int timerange) {
    int value_max = 0;
    
    struct mpd_stats *stats = mpd_run_stats(mpd_state->conn);
    if (stats != NULL) {
        value_max = mpd_stats_get_db_update_time(stats);
        mpd_stats_free(stats);
    }
    else {
        check_error_and_recover(mpd_state, NULL, NULL, 0);
        return false;
    }

    mpd_client_smartpls_clear(mpd_state, playlist);
    value_max -= timerange;
    sds buffer = sdsempty();
    sds method = sdsempty();
    if (value_max > 0) {
        if (mpd_state->feat_advsearch == true) {
            sds searchstr = sdscatprintf(sdsempty(), "(modified-since '%d')", value_max);
            buffer = mpd_client_search_adv(mpd_state, buffer, method, 0, searchstr, NULL, true, NULL, playlist, 0, NULL);
            sdsfree(searchstr);
        }
        else {
            sds searchstr = sdsfromlonglong(value_max);
            buffer = mpd_client_search(mpd_state, buffer, method, 0, searchstr, "modified-since", playlist, 0, NULL);
            sdsfree(searchstr);
        }
        LOG_INFO("Updated smart playlist %s", playlist);
    }
    sdsfree(buffer);
    sdsfree(method);
    return true;
}
