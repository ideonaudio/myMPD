/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __MPD_CLIENT_SETTINGS_H__
#define __MPD_CLIENT_SETTINGS_H__
bool mpd_api_settings_set(t_config *config, t_mpd_client_state *mpd_client_state, struct json_token *key, 
                          struct json_token *val, bool *mpd_host_changed, bool *jukebox_changed,
                          bool *check_mpd_error);
sds mpd_client_put_settings(t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id);
#endif
