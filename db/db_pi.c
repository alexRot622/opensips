/*
 * Copyright (C) 2020 OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "../str.h"
#include "../str_list.h"
#include "../mi/mi.h"
#include "../db/db.h"

struct pi_conn {
    str name;           /* the name of the connector */
    db_con_t *con;      /* database connector */
    unsigned int flags; /* different flags */
    struct str_list *tables;
    struct pi_conn *next;
} *pi_conns_list;

int db_pi_add(str *name, str *table, db_con_t *con, unsigned int flags)
{
    struct pi_conn *pi_conn, *new_pi_conn;

    LM_INFO("adding %.*s connector to PI table=%.*s\n",
            name->len, name->s, table->len, table->s);
    new_pi_conn = (struct pi_conn *) malloc(sizeof(struct pi_conn));
    if(new_pi_conn == NULL) {
        LM_ERR("Could not allocate memory for new pi_conn.\n");
        return -1;
    }
    new_pi_conn->name.s = (char *) malloc(name->len);
    if(new_pi_conn->name.s == NULL) {
        LM_ERR("Could not allocate memory for new pi_conn.\n");
        free(new_pi_conn);
        return -1;
    }
    str_cpy(&new_pi_conn->name, name);
    new_pi_conn->con = con;
    new_pi_conn->flags = flags;
    new_pi_conn->tables = (struct str_list *) malloc(sizeof(struct str_list));
    if(new_pi_conn->tables == NULL) {
        LM_ERR("Could not allocate memory for new pi_conn.\n");
        free(new_pi_conn->name.s);
        free(new_pi_conn);
        return -1;
    }
    new_pi_conn->tables->s.s = (char *) malloc(table->len);
    if(new_pi_conn->tables->s.s == NULL) {
        LM_ERR("Could not allocate memory for new pi_conn.\n");
        free(new_pi_conn->tables);
        free(new_pi_conn->name.s);
        free(new_pi_conn);
        return -1;
    }
    str_cpy(&new_pi_conn->tables->s, table);
    new_pi_conn->next = NULL;
    for(pi_conn = pi_conns_list; pi_conn != NULL; pi_conn = pi_conn->next) {
        if(pi_conn->next == NULL) {
            break;
        }
    }
    pi_conn->next = new_pi_conn;
    return 0;
}

mi_response_t *w_mi_pi_list(const mi_params_t *params,
								struct mi_handler *async_hdl)
{
    struct {
        str name;
        str table[10];
    } pi_conns[] = {
        {str_init("acc"), {str_init("acc"), str_init("missed_calls"), {0, 0}}},
        {str_init("auth_db"), {str_init("subscriber"), {0, 0}}},
        {str_init("call_center"), {str_init("call_center"), {0, 0}}},
        {str_init("domain"), {str_init("domain"), {0, 0}}},
        {str_init("dialplan"), {str_init("dialplan"), {0, 0}}},
        {str_init("rtpproxy"), {str_init("rtpproxy_sockets"), {0, 0}}},
    };
	mi_item_t *resp_obj;
	mi_item_t *arr_obj;
	mi_response_t *resp = init_mi_result_object(&resp_obj);
    struct pi_conn *pi_conn;
    struct str_list *table;

	if (!resp)
		return 0;
    for (pi_conn = pi_conns_list; pi_conn != NULL; pi_conn = pi_conn->next) {
        arr_obj = add_mi_array(resp_obj, pi_conn->name.s, pi_conn->name.len);
        for (table = pi_conn->tables; table != NULL; table = table->next) {
            add_mi_string(arr_obj, 0, 0, table->s.s, table->s.len);
        }
    }
    return resp;
}
