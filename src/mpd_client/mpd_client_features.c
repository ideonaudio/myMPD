/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2019 Juergen Mang <mail@jcgames.de>
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
#include "../log.h"
#include "../list.h"
#include "../utility.h"
#include "../api.h"
#include "../tiny_queue.h"
#include "../global.h"
#include "config_defs.h"
#include "mpd_client_utility.h"
#include "mpd_client_state.h"
#include "mpd_client_features.h"

//private definitions
static void mpd_client_feature_commands(t_mpd_state *mpd_state);
static void mpd_client_feature_tags(t_mpd_state *mpd_state);
static void mpd_client_feature_music_directory(t_mpd_state *mpd_state);

//public functions
void mpd_client_mpd_features(t_config *config, t_mpd_state *mpd_state) {
    mpd_state->protocol = mpd_connection_get_server_version(mpd_state->conn);
    LOG_INFO("MPD protocol version: %u.%u.%u", mpd_state->protocol[0], mpd_state->protocol[1], mpd_state->protocol[2]);

    // Defaults
    mpd_state->feat_sticker = false;
    mpd_state->feat_playlists = false;
    mpd_state->feat_tags = false;
    mpd_state->feat_advsearch = false;
    mpd_state->feat_fingerprint = false;
    mpd_state->feat_smartpls = mpd_state->smartpls;;
    mpd_state->feat_coverimage = true;
    
    //get features
    mpd_client_feature_commands(mpd_state);
    mpd_client_feature_music_directory(mpd_state);
    mpd_client_feature_love(mpd_state);
    mpd_client_feature_tags(mpd_state);
    
    //set state
    sds buffer = sdsempty();
    buffer = mpd_client_put_state(config, mpd_state, buffer, NULL, 0);
    sdsfree(buffer);
    
    if (LIBMPDCLIENT_CHECK_VERSION(2, 17, 0) && mpd_connection_cmp_server_version(mpd_state->conn, 0, 21, 0) >= 0) {
        mpd_state->feat_advsearch = true;
        LOG_INFO("Enabling advanced search");
    } 
    else {
        LOG_WARN("Disabling advanced search, depends on mpd >= 0.21.0 and libmpdclient >= 2.17.0.");
    }
}

void mpd_client_feature_love(t_mpd_state *mpd_state) {
    struct mpd_pair *pair;
    mpd_state->feat_love = false;
    if (mpd_state->love == true) {
        if (mpd_send_channels(mpd_state->conn)) {
            while ((pair = mpd_recv_channel_pair(mpd_state->conn)) != NULL) {
                if (strcmp(pair->value, mpd_state->love_channel) == 0) {
                    mpd_state->feat_love = true;
                }
                mpd_return_pair(mpd_state->conn, pair);            
            }
        }
        mpd_response_finish(mpd_state->conn);
        if (mpd_state->feat_love == false) {
            LOG_WARN("Disabling featLove, channel %s not found", mpd_state->love_channel);
        }
        else {
            LOG_INFO("Enabling featLove, channel %s found", mpd_state->love_channel);
        }
    }
}

//private functions
static void mpd_client_feature_commands(t_mpd_state *mpd_state) {
    struct mpd_pair *pair;
    if (mpd_send_allowed_commands(mpd_state->conn)) {
        while ((pair = mpd_recv_command_pair(mpd_state->conn)) != NULL) {
            if (strcmp(pair->value, "sticker") == 0) {
                LOG_DEBUG("MPD supports stickers");
                mpd_state->feat_sticker = true;
            }
            else if (strcmp(pair->value, "listplaylists") == 0) {
                LOG_DEBUG("MPD supports playlists");
                mpd_state->feat_playlists = true;
            }
            else if (strcmp(pair->value, "getfingerprint") == 0) {
                LOG_DEBUG("MPD supports fingerprint command");
                if (LIBMPDCLIENT_CHECK_VERSION(2, 17, 0)) {
                    mpd_state->feat_fingerprint = true;
                }
                else {
                    LOG_DEBUG("libmpdclient don't support fingerprint command");
                }
            }
            mpd_return_pair(mpd_state->conn, pair);
        }
        mpd_response_finish(mpd_state->conn);
    }
    else {
        check_error_and_recover(mpd_state, NULL, NULL, 0);
    }
    if (mpd_state->feat_sticker == false && mpd_state->stickers == true) {
        LOG_WARN("MPD don't support stickers, disabling myMPD feature");
        mpd_state->feat_sticker = false;
    }
    if (mpd_state->feat_sticker == true && mpd_state->stickers == false) {
        mpd_state->feat_sticker = false;
    }
    if (mpd_state->feat_sticker == false && mpd_state->smartpls == true) {
        LOG_WARN("Stickers are disabled, disabling smart playlists");
        mpd_state->feat_smartpls = false;
    }
    if (mpd_state->feat_playlists == false && mpd_state->smartpls == true) {
        LOG_WARN("Playlists are disabled, disabling smart playlists");
        mpd_state->feat_smartpls = false;
    }
}

