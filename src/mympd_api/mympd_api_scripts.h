/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __MYMPD_API_SCRIPTS_H
#define __MYMPD_API_SCRIPTS_H
#ifdef ENABLE_LUA

#define LUA_VERSION_5_3(major, minor) \
         ((5) == LUA_VERSION_MAJOR && \
          ((3) == LUA_VERSION_MINOR))

bool mympd_api_script_save(t_config *config, const char *script, int order, const char *content, const char *arguments, const char *oldscript);
bool mympd_api_script_delete(t_config *config, const char *script);
sds mympd_api_script_get(t_config *config, sds buffer, sds method, long request_id, const char *script);
sds mympd_api_script_list(t_config *config, sds buffer, sds method, long request_id, bool all);
bool mympd_api_script_start(t_config *config, const char *script, struct list *arguments, bool localscript);
bool mympd_api_get_lua_mympd_state(t_mympd_state *mympd_state, struct list *lua_mympd_state);
#endif
#endif
