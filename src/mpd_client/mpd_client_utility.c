/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <libgen.h>
#include <pthread.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <mpd/client.h>

#include "../../dist/src/sds/sds.h"
#include "../sds_extras.h"
#include "../../dist/src/frozen/frozen.h"
#include "../list.h"
#include "config_defs.h"
#include "../tiny_queue.h"
#include "../api.h"
#include "../global.h"
#include "../utility.h"
#include "../log.h"
#include "../mpd_shared/mpd_shared_typedefs.h"
#include "../mpd_shared/mpd_shared_tags.h"
#include "../mpd_shared.h"
#include "mpd_client_utility.h"

//private definitons
static void detect_extra_files(t_mpd_client_state *mpd_client_state, const char *uri, sds *booklet_path, struct list *images, bool is_dirname);

//public functions

sds put_extra_files(t_mpd_client_state *mpd_client_state, sds buffer, const char *uri, bool is_dirname)
{
    struct list images;
    list_init(&images);
    sds booklet_path = sdsempty();
    if (is_streamuri(uri) == false && mpd_client_state->feat_library == true)
    {
        detect_extra_files(mpd_client_state, uri, &booklet_path, &images, is_dirname);
    }
    buffer = tojson_char(buffer, "bookletPath", booklet_path, true);
    buffer = sdscat(buffer, "\"images\": [");
    struct list_node *current = images.head;
    while (current != NULL)
    {
        if (current != images.head)
        {
            buffer = sdscatlen(buffer, ",", 1);
        }
        buffer = sdscatjson(buffer, current->key, sdslen(current->key));
        current = current->next;
    }
    buffer = sdscat(buffer, "]");
    list_free(&images);
    sdsfree(booklet_path);
    return buffer;
}

void json_to_tags(const char *str, int len, void *user_data)
{
    struct json_token t;
    int i;
    t_tags *tags = (t_tags *)user_data;
    tags->len = 0;
    for (i = 0; json_scanf_array_elem(str, len, "", i, &t) > 0; i++)
    {
        sds token = sdscatlen(sdsempty(), t.ptr, t.len);
        enum mpd_tag_type tag = mpd_tag_name_iparse(token);
        sdsfree(token);
        if (tag != MPD_TAG_UNKNOWN)
        {
            tags->tags[tags->len++] = tag;
        }
    }
}

bool is_smartpls(t_config *config, t_mpd_client_state *mpd_client_state, const char *plpath)
{
    bool smartpls = false;
    if (mpd_client_state->feat_smartpls == true)
    {
        sds smartpls_file = sdscatfmt(sdsempty(), "%s/smartpls/%s", config->varlibdir, plpath);
        if (validate_string(plpath) == true)
        {
            if (access(smartpls_file, F_OK) != -1)
            { /* Flawfinder: ignore */
                smartpls = true;
            }
        }
        sdsfree(smartpls_file);
    }
    return smartpls;
}

void default_mpd_client_state(t_mpd_client_state *mpd_client_state)
{
    mpd_client_state->song_id = -1;
    mpd_client_state->song_uri = sdsempty();
    mpd_client_state->next_song_id = -1;
    mpd_client_state->last_song_id = -1;
    mpd_client_state->last_song_uri = sdsempty();
    mpd_client_state->queue_version = 0;
    mpd_client_state->queue_length = 0;
    mpd_client_state->last_last_played_id = -1;
    mpd_client_state->song_end_time = 0;
    mpd_client_state->song_start_time = 0;
    mpd_client_state->last_song_end_time = 0;
    mpd_client_state->last_song_start_time = 0;
    mpd_client_state->last_skipped_id = 0;
    mpd_client_state->crossfade = 0;
    mpd_client_state->coverimage = false;
    mpd_client_state->set_song_played_time = 0;
    mpd_client_state->music_directory = sdsempty();
    mpd_client_state->music_directory_value = sdsempty();
    mpd_client_state->jukebox_mode = JUKEBOX_OFF;
    mpd_client_state->jukebox_playlist = sdsempty();
    mpd_client_state->jukebox_unique_tag.len = 1;
    mpd_client_state->jukebox_unique_tag.tags[0] = MPD_TAG_ARTIST;
    mpd_client_state->jukebox_last_played = 24;
    mpd_client_state->jukebox_queue_length = 1;
    mpd_client_state->jukebox_enforce_unique = true;
    mpd_client_state->coverimage_name = sdsempty();
    mpd_client_state->love = false;
    mpd_client_state->love_channel = sdsempty();
    mpd_client_state->love_message = sdsempty();
    mpd_client_state->searchtaglist = sdsempty();
    mpd_client_state->browsetaglist = sdsempty();
    mpd_client_state->smartpls = false;
    mpd_client_state->generate_pls_tags = sdsempty();
    mpd_client_state->smartpls_sort = sdsempty();
    mpd_client_state->smartpls_prefix = sdsempty();
    mpd_client_state->smartpls_interval = 14400;
    mpd_client_state->booklet_name = sdsnew("booklet.pdf");
    mpd_client_state->stickers = false;
    mpd_client_state->max_elements_per_page = 100;
    mpd_client_state->feat_coverimage = false;
    mpd_client_state->auto_play = false;
    reset_t_tags(&mpd_client_state->search_tag_types);
    reset_t_tags(&mpd_client_state->browse_tag_types);
    reset_t_tags(&mpd_client_state->generate_pls_tag_types);
    //init last played songs list
    list_init(&mpd_client_state->last_played);
    //init sticker queue
    list_init(&mpd_client_state->sticker_queue);
    mpd_client_state->sticker_cache_building = false;
    mpd_client_state->sticker_cache = NULL;
    //jukebox queue
    list_init(&mpd_client_state->jukebox_queue);
    list_init(&mpd_client_state->jukebox_queue_tmp);
    //mpd state
    mpd_client_state->mpd_state = (t_mpd_state *)malloc(sizeof(t_mpd_state));
    assert(mpd_client_state->mpd_state);
    mpd_client_state->dc = 0;
    mpd_shared_default_mpd_state(mpd_client_state->mpd_state);
    //init triggers;
    list_init(&mpd_client_state->triggers);
}