static void mpd_client_feature_tags(t_mpd_state *mpd_state) {
    sds taglist = sdsnew(mpd_state->taglist);
    sds searchtaglist = sdsnew(mpd_state->searchtaglist);
    sds searchtidaltaglist = sdsnew(mpd_state->searchtidaltaglist);
    sds searchqobuztaglist = sdsnew(mpd_state->searchqobuztaglist);
    sds browsetaglist = sdsnew(mpd_state->browsetaglist);
    sds *tokens;
    int tokens_count;
    struct mpd_pair *pair;

//    reset_t_tags(&mpd_state->mpd_tag_types);
    reset_t_tags(&mpd_state->mympd_tag_types);
    reset_t_tags(&mpd_state->search_tag_types);
    reset_t_tags(&mpd_state->search_tidal_tag_types);
    reset_t_tags(&mpd_state->search_qobuz_tag_types);
    reset_t_tags(&mpd_state->browse_tag_types);
    
    sds logline = sdsnew("MPD supported tags: ");
    if (mpd_state->mpd_tag_types.len == 0) {
        if (mpd_send_list_tag_types(mpd_state->conn)) {
            while ((pair = mpd_recv_tag_type_pair(mpd_state->conn)) != NULL) {
                enum mpd_tag_type tag = mpd_tag_name_parse(pair->value);
                if (tag != MPD_TAG_UNKNOWN) {
                    logline = sdscatfmt(logline, "%s ", pair->value);
                    mpd_state->mpd_tag_types.tags[mpd_state->mpd_tag_types.len++] = tag;
                }
                else {
                    LOG_WARN("Unknown tag %s (libmpdclient to old)", pair->value);
                }
                mpd_return_pair(mpd_state->conn, pair);
            }
            mpd_response_finish(mpd_state->conn);
        }
        else {
            check_error_and_recover(mpd_state, NULL, NULL, 0);
        }
    }
    else {
        logline = sdscat(logline, "already parsed");
    }

    if (mpd_state->mpd_tag_types.len == 0) {
        logline = sdscat(logline, "none");
        LOG_INFO(logline);
        LOG_INFO("Tags are disabled");
        mpd_state->feat_tags = false;
    }
    else {
        mpd_state->feat_tags = true;
        LOG_INFO(logline);
        logline = sdsreplace(logline, "myMPD enabled tags: ");
        tokens = sdssplitlen(taglist, sdslen(taglist), ",", 1, &tokens_count);
        for (int i = 0; i < tokens_count; i++) {
            sdstrim(tokens[i], " ");
            enum mpd_tag_type tag = mpd_tag_name_iparse(tokens[i]);
            if (tag == MPD_TAG_UNKNOWN) {
                LOG_WARN("Unknown tag %s", tokens[i]);
            }
            else {
                if (mpd_client_tag_exists(mpd_state->mpd_tag_types.tags, mpd_state->mpd_tag_types.len, tag) == true) {
                    logline = sdscatfmt(logline, "%s ", mpd_tag_name(tag));
                    mpd_state->mympd_tag_types.tags[mpd_state->mympd_tag_types.len++] = tag;
                }
                else {
                    LOG_DEBUG("Disabling tag %s", mpd_tag_name(tag));
                }
            }
        }
        sdsfreesplitres(tokens, tokens_count);
        LOG_INFO(logline);
        
        #if LIBMPDCLIENT_CHECK_VERSION(2,12,0)
        if (mpd_connection_cmp_server_version(mpd_state->conn, 0, 21, 0) >= 0) {
            LOG_VERBOSE("Enabling mpd tag types");
            if (mpd_command_list_begin(mpd_state->conn, false)) {
                mpd_send_clear_tag_types(mpd_state->conn);
                mpd_send_enable_tag_types(mpd_state->conn, mpd_state->mympd_tag_types.tags, mpd_state->mympd_tag_types.len);
                if (mpd_command_list_end(mpd_state->conn)) {
                    mpd_response_finish(mpd_state->conn);
                }
            }
            check_error_and_recover(mpd_state, NULL, NULL, 0);
        }
        #endif
        logline = sdsreplace(logline, "myMPD enabled searchtags: ");
        tokens = sdssplitlen(searchtaglist, sdslen(searchtaglist), ",", 1, &tokens_count);
        for (int i = 0; i < tokens_count; i++) {
            sdstrim(tokens[i], " ");
            enum mpd_tag_type tag = mpd_tag_name_iparse(tokens[i]);
            if (tag == MPD_TAG_UNKNOWN) {
                LOG_WARN("Unknown tag %s", tokens[i]);
            }
            else {
                if (mpd_client_tag_exists(mpd_state->mympd_tag_types.tags, mpd_state->mympd_tag_types.len, tag) == true) {
                    logline = sdscatfmt(logline, "%s ", mpd_tag_name(tag));
                    mpd_state->search_tag_types.tags[mpd_state->search_tag_types.len++] = tag;
                }
                else {
                    LOG_DEBUG("Disabling tag %s", mpd_tag_name(tag));
                }
            }
        }
        sdsfreesplitres(tokens, tokens_count);
        LOG_INFO(logline);

        logline = sdsreplace(logline, "myMPD enabled searchtidaltags: ");
        tokens = sdssplitlen(searchtidaltaglist, sdslen(searchtidaltaglist), ",", 1, &tokens_count);
        for (int i = 0; i < tokens_count; i++) {
            sdstrim(tokens[i], " ");
            enum mpd_tag_type tag = mpd_tag_name_iparse(tokens[i]);
            if (tag == MPD_TAG_UNKNOWN) {
                LOG_WARN("Unknown tag %s", tokens[i]);
            }
            else {
                if (mpd_client_tag_exists(mpd_state->mympd_tag_types.tags, mpd_state->mympd_tag_types.len, tag) == true) {
                    logline = sdscatfmt(logline, "%s ", mpd_tag_name(tag));
                    mpd_state->search_tidal_tag_types.tags[mpd_state->search_tidal_tag_types.len++] = tag;
                }
                else {
                    LOG_DEBUG("Disabling tag %s", mpd_tag_name(tag));
                }
            }
        }
        sdsfreesplitres(tokens, tokens_count);
        LOG_INFO(logline);
        
        logline = sdsreplace(logline, "myMPD enabled searchqobuztags: ");
        tokens = sdssplitlen(searchqobuztaglist, sdslen(searchqobuztaglist), ",", 1, &tokens_count);
        for (int i = 0; i < tokens_count; i++) {
            sdstrim(tokens[i], " ");
            enum mpd_tag_type tag = mpd_tag_name_iparse(tokens[i]);
            if (tag == MPD_TAG_UNKNOWN) {
                LOG_WARN("Unknown tag %s", tokens[i]);
            }
            else {
                if (mpd_client_tag_exists(mpd_state->mympd_tag_types.tags, mpd_state->mympd_tag_types.len, tag) == true) {
                    logline = sdscatfmt(logline, "%s ", mpd_tag_name(tag));
                    mpd_state->search_qobuz_tag_types.tags[mpd_state->search_qobuz_tag_types.len++] = tag;
                }
                else {
                    LOG_DEBUG("Disabling tag %s", mpd_tag_name(tag));
                }
            }
        }
        sdsfreesplitres(tokens, tokens_count);
        LOG_INFO(logline);

        logline = sdsreplace(logline, "myMPD enabled browsetags: ");
        tokens = sdssplitlen(browsetaglist, sdslen(browsetaglist), ",", 1, &tokens_count);
        for (int i = 0; i < tokens_count; i++) {
            sdstrim(tokens[i], " ");
            enum mpd_tag_type tag = mpd_tag_name_iparse(tokens[i]);
            if (tag == MPD_TAG_UNKNOWN) {
                LOG_WARN("Unknown tag %s", tokens[i]);
            }
            else {
                if (mpd_client_tag_exists(mpd_state->mympd_tag_types.tags, mpd_state->mympd_tag_types.len, tag) == true) {
                    logline = sdscatfmt(logline, "%s ", mpd_tag_name(tag));
                    mpd_state->browse_tag_types.tags[mpd_state->browse_tag_types.len++] = tag;
                }
                else {
                    LOG_DEBUG("Disabling tag %s", mpd_tag_name(tag));
                }
            }
        }
        sdsfreesplitres(tokens, tokens_count);
        LOG_INFO(logline);
    }
    sdsfree(logline);
    sdsfree(taglist);
    sdsfree(searchtaglist);
    sdsfree(searchtidaltaglist);
    sdsfree(searchqobuztaglist);
    sdsfree(browsetaglist);
}

