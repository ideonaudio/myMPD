/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#define _GNU_SOURCE 

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <limits.h>
#include <libgen.h>
#include <mpd/client.h>
#include <signal.h>
#include <time.h>
#include <pcre.h>

#include "../../dist/src/sds/sds.h"
#include "../sds_extras.h"
#include "../../dist/src/rax/rax.h"
#include "../list.h"
#include "config_defs.h"
#include "../utility.h"
#include "../api.h"
#include "../log.h"
#include "../tiny_queue.h"
#include "../global.h"
#include "../mpd_shared/mpd_shared_typedefs.h"
#include "../mpd_shared.h"
#include "../mpd_shared/mpd_shared_tags.h"
#include "../mpd_shared/mpd_shared_sticker.h"
#include "mpd_client_utility.h"
#include "mpd_client_cover.h"
#include "mpd_client_browse.h"

//private definitions
static bool _search_song(struct mpd_song *song, struct list *expr_list, t_tags *browse_tag_types);
static pcre *_compile_regex(const char *regex_str);
static bool _cmp_regex(pcre *re_compiled, const char *value);

//public functions
sds mpd_client_put_fingerprint(t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id,
                               const char *uri)
{
    if (validate_songuri(uri) == false) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, true, "database", "error", "Invalid URI");
        return buffer;
    }
    
    char fp_buffer[8192];
    const char *fingerprint = mpd_run_getfingerprint_chromaprint(mpd_client_state->mpd_state->conn, uri, fp_buffer, sizeof(fp_buffer));
    if (fingerprint == NULL) {
        check_error_and_recover2(mpd_client_state->mpd_state, &buffer, method, request_id, false);
        return buffer;
    }
    
    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = tojson_char(buffer, "fingerprint", fingerprint, false);
    buffer = jsonrpc_result_end(buffer);
    
    mpd_response_finish(mpd_client_state->mpd_state->conn);
    check_error_and_recover2(mpd_client_state->mpd_state, &buffer, method, request_id, false);
    
    return buffer;
}

