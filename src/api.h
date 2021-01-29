/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __API_H__
#define __API_H__

//API cmds
#define MYMPD_CMDS(X)                        \
    X(MPD_API_UNKNOWN)                       \
    X(MPD_API_QUEUE_CROP_OR_CLEAR)           \
    X(MPD_API_QUEUE_CLEAR)                   \
    X(MPD_API_QUEUE_CROP)                    \
    X(MPD_API_QUEUE_SAVE)                    \
    X(MPD_API_QUEUE_LIST)                    \
    X(MPD_API_QUEUE_SEARCH)                  \
    X(MPD_API_QUEUE_RM_TRACK)                \
    X(MPD_API_QUEUE_RM_RANGE)                \
    X(MPD_API_QUEUE_MOVE_TRACK)              \
    X(MPD_API_QUEUE_ADD_TRACK_AFTER)         \
    X(MPD_API_QUEUE_ADD_TRACK_PLAY)          \
    X(MPD_API_QUEUE_ADD_TRACK)               \
    X(MPD_API_QUEUE_REPLACE_TRACK)           \
    X(MPD_API_QUEUE_ADD_DIR_PLAY)            \
    X(MPD_API_QUEUE_ADD_PLAYLIST_PLAY)       \
    X(MPD_API_QUEUE_ADD_PLAYLIST)            \
    X(MPD_API_QUEUE_REPLACE_PLAYLIST)        \
    X(MPD_API_QUEUE_SHUFFLE)                 \
    X(MPD_API_QUEUE_LAST_PLAYED)             \
    X(MPD_API_QUEUE_ADD_RANDOM)              \
    X(MPD_API_QUEUE_MINI)                    \
    X(MPD_API_PLAYLIST_CLEAR)                \
    X(MPD_API_PLAYLIST_RENAME)               \
    X(MPD_API_PLAYLIST_MOVE_TRACK)           \
    X(MPD_API_PLAYLIST_ADD_TRACK)            \
    X(MPD_API_PLAYLIST_RM_TRACK)             \
    X(MPD_API_PLAYLIST_RM_ALL)               \
    X(MPD_API_PLAYLIST_RM)                   \
    X(MPD_API_PLAYLIST_LIST_ALL)             \
    X(MPD_API_PLAYLIST_LIST)                 \
    X(MPD_API_PLAYLIST_CONTENT_LIST)         \
    X(MPD_API_PLAYLIST_SHUFFLE)              \
    X(MPD_API_PLAYLIST_SORT)                 \
    X(MPDWORKER_API_SMARTPLS_UPDATE_ALL)     \
    X(MPDWORKER_API_SMARTPLS_UPDATE)         \
    X(MPDWORKER_API_STICKERCACHE_CREATE)     \
    X(MPD_API_STICKERCACHE_CREATED)          \
    X(MPD_API_SMARTPLS_SAVE)                 \
    X(MPD_API_SMARTPLS_GET)                  \
    X(MPD_API_DATABASE_SEARCH_ADV)           \
    X(MPD_API_DATABASE_SEARCH)               \
    X(MPD_API_DATABASE_UPDATE)               \
    X(MPD_API_DATABASE_RESCAN)               \
    X(MPD_API_DATABASE_FILESYSTEM_LIST)      \
    X(MPD_API_DATABASE_TAG_LIST)             \
    X(MPD_API_DATABASE_TAG_ALBUM_TITLE_LIST) \
    X(MPD_API_DATABASE_STATS)                \
    X(MPD_API_DATABASE_SONGDETAILS)          \
    X(MPD_API_DATABASE_FINGERPRINT)          \
    X(MPD_API_DATABASE_GET_ALBUMS)           \
    X(MPD_API_PLAYER_PLAY_TRACK)             \
    X(MPD_API_PLAYER_VOLUME_SET)             \
    X(MPD_API_PLAYER_VOLUME_GET)             \
    X(MPD_API_PLAYER_PAUSE)                  \
    X(MPD_API_PLAYER_PLAY)                   \
    X(MPD_API_PLAYER_STOP)                   \
    X(MPD_API_PLAYER_SEEK_CURRENT)           \
    X(MPD_API_PLAYER_SEEK)                   \
    X(MPD_API_PLAYER_NEXT)                   \
    X(MPD_API_PLAYER_PREV)                   \
    X(MPD_API_PLAYER_OUTPUT_LIST)            \
    X(MPD_API_PLAYER_OUTPUT_ATTRIBUTS_SET)   \
    X(MPD_API_PLAYER_TOGGLE_OUTPUT)          \
    X(MPD_API_PLAYER_CURRENT_SONG)           \
    X(MPD_API_PLAYER_STATE)                  \
    X(MPD_API_SETTINGS_GET)                  \
    X(MPD_API_LIKE)                          \
    X(MPD_API_LOVE)                          \
    X(MPD_API_MESSAGE_SEND)                  \
    X(MPD_API_URLHANDLERS)                   \
    X(MPD_API_ALBUMART)                      \
    X(MPD_API_TIMER_STARTPLAY)               \
    X(MPD_API_MOUNT_LIST)                    \
    X(MPD_API_MOUNT_MOUNT)                   \
    X(MPD_API_MOUNT_UNMOUNT)                 \
    X(MPD_API_MOUNT_NEIGHBOR_LIST)           \
    X(MPD_API_PARTITION_LIST)                \
    X(MPD_API_PARTITION_NEW)                 \
    X(MPD_API_PARTITION_SWITCH)              \
    X(MPD_API_PARTITION_RM)                  \
    X(MPD_API_PARTITION_OUTPUT_MOVE)         \
    X(MPD_API_SCRIPT_INIT)                   \
    X(MPD_API_TRIGGER_LIST)                  \
    X(MPD_API_TRIGGER_GET)                   \
    X(MPD_API_TRIGGER_SAVE)                  \
    X(MPD_API_TRIGGER_DELETE)                \
    X(MPD_API_JUKEBOX_LIST)                  \
    X(MPD_API_JUKEBOX_RM)                    \
    X(MPD_API_STATE_SAVE)                    \
    X(MYMPD_API_COLS_SAVE)                   \
    X(MYMPD_API_SYSCMD)                      \
    X(MYMPD_API_SETTINGS_GET)                \
    X(MYMPD_API_SETTINGS_SET)                \
    X(MYMPD_API_SETTINGS_RESET)              \
    X(MYMPD_API_CONNECTION_SAVE)             \
    X(MYMPD_API_BOOKMARK_LIST)               \
    X(MYMPD_API_BOOKMARK_SAVE)               \
    X(MYMPD_API_BOOKMARK_RM)                 \
    X(MYMPD_API_BOOKMARK_CLEAR)              \
    X(MYMPD_API_COVERCACHE_CROP)             \
    X(MYMPD_API_COVERCACHE_CLEAR)            \
    X(MYMPD_API_TIMER_SET)                   \
    X(MYMPD_API_TIMER_SAVE)                  \
    X(MYMPD_API_TIMER_LIST)                  \
    X(MYMPD_API_TIMER_GET)                   \
    X(MYMPD_API_TIMER_RM)                    \
    X(MYMPD_API_TIMER_TOGGLE)                \
    X(MYMPD_API_SCRIPT_INIT)                 \
    X(MYMPD_API_SCRIPT_EXECUTE)              \
    X(MYMPD_API_SCRIPT_POST_EXECUTE)         \
    X(MYMPD_API_SCRIPT_LIST)                 \
    X(MYMPD_API_SCRIPT_GET)                  \
    X(MYMPD_API_SCRIPT_SAVE)                 \
    X(MYMPD_API_SCRIPT_DELETE)               \
    X(MYMPD_API_HOME_LIST)                   \
    X(MYMPD_API_HOME_ICON_GET)               \
    X(MYMPD_API_HOME_ICON_SAVE)              \
    X(MYMPD_API_HOME_ICON_DELETE)            \
    X(MYMPD_API_HOME_ICON_MOVE)              \
    X(MYMPD_API_HOME_ICON_PICTURE_LIST)      \
    X(MYMPD_API_STATE_SAVE)                  \
    X(MYMPD_API_UPDATE_CHECK)                \
    X(MYMPD_API_UPDATE_INSTALL)              \
    X(MYMPD_API_NS_SERVER_LIST)

#define GEN_ENUM(X) X,
#define GEN_STR(X) #X,

//global enums
enum mympd_cmd_ids
{
    MYMPD_CMDS(GEN_ENUM)
};

//global functions
enum mympd_cmd_ids get_cmd_id(const char *cmd);
bool is_public_api_method(enum mympd_cmd_ids cmd_id);
#endif