static void mpd_client_feature_music_directory(t_mpd_state *mpd_state) {
    struct mpd_pair *pair;
    mpd_state->feat_library = false;
    mpd_state->feat_coverimage = mpd_state->coverimage;
    mpd_state->music_directory_value = sdscrop(mpd_state->music_directory_value);

    if (strncmp(mpd_state->mpd_host, "/", 1) == 0 && strncmp(mpd_state->music_directory, "auto", 4) == 0) {
        //get musicdirectory from mpd
        if (mpd_send_command(mpd_state->conn, "config", NULL)) {
            while ((pair = mpd_recv_pair(mpd_state->conn)) != NULL) {
                if (strcmp(pair->name, "music_directory") == 0) {
                    if (strncmp(pair->value, "smb://", 6) != 0 && strncmp(pair->value, "nfs://", 6) != 0) {
                        mpd_state->music_directory_value = sdsreplace(mpd_state->music_directory_value, pair->value);
                    }
                }
                mpd_return_pair(mpd_state->conn, pair);
            }
            mpd_response_finish(mpd_state->conn);
        }
        else {
            check_error_and_recover(mpd_state, NULL, NULL, 0);
        }
    }
    else if (strncmp(mpd_state->music_directory, "/", 1) == 0) {
        mpd_state->music_directory_value = sdsreplace(mpd_state->music_directory_value, mpd_state->music_directory);
    }
    else {
        //none or garbage, empty music_directory_value
    }
    
    //set feat_library
    if (sdslen(mpd_state->music_directory_value) == 0) {
        LOG_WARN("Disabling featLibrary support");
        mpd_state->feat_library = false;
    }
    else if (testdir("MPD music_directory", mpd_state->music_directory_value, false) == 0) {
        LOG_INFO("Enabling featLibrary support");
        mpd_state->feat_library = true;
    }
    else {
        LOG_WARN("Disabling featLibrary support");
        mpd_state->feat_library = false;
        mpd_state->music_directory_value = sdscrop(mpd_state->music_directory_value);
    }
    
    if (mpd_state->feat_library == false) {
        LOG_WARN("Disabling coverimage support");
        mpd_state->feat_coverimage = false;
    }

    //push music_directory setting to web_server_queue
    t_work_result *web_server_response = (t_work_result *)malloc(sizeof(t_work_result));
    assert(web_server_response);
    web_server_response->conn_id = -1;
    
    sds data = sdsnew("{");
    data = tojson_char(data, "musicDirectory", mpd_state->music_directory_value, true);
    data = tojson_bool(data, "featLibrary", mpd_state->feat_library, false);
    data = sdscat(data, "}");
    web_server_response->data = data;
    tiny_queue_push(web_server_queue, web_server_response);
}
