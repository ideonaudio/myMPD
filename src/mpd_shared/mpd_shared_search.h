/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef __MPD_SHARED_SEARCH_H__
#define __MPD_SHARED_SEARCH_H__
sds mpd_shared_search(t_mpd_state *mpd_state, sds buffer, sds method, long request_id,
                      const char *searchstr, const char *searchtag, const char *plist, 
                      const unsigned int offset, const t_tags *tagcols, int max_elements_per_page);
sds mpd_shared_search_adv(t_mpd_state *mpd_state, sds buffer, sds method, long request_id,
                          const char *expression, const char *sort, const bool sortdesc, 
                          const char *grouptag, const char *plist, const unsigned int offset,
                          const t_tags *tagcols, int max_elements_per_page);
#endif