/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __API_H__
#define __API_H__

//API cmds
#define MYMPD_CMDS(X) \
    X(MPD_API_UNKNOWN) \
    X(MPD_API_QUEUE_CLEAR) \
    X(MPD_API_QUEUE_CROP) \
    X(MPD_API_QUEUE_SAVE) \
    X(MPD_API_QUEUE_LIST) \
    X(MPD_API_QUEUE_SEARCH) \
    X(MPD_API_QUEUE_RM_TRACK) \
    X(MPD_API_QUEUE_RM_RANGE) \
    X(MPD_API_QUEUE_MOVE_TRACK) \
    X(MPD_API_QUEUE_ADD_TRACK_AFTER) \
    X(MPD_API_QUEUE_ADD_TRACK) \
    X(MPD_API_QUEUE_ADD_PLAY_TRACK) \
    X(MPD_API_QUEUE_REPLACE_TRACK) \
    X(MPD_API_QUEUE_ADD_PLAYLIST) \
    X(MPD_API_QUEUE_REPLACE_PLAYLIST) \
    X(MPD_API_QUEUE_SHUFFLE) \
    X(MPD_API_QUEUE_LAST_PLAYED) \
    X(MPD_API_QUEUE_ADD_RANDOM) \
    X(MPD_API_PLAYLIST_CLEAR) \
    X(MPD_API_PLAYLIST_RENAME) \
    X(MPD_API_PLAYLIST_MOVE_TRACK) \
    X(MPD_API_PLAYLIST_ADD_TRACK) \
    X(MPD_API_PLAYLIST_RM_TRACK) \
    X(MPD_API_PLAYLIST_RM_ALL) \
    X(MPD_API_PLAYLIST_RM) \
    X(MPD_API_PLAYLIST_LIST) \
    X(MPD_API_PLAYLIST_CONTENT_LIST) \
    X(MPD_API_PLAYLIST_SHUFFLE) \
    X(MPD_API_PLAYLIST_SORT) \
    X(MPD_API_SMARTPLS_UPDATE_ALL) \
    X(MPD_API_SMARTPLS_UPDATE) \
    X(MPD_API_SMARTPLS_SAVE) \
    X(MPD_API_SMARTPLS_GET) \
    X(MPD_API_DATABASE_SEARCH_ADV) \
    X(MPD_API_DATABASE_SEARCH) \
    X(MPD_API_DATABASE_UPDATE) \
    X(MPD_API_DATABASE_RESCAN) \
    X(MPD_API_DATABASE_FILESYSTEM_LIST) \
    X(MPD_API_DATABASE_TAG_LIST) \
    X(MPD_API_DATABASE_TAG_ALBUM_LIST) \
    X(MPD_API_DATABASE_TAG_ALBUM_TITLE_LIST) \
    X(MPD_API_DATABASE_STATS) \
    X(MPD_API_DATABASE_SONGDETAILS) \
    X(MPD_API_DATABASE_FINGERPRINT) \
    X(MPD_API_DATABASE_GET_ALBUMS) \
    X(MPD_API_PLAYER_PLAY_TRACK) \
    X(MPD_API_PLAYER_VOLUME_SET) \
    X(MPD_API_PLAYER_VOLUME_GET) \
    X(MPD_API_PLAYER_PAUSE) \
    X(MPD_API_PLAYER_PLAY) \
    X(MPD_API_PLAYER_STOP) \
    X(MPD_API_PLAYER_SEEK_CURRENT) \
    X(MPD_API_PLAYER_SEEK) \
    X(MPD_API_PLAYER_NEXT) \
    X(MPD_API_PLAYER_PREV) \
    X(MPD_API_PLAYER_OUTPUT_LIST) \
    X(MPD_API_PLAYER_TOGGLE_OUTPUT) \
    X(MPD_API_PLAYER_CURRENT_SONG) \
    X(MPD_API_PLAYER_STATE) \
    X(MPD_API_SETTINGS_GET) \
    X(MPD_API_LIKE) \
    X(MPD_API_LOVE) \
    X(MPD_API_ALBUMART) \
    X(MPD_API_TIMER_STARTPLAY) \
    X(MYMPD_API_COLS_SAVE) \
    X(MYMPD_API_SYSCMD) \
    X(MYMPD_API_SETTINGS_GET) \
    X(MYMPD_API_SETTINGS_SET) \
    X(MYMPD_API_SETTINGS_RESET) \
    X(MYMPD_API_CONNECTION_SAVE) \
    X(MYMPD_API_BOOKMARK_LIST) \
    X(MYMPD_API_BOOKMARK_SAVE) \
    X(MYMPD_API_BOOKMARK_RM) \
    X(MYMPD_API_BOOKMARK_CLEAR) \
    X(MYMPD_API_COVERCACHE_CROP) \
    X(MYMPD_API_COVERCACHE_CLEAR) \
    X(MYMPD_API_TIMER_SET) \
    X(MYMPD_API_TIMER_SAVE) \
    X(MYMPD_API_TIMER_LIST) \
    X(MYMPD_API_TIMER_GET) \
    X(MYMPD_API_TIMER_RM) \
    X(MYMPD_API_TIMER_TOGGLE) \
    X(MPD_API_QUEUE_MINI) \
    X(MPD_API_QUEUE_ADD_ALL_TRACKS) \
    X(MPD_API_QUEUE_ADD_PLAY_DIR) \
    X(MPD_API_QUEUE_ADD_PLAY_PLAYLIST) \
    X(MPD_API_PLAYLIST_ADD_ALL_TRACKS) \
    X(MYMPD_API_TIDAL_SEARCH) \
    X(MYMPD_API_TIDAL_SONGDETAILS) \
    X(MYMPD_API_TIDAL_ALBUMDETAILS) \
    X(MYMPD_API_TIDAL_ARTISTDETAILS) \
    X(MYMPD_API_IDEON_UPDATE)

#define GEN_ENUM(X) X,
#define GEN_STR(X) #X,

//global enums
enum mympd_cmd_ids {
    MYMPD_CMDS(GEN_ENUM)
};

//global functions
enum mympd_cmd_ids get_cmd_id(const char *cmd);
#endif
