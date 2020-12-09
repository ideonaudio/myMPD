/*
 SPDX-License-Identifier: GPL-2.0-or-later
 myMPD (c) 2018-2020 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpd/client.h>

#include "../../dist/src/sds/sds.h"
#include "../sds_extras.h"
#include "../api.h"
#include "../log.h"
#include "../list.h"
#include "config_defs.h"
#include "../utility.h"
#include "../mpd_shared/mpd_shared_typedefs.h"
#include "../mpd_shared.h"
#include "mpd_client_utility.h"
#include "mpd_client_partitions.h"

//public functions
sds mpd_client_put_partitions(t_mpd_client_state *mpd_client_state, sds buffer, sds method, long request_id) {
    bool rc = mpd_send_listpartitions(mpd_client_state->mpd_state->conn);
    if (check_rc_error_and_recover(mpd_client_state->mpd_state, &buffer, method, request_id, false, rc, "mpd_send_listpartitions") == false) {
        return buffer;
    }
        
    buffer = jsonrpc_start_result(buffer, method, request_id);
    buffer = sdscat(buffer, ",\"data\":[");
    unsigned entity_count = 0;
    struct mpd_pair *partition;
    while ((partition = mpd_recv_partition_pair(mpd_client_state->mpd_state->conn)) != NULL) {
        if (entity_count++) {
            buffer = sdscat(buffer, ",");
        }
        buffer = sdscat(buffer, "{");
        buffer = tojson_char(buffer, "name", partition->value, false);
        buffer = sdscat(buffer, "}");
        mpd_return_pair(mpd_client_state->mpd_state->conn, partition);
    }

    buffer = sdscat(buffer, "],");
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entity_count, false);
    buffer = jsonrpc_end_result(buffer);
    
    mpd_response_finish(mpd_client_state->mpd_state->conn);
    if (check_error_and_recover2(mpd_client_state->mpd_state, &buffer, method, request_id, false) == false) {
        return buffer;
    }
    
    return buffer;
}