sds mpd_client_put_songdetails(t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id, 
                               const char *uri)
{
    if (validate_songuri(uri) == false) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, true, "database", "error", "Invalid URI");
        return buffer;
    }

    bool rc = mpd_send_list_meta(mpd_client_state->mpd_state->conn, uri);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_send_list_meta") == false) {
        return buffer;
    }

    buffer = jsonrpc_result_start(buffer, method, request_id);

    struct mpd_song *song;
    if ((song = mpd_recv_song(mpd_client_state->mpd_state->conn)) != NULL) {
        buffer = put_song_tags(buffer, mpd_client_state->mpd_state, &mpd_client_state->mpd_state->mympd_tag_types, song);
        mpd_song_free(song);
    }

    mpd_response_finish(mpd_client_state->mpd_state->conn);
    if (check_error_and_recover2(mpd_client_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    
    if (mpd_client_state->feat_sticker) {
        buffer = sdscat(buffer, ",");
        buffer = mpd_shared_sticker_list(buffer, mpd_client_state->sticker_cache, uri);
    }
    
    buffer = sdscat(buffer, ",");
    buffer = put_extra_files(mpd_client_state, buffer, uri, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

sds mpd_client_put_filesystem(t_config *config, t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id, 
                              const char *path, const unsigned int offset, const unsigned int limit, const char *searchstr, const t_tags *tagcols)
{
    bool rc = mpd_send_list_meta(mpd_client_state->mpd_state->conn, path);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_send_list_meta") == false) {
        return buffer;
    }

    struct list entity_list;
    list_init(&entity_list);
    struct mpd_entity *entity;
    size_t search_len = strlen(searchstr);
    while ((entity = mpd_recv_entity(mpd_client_state->mpd_state->conn)) != NULL) {
        switch (mpd_entity_get_type(entity)) {
            case MPD_ENTITY_TYPE_UNKNOWN: {
                break;
            }
            case MPD_ENTITY_TYPE_SONG: {
                const struct mpd_song *song = mpd_entity_get_song(entity);
                sds entity_name = sdsempty();
                entity_name = mpd_shared_get_tags(song, MPD_TAG_TITLE, entity_name);
                if (search_len == 0  || strcasestr(entity_name, searchstr) != NULL) {
                    sds key = sdscatprintf(sdsempty(), "2%s", mpd_song_get_uri(song));
                    sdstolower(key);
                    list_insert_sorted_by_key(&entity_list, key, MPD_ENTITY_TYPE_SONG, entity_name, mpd_song_dup(song), false);
                    sdsfree(key);
                }
                sdsfree(entity_name);
                break;
            }
            case MPD_ENTITY_TYPE_DIRECTORY: {
                const struct mpd_directory *dir = mpd_entity_get_directory(entity);
                const char *entity_name = mpd_directory_get_path(dir);
                char *dir_name = strrchr(entity_name, '/');
                if (dir_name != NULL) {
                    dir_name++;
                }
                else {
                    dir_name = (char *)entity_name;
                }
                if (search_len == 0  || strcasestr(dir_name, searchstr) != NULL) {
                    sds key = sdscatprintf(sdsempty(), "0%s", mpd_directory_get_path(dir));
                    sdstolower(key);
                    list_insert_sorted_by_key(&entity_list, key, MPD_ENTITY_TYPE_DIRECTORY, dir_name, mpd_directory_dup(dir), false);
                    sdsfree(key);
                }
                break;
            }
            case MPD_ENTITY_TYPE_PLAYLIST: {
                const struct mpd_playlist *pl = mpd_entity_get_playlist(entity);
                const char *entity_name = mpd_playlist_get_path(pl);
                //do not show mpd playlists in root directory
                if (strcmp(path, "/") == 0) {
                    sds ext = get_extension_from_filename(entity_name);
                    if (strcmp(ext, "m3u") != 0 && strcmp(ext, "pls") != 0) {
                        sdsfree(ext);
                        break;
                    }
                    sdsfree(ext);
                }
                char *pl_name = strrchr(entity_name, '/');
                if (pl_name != NULL) {
                    pl_name++;
                }
                else {
                    pl_name = (char *)entity_name;
                }
                if (search_len == 0  || strcasestr(pl_name, searchstr) != NULL) {
                    sds key = sdscatprintf(sdsempty(), "1%s", mpd_playlist_get_path(pl));
                    sdstolower(key);
                    list_insert_sorted_by_key(&entity_list, key, MPD_ENTITY_TYPE_PLAYLIST, pl_name, mpd_playlist_dup(pl), false);
                    sdsfree(key);
                }
                break;
            }
        }
        mpd_entity_free(entity);
    }
    
    mpd_response_finish(mpd_client_state->mpd_state->conn);
    if (check_error_and_recover2(mpd_client_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }

    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    
    unsigned entity_count = 0;
    unsigned entities_returned = 0;
    if (strlen(path) > 1) {
        char *path_cpy = strdup(path);
        char *parent_dir = dirname(path_cpy);
        buffer = sdscat(buffer, "{\"Type\":\"parentDir\",\"name\":\"parentDir\",");
        buffer = tojson_char(buffer, "uri", (parent_dir[0] == '.' ? "" : parent_dir), false);
        buffer = sdscat(buffer, "}");
        entity_count++;
        entities_returned++;
        free(path_cpy);
    }
    struct list_node *current;
    while ((current = list_shift_first(&entity_list)) != NULL) {
        entity_count++;
        if (entity_count > offset && (entity_count <= offset + limit || limit == 0)) {
            if (entities_returned++) {
                buffer = sdscat(buffer, ",");
            }
            switch (current->value_i) {
                case MPD_ENTITY_TYPE_SONG: {
                    struct mpd_song *song = (struct mpd_song *)current->user_data;
                    buffer = sdscat(buffer, "{\"Type\":\"song\",");
                    buffer = put_song_tags(buffer, mpd_client_state->mpd_state, tagcols, song);
                    if (mpd_client_state->feat_sticker) {
                        buffer = sdscat(buffer, ",");
                        buffer = mpd_shared_sticker_list(buffer, mpd_client_state->sticker_cache, mpd_song_get_uri(song));
                    }
                    buffer = sdscat(buffer, "}");
                    mpd_song_free(song);
                    break;
                }
                case MPD_ENTITY_TYPE_DIRECTORY: {
                    struct mpd_directory *dir = (struct mpd_directory *)current->user_data;
                    buffer = sdscat(buffer, "{\"Type\":\"dir\",");
                    buffer = tojson_char(buffer, "uri", mpd_directory_get_path(dir), true);
                    buffer = tojson_char(buffer, "name", current->value_p, false);
                    buffer = sdscat(buffer, "}");
                    mpd_directory_free(dir);
                    break;
                }
                case MPD_ENTITY_TYPE_PLAYLIST: {
                    struct mpd_playlist *pl = (struct mpd_playlist *)current->user_data;
                    bool smartpls = is_smartpls(config, mpd_client_state, current->value_p);
                    buffer = sdscatfmt(buffer, "{\"Type\": \"%s\",", (smartpls == true ? "smartpls" : "plist"));
                    buffer = tojson_char(buffer, "uri", mpd_playlist_get_path(pl), true);
                    buffer = tojson_char(buffer, "name", current->value_p, false);
                    buffer = sdscat(buffer, "}");
                    mpd_playlist_free(pl);
                    break;
                }
            }
        }
        else {
            switch (current->value_i) {
                case MPD_ENTITY_TYPE_SONG: {
                    struct mpd_song *song = (struct mpd_song *)current->user_data;
                    mpd_song_free(song);
                    break;
                }
                case MPD_ENTITY_TYPE_DIRECTORY: {
                    struct mpd_directory *dir = (struct mpd_directory *)current->user_data;
                    mpd_directory_free(dir);
                    break;
                }
                case MPD_ENTITY_TYPE_PLAYLIST: {
                    struct mpd_playlist *pl = (struct mpd_playlist *)current->user_data;
                    mpd_playlist_free(pl);
                    break;
                }
            }
        }
        list_node_free_keep_user_data(current);
    }

    buffer = sdscatlen(buffer, "],", 2);
    buffer = put_extra_files(mpd_client_state, buffer, path, true);
    buffer = sdscatlen(buffer, ",", 1);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_char(buffer, "search", searchstr, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

sds mpd_client_put_songs_in_album(t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id,
                                  const char *album, const char *search, const char *tag, const t_tags *tagcols)
{
    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");

    bool rc = mpd_search_db_songs(mpd_client_state->mpd_state->conn, true);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_db_songs") == false) {
        mpd_search_cancel(mpd_client_state->mpd_state->conn);
        return buffer;
    }    
    rc = mpd_search_add_tag_constraint(mpd_client_state->mpd_state->conn, MPD_OPERATOR_DEFAULT, mpd_tag_name_parse(tag), search);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_add_tag_constraint") == false) {
        mpd_search_cancel(mpd_client_state->mpd_state->conn);
        return buffer;
    }
    rc = mpd_search_add_tag_constraint(mpd_client_state->mpd_state->conn, MPD_OPERATOR_DEFAULT, MPD_TAG_ALBUM, album);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_add_tag_constraint") == false) {
        mpd_search_cancel(mpd_client_state->mpd_state->conn);
        return buffer;
    }
    rc = mpd_search_commit(mpd_client_state->mpd_state->conn);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_commit") == false) {
        buffer = check_error_and_recover(mpd_client_state->mpd_state, buffer, method, request_id);
        return buffer;
    }

    struct mpd_song *song;
    struct mpd_song *first_song = NULL;
    int entity_count = 0;
    int entities_returned = 0;
    unsigned int totalTime = 0;
    int discs = 1;

    while ((song = mpd_recv_song(mpd_client_state->mpd_state->conn)) != NULL) {
        entity_count++;
        if (entities_returned++) {
            buffer = sdscat(buffer, ",");
        }
        else {
            first_song = mpd_song_dup(song);
        }
        const char *disc;
        if ((disc = mpd_song_get_tag(song, MPD_TAG_DISC, 0)) != NULL) {
            int d = strtoimax(disc, NULL, 10);
            if (d > discs) {
                discs = d;
            }
        }
        buffer = sdscat(buffer, "{\"Type\": \"song\",");
        buffer = put_song_tags(buffer, mpd_client_state->mpd_state, tagcols, song);
        if (mpd_client_state->feat_sticker) {
            buffer = sdscat(buffer, ",");
            buffer = mpd_shared_sticker_list(buffer, mpd_client_state->sticker_cache, mpd_song_get_uri(song));
        }
        buffer = sdscat(buffer, "}");

        totalTime += mpd_song_get_duration(song);
        mpd_song_free(song);
    }

    buffer = sdscat(buffer, "],");
    
    sds albumartist = sdsempty();
    if (first_song != NULL) {
        albumartist = mpd_shared_get_tags(first_song, MPD_TAG_ALBUM_ARTIST, albumartist);
        buffer = put_extra_files(mpd_client_state, buffer, mpd_song_get_uri(first_song), false);
    }
    else {
        buffer = sdscat(buffer, "\"images\":[],\"bookletPath\":\"\"");
    }
    
    buffer = sdscatlen(buffer, ",", 1);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_char(buffer, "Album", album, true);
    buffer = tojson_char(buffer, "search", search, true);
    buffer = tojson_char(buffer, "tag", tag, true);
    buffer = tojson_char(buffer, "AlbumArtist", albumartist, true);
    buffer = tojson_long(buffer, "Discs", discs, true);
    buffer = tojson_long(buffer, "totalTime", totalTime, false);
    buffer = jsonrpc_result_end(buffer);
        
    if (first_song != NULL) {
        mpd_song_free(first_song);
    }
    sdsfree(albumartist);
    mpd_response_finish(mpd_client_state->mpd_state->conn);
    if (check_error_and_recover2(mpd_client_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    
    return buffer;    
}

sds mpd_client_put_firstsong_in_albums(t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id, 
                                       const char *searchstr, const char *filter, const char *sort, bool sortdesc, const unsigned int offset, unsigned int limit)
{
    if (mpd_client_state->album_cache == NULL) {
        buffer = jsonrpc_respond_message(buffer, method, request_id, true, "database", "error", "Albumcache not ready");
        return buffer;
    }

    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");

    struct mpd_song *song;
    //parse sort tag
    bool sort_by_last_modified = false;
    enum mpd_tag_type sort_tag = MPD_TAG_ALBUM;

    if (strlen(sort) > 0) {
        enum mpd_tag_type sort_tag_org = mpd_tag_name_parse(sort);
        if (sort_tag_org != MPD_TAG_UNKNOWN) {
            sort_tag = get_sort_tag(sort_tag_org);
            if (mpd_shared_tag_exists(mpd_client_state->mpd_state->mympd_tag_types.tags, mpd_client_state->mpd_state->mympd_tag_types.len, sort_tag) == false) {
                //sort tag is not enabled, revert 
                sort_tag = sort_tag_org;
            }
        }
        else if (strcmp(sort, "Last-Modified") == 0) {
            sort_by_last_modified = true;
        }
        else {
            MYMPD_LOG_WARN("Unknown sort tag: %s", sort);
        }
    }
    //parse mpd search expression
    struct list expr_list;
    list_init(&expr_list);
    int count;
    sds *tokens = sdssplitlen(searchstr, strlen(searchstr), ") AND (", 7, &count);
    for (int j = 0; j < count; j++) {
        sdstrim(tokens[j], "() ");
        sds tag = sdsempty();
        sds op = sdsempty();
        sds value = sdsempty();
        unsigned i = 0;
        char *p = tokens[j];
        //tag
        for (i = 0; i < sdslen(tokens[j]); i++, p++) {
            if (tokens[j][i] == ' ') {
                break;
            }
            tag = sdscatprintf(tag, "%.*s", 1, p);
        }
        if (i + 1 >= sdslen(tokens[j])) {
            MYMPD_LOG_ERROR("Can not parse search expression");
            sdsfree(tag);
            sdsfree(op);
            sdsfree(value);
            break;
        }
        i++;
        p++;
        //operator
        for (; i < sdslen(tokens[j]); i++, p++) {
            if (tokens[j][i] == ' ') {
                break;
            }
            op = sdscatprintf(op, "%.*s", 1, p);
        }
        if (i + 2 >= sdslen(tokens[j])) {
            MYMPD_LOG_ERROR("Can not parse search expression");
            sdsfree(tag);
            sdsfree(op);
            sdsfree(value);
            break;
        }
        i = i + 2;
        p = p + 2;
        //value
        for (; i < sdslen(tokens[j]) - 1; i++, p++) {
            value = sdscatprintf(value, "%.*s", 1, p);
        }
        int tag_type = mpd_tag_name_parse(tag);
        if (tag_type == -1 && strcmp(tag, "any") == 0) {
            tag_type = -2;
        }
        if (strcmp(op, "=~") == 0 || strcmp(op, "!~") == 0) {
            //is regex, compile
            pcre *re_compiled = _compile_regex(value);
            list_push(&expr_list, value, tag_type, op , re_compiled);
        }
        else {
            list_push(&expr_list, value, tag_type, op , NULL);
        }
        MYMPD_LOG_DEBUG("Parsed expression tag: \"%s\", op: \"%s\", value:\"%s\"", tag, op, value);
        sdsfree(tag);
        sdsfree(op);
        sdsfree(value);
    }
    sdsfreesplitres(tokens, count);
    
    //search and sort albumlist
    struct list album_list;
    list_init(&album_list);
    raxIterator iter;
    raxStart(&iter, mpd_client_state->album_cache);
    raxSeek(&iter, "^", NULL, 0);
    sds key = sdsempty();
    while (raxNext(&iter)) {
        song = (struct mpd_song *)iter.data;
        if (_search_song(song, &expr_list, &mpd_client_state->browse_tag_types) == true) {
            if (sort_by_last_modified == true) {
                key = sdscatlen(key, iter.key, iter.key_len);
                list_insert_sorted_by_value_i(&album_list, key, mpd_song_get_last_modified(song), NULL, iter.data, sortdesc);
                sdsclear(key);
            }
            else {
                const char *sort_value = mpd_song_get_tag(song, sort_tag, 0);
                if (sort_value != NULL) {
                    list_insert_sorted_by_key(&album_list, sort_value, 0, NULL, iter.data, sortdesc);
                }
                else if (sort_tag == MPD_TAG_ALBUM_ARTIST) {
                    //fallback to artist tag if albumartist tag is not set
                    sort_value = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
                    list_insert_sorted_by_key(&album_list, sort_value, 0, NULL, iter.data, sortdesc);
                }
                else {
                    //sort tag not present, append to end of the list
                    list_push(&album_list, "zzzzzzzzzz", 0, NULL, iter.data);
                }
            }
        }
    }
    raxStop(&iter);
    sdsfree(key);
    list_free(&expr_list);
    
    //print album list
    unsigned entity_count = 0;
    unsigned entities_returned = 0;
    unsigned end = offset + limit;
    sds album = sdsempty();
    sds artist = sdsempty();
    struct list_node *current;
    while ((current = list_shift_first(&album_list)) != NULL) {
        entity_count++;
        if (entity_count > offset && (entity_count <= end || limit == 0)) {
            if (entities_returned++) {
                buffer = sdscat(buffer, ",");
            }
            song = (struct mpd_song *)current->user_data;
            album = mpd_shared_get_tags(song, MPD_TAG_ALBUM, album);
            artist = mpd_shared_get_tags(song, MPD_TAG_ALBUM_ARTIST, artist);
            buffer = sdscat(buffer, "{\"Type\": \"album\",");
            buffer = tojson_char(buffer, "Album", album, true);
            buffer = tojson_char(buffer, "AlbumArtist", artist, true);
            buffer = tojson_char(buffer, "FirstSongUri", mpd_song_get_uri(song), false);
            buffer = sdscat(buffer, "}");
        }
        list_node_free_keep_user_data(current);
        if (entity_count > end && limit > 0) {
            break;
        }
    }
    sdsfree(album);
    sdsfree(artist);
    entity_count = album_list.length;
    list_free_keep_user_data(&album_list);

    buffer = sdscat(buffer, "],");
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_char(buffer, "filter", filter, true);
    buffer = tojson_char(buffer, "searchstr", searchstr, true);
    buffer = tojson_char(buffer, "sort", sort, true);
    buffer = tojson_bool(buffer, "sortdesc", sortdesc, true);
    buffer = tojson_char(buffer, "tag", "Album", false);
    buffer = jsonrpc_result_end(buffer);
        
    return buffer;    
}

sds mpd_client_put_db_tag2(t_config *config, t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id, 
                           const char *searchstr, const char *filter, const char *sort, bool sortdesc, const unsigned int offset, const unsigned int limit, const char *tag)
{
    (void) sort;
    (void) sortdesc;
    size_t searchstr_len = strlen(searchstr);
    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");
   
    bool rc = mpd_search_db_tags(mpd_client_state->mpd_state->conn, mpd_tag_name_parse(tag));
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_db_tags") == false) {
        mpd_search_cancel(mpd_client_state->mpd_state->conn);
        return buffer;
    }
    
    rc = mpd_search_commit(mpd_client_state->mpd_state->conn);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_search_commit") == false) {
        return buffer;
    }

    struct mpd_pair *pair;
    unsigned entity_count = 0;
    unsigned entities_returned = 0;
    enum mpd_tag_type mpdtag = mpd_tag_name_parse(tag);
    while ((pair = mpd_recv_pair_tag(mpd_client_state->mpd_state->conn, mpdtag)) != NULL) {
        entity_count++;
        if (entity_count > offset && (entity_count <= offset + limit || limit == 0)) {
            if (strcmp(pair->value, "") == 0) {
                entity_count--;
            }
            else if (searchstr_len == 0
                     || (searchstr_len <= 2 && strncasecmp(searchstr, pair->value, searchstr_len) == 0)
                     || (searchstr_len > 2 && strcasestr(pair->value, searchstr) != NULL))
            {
                if (entities_returned++) {
                    buffer = sdscat(buffer, ",");
                }
                buffer = sdscat(buffer, "{");
                buffer = tojson_char(buffer, "value", pair->value, false);
                buffer = sdscat(buffer, "}");
            }
            else {
                entity_count--;
            }
        }
        mpd_return_pair(mpd_client_state->mpd_state->conn, pair);
    }
    mpd_response_finish(mpd_client_state->mpd_state->conn);
    if (check_error_and_recover2(mpd_client_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }

    //checks if this tag has a directory with pictures in /var/lib/mympd/pics
    sds pic_path = sdscatfmt(sdsempty(), "%s/pics/%s", config->varlibdir, tag);
    bool pic = false;
    DIR* dir = opendir(pic_path);
    if (dir != NULL) {
        closedir(dir);
        pic = true;
    }
    else {
        MYMPD_LOG_DEBUG("Can not open directory \"%s\": %s", pic_path, strerror(errno));
        //ignore error
    }
    sdsfree(pic_path);

    buffer = sdscat(buffer, "],");
    buffer = tojson_long(buffer, "totalEntities", -1, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_char(buffer, "filter", filter, true);
    buffer = tojson_char(buffer, "searchstr", searchstr, true);
    buffer = tojson_char(buffer, "sort", sort, true);
    buffer = tojson_bool(buffer, "sortdesc", sortdesc, true);
    buffer = tojson_char(buffer, "tag", tag, true);
    buffer = tojson_bool(buffer, "pics", pic, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

//private functions
static bool _search_song(struct mpd_song *song, struct list *expr_list, t_tags *browse_tag_types) {
    struct list_node *current = expr_list->head;
    sds value = sdsempty();
    (void) browse_tag_types;
    struct t_tags one_tag;
    one_tag.len = 1;
    while (current != NULL) {
        struct t_tags *tags = NULL;
        if (current->value_i == -2) {
            //any - use all browse tags
            tags = browse_tag_types;
        }
        else {
            //use selected tag only
            tags = &one_tag;
            tags->tags[0] = current->value_i;
        }
        bool rc = false;
        for (unsigned i = 0; i < tags->len; i++) {
            rc = true;
            value = mpd_shared_get_tags(song, tags->tags[i], value);
            if (strcmp(current->value_p, "contains") == 0 && strcasestr(value, current->key) == NULL) {
                rc = false;
            }
            else if (strcmp(current->value_p, "starts_with") == 0 && strncasecmp(current->key, value, strlen(current->key)) != 0) {
                rc = false;
            }
            else if (strcmp(current->value_p, "==") == 0 && strcasecmp(value, current->key) != 0) {
                rc = false;
            }
            else if (strcmp(current->value_p, "!=") == 0 && strcasecmp(value, current->key) == 0) {
                rc = false;
            }
            else if (strcmp(current->value_p, "=~") == 0 && _cmp_regex((pcre *)current->user_data, value) == false) {
                rc = false;
            }
            else if (strcmp(current->value_p, "!~") == 0 && _cmp_regex((pcre *)current->user_data, value) == true) {
                rc = false;
            }
            else {
                //tag value matched
                break;
            }
        }
        if (rc == false) {
            sdsfree(value);
            return false;
        }
        current = current->next;
    }
    sdsfree(value);
    return true;
}

static pcre *_compile_regex(const char *regex_str) {
    MYMPD_LOG_DEBUG("Compiling regex: \"%s\"", regex_str);
    const char *pcre_error_str;
    int pcre_error_offset;
    pcre *re_compiled = pcre_compile(regex_str, PCRE_CASELESS, &pcre_error_str, &pcre_error_offset, NULL);
    if (re_compiled == NULL) {
        MYMPD_LOG_DEBUG("Could not compile '%s': %s\n", regex_str, pcre_error_str);
    }
    return re_compiled;
}

static bool _cmp_regex(pcre *re_compiled, const char *value) {
    if (re_compiled == NULL) {
        return false;
    }
    bool rc = false;
    int substr_vec[30];
    int pcre_exec_ret = pcre_exec(re_compiled, NULL, value, (int) strlen(value), 0, 0, substr_vec, 30);
    if (pcre_exec_ret < 0) {
        switch(pcre_exec_ret) {
            case PCRE_ERROR_NOMATCH      : break;
            case PCRE_ERROR_NULL         : MYMPD_LOG_ERROR("Something was null"); break;
            case PCRE_ERROR_BADOPTION    : MYMPD_LOG_ERROR("A bad option was passed"); break;
            case PCRE_ERROR_BADMAGIC     : MYMPD_LOG_ERROR("Magic number bad (compiled regex corrupt?)"); break;
            case PCRE_ERROR_UNKNOWN_NODE : MYMPD_LOG_ERROR("Something kooky in the compiled regex"); break;
            case PCRE_ERROR_NOMEMORY     : MYMPD_LOG_ERROR("Ran out of memory"); break;
            default                      : MYMPD_LOG_ERROR("Unknown error"); break;
        }
    }
    else {
        rc = true;
    }
    return rc;
}