void free_mpd_client_state(t_mpd_client_state *mpd_client_state)
{
    sdsfree(mpd_client_state->music_directory);
    sdsfree(mpd_client_state->music_directory_value);
    sdsfree(mpd_client_state->jukebox_playlist);
    sdsfree(mpd_client_state->song_uri);
    sdsfree(mpd_client_state->last_song_uri);
    sdsfree(mpd_client_state->coverimage_name);
    sdsfree(mpd_client_state->love_channel);
    sdsfree(mpd_client_state->love_message);
    sdsfree(mpd_client_state->searchtaglist);
    sdsfree(mpd_client_state->browsetaglist);
    sdsfree(mpd_client_state->generate_pls_tags);
    sdsfree(mpd_client_state->smartpls_sort);
    sdsfree(mpd_client_state->smartpls_prefix);
    sdsfree(mpd_client_state->booklet_name);
    list_free(&mpd_client_state->jukebox_queue);
    list_free(&mpd_client_state->jukebox_queue_tmp);
    list_free(&mpd_client_state->sticker_queue);
    list_free(&mpd_client_state->triggers);
    //mpd state
    mpd_shared_free_mpd_state(mpd_client_state->mpd_state);
    free(mpd_client_state);
}

//private functions

static void detect_extra_files(t_mpd_client_state *mpd_client_state, const char *uri, sds *booklet_path, struct list *images, bool is_dirname)
{
    char *uricpy = strdup(uri);

    const char *path = is_dirname == false ? dirname(uricpy) : uri;
    sds albumpath = sdscatfmt(sdsempty(), "%s/%s", mpd_client_state->music_directory_value, path);
    LOG_DEBUG("Read extra files from albumpath: %s", albumpath);
    DIR *album_dir = opendir(albumpath);
    if (album_dir != NULL)
    {
        struct dirent *next_file;
        while ((next_file = readdir(album_dir)) != NULL)
        {
            const char *ext = strrchr(next_file->d_name, '.');
            if (strcmp(next_file->d_name, mpd_client_state->booklet_name) == 0)
            {
                LOG_DEBUG("Found booklet for uri %s", uri);
                *booklet_path = sdscatfmt(*booklet_path, "%s/%s", path, mpd_client_state->booklet_name);
            }
            else if (ext != NULL)
            {
                if (strcasecmp(ext, ".webp") == 0 || strcasecmp(ext, ".jpg") == 0 ||
                    strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".png") == 0 ||
                    strcasecmp(ext, ".tiff") == 0 || strcasecmp(ext, ".svg") == 0 ||
                    strcasecmp(ext, ".bmp") == 0)
                {
                    sds fullpath = sdscatfmt(sdsempty(), "%s/%s", path, next_file->d_name);
                    list_push(images, fullpath, 0, NULL, NULL);
                    sdsfree(fullpath);
                }
            }
        }
        closedir(album_dir);
    }
    else
    {
        LOG_ERROR("Can not open directory \"%s\" to get list of extra files: %s", albumpath, strerror(errno));
    }
    FREE_PTR(uricpy);
    sdsfree(albumpath);
}
