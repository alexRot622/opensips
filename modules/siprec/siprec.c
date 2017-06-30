/*
 * Copyright (C) 2017 OpenSIPS Project
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2017-06-20  created (razvanc)
 */

#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../mod_fix.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "src_sess.h"
#include "src_logic.h"

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int srec_engage(struct sip_msg *msg, char *_set, char *_media);
static int fixup_srec_engage(void **param, int param_no);
static int free_fixup_srec_engage(void **param, int param_no);
static struct mi_root* mi_example(struct mi_root* cmd_tree, void* param);
static stat_var *example_stat = 0;

/* modules dependencies */
static dep_export_t deps = {
	{ /* OpenSIPS module dependencies */
		{ MOD_TYPE_DEFAULT, "dialog", DEP_ABORT },
		{ MOD_TYPE_NULL, NULL, 0 },
	},
	{ /* modparam dependencies */
		{ "db_url", get_deps_sqldb_url },
		{ NULL, NULL },
	},
};

/* exported commands */
static cmd_export_t cmds[] = {
	{"siprec_engage",(cmd_function)srec_engage, 2, fixup_srec_engage,
		free_fixup_srec_engage, REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

/* exported statistics */
static stat_export_t stats[] = {
	{"example", STAT_NO_RESET, &example_stat},
	{0,0,0}
};

/* exported parameters */
static param_export_t params[] = {
	{0, 0, 0}
};

/* exported MI commands */
static mi_export_t mi_cmds[] = {
	{ "example", "dummy MI function used as an example",
		mi_example, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0}
};

/* module exports */
struct module_exports exports = {
	"siprec",						/* module name */
	MOD_TYPE_DEFAULT,				/* class of this module */
	MODULE_VERSION,
	DEFAULT_DLFLAGS,				/* dlopen flags */
	&deps,						    /* OpenSIPS module dependencies */
	cmds,							/* exported functions */
	0,								/* exported async functions */
	params,							/* exported parameters */
	stats,							/* exported statistics */
	mi_cmds,						/* exported MI functions */
	0,								/* exported pseudo-variables */
	0,								/* extra processes */
	0,								/* extra transformations */
	mod_init,						/* module initialization function */
	(response_function) 0,			/* response handling function */
	(destroy_function)mod_destroy,	/* destroy function */
	child_init						/* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	LM_DBG("initializing siprec module ...\n");

	if (load_dlg_api(&srec_dlg) != 0) {
		LM_ERR("dialog module not loaded! Cannot use siprec module\n");
		return -1;
	}

	if (load_tm_api(&srec_tm) != 0) {
		LM_ERR("tm module not loaded! Cannot use siprec module\n");
		return -1;
	}

	if (load_b2b_api(&srec_b2b) != 0) {
		LM_ERR("b2b_entities module not loaded! Cannot use siprec module\n");
		return -1;
	}

	if (load_rtpproxy_api(&srec_rtp) != 0) {
		LM_ERR("rtpproxy module not loaded! Cannot use siprec module\n");
		return -1;
	}

	return 0;
}

/*
 * function called when a child process starts
 */
static int child_init(int rank)
{
	return 0;
}

/*
 * function called after OpenSIPS has been stopped to cleanup resources
 */
static void mod_destroy(void)
{
}

/*
 * fixup siprec function
 */
static int fixup_srec_engage(void **param, int param_no)
{
	if (param_no > 0 && param_no < 3)
		return fixup_spve(param);
	LM_ERR("Unsupported parameter %d\n", param_no);
	return E_CFG;
}

static int free_fixup_srec_engage(void **param, int param_no)
{
	if (param_no > 0 && param_no < 3)
		return fixup_free_spve(param);
	LM_ERR("Unsupported parameter %d\n", param_no);
	return E_CFG;
}

static struct srs_set *srs_get_set(str name)
{
	struct srs_set *srs;
	static struct srs_set default_srs = {
		.name = {"default", 7},
	};
	srs = &default_srs;

	INIT_LIST_HEAD(&srs->nodes);
	static struct srs_node default_node = {
		.uri = { "sip:10.0.0.137:5080", 19 }
	};

	list_add(&default_node.list, &srs->nodes);
	return srs;
}

/*
 * function that simply prints the parameters passed
 */
static int srec_engage(struct sip_msg *msg, char *_set, char *_media)
{
	int ret;
	str set, media;
	struct src_sess *ss;
	struct srs_set *sset;
	struct dlg_cell *dlg;

	if (!_set) {
		LM_ERR("No siprec set specified!\n");
		return -1;
	}
	if (!_media) {
		LM_ERR("No media ip specified!\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_p)_media, &media) < 0) {
		LM_ERR("cannot fetch media IP!\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)_set, &set) < 0) {
		LM_ERR("cannot fetch set!\n");
		return -1;
	}

	sset = srs_get_set(set);
	if (!sset) {
		LM_ERR("Unknown siprec set %.*s\n", set.len, set.s);
		return -3;
	}

	/*
	 * TODO: check where it was called: request or reply: depending on that we
	 * can use different logics for caller/callee;
	 * for now we presume it's always on initial requests, not on replies
	 */
	/* create the dialog, if does not exist yet */
	dlg = srec_dlg.get_dlg();
	if (!dlg) {
		if (srec_dlg.create_dlg(msg, 0) < 0) {
			LM_ERR("cannot create dialog!\n");
			return -2;
		}
		dlg = srec_dlg.get_dlg();
	}

	/* check if the current dialog has a siprec session ongoing */
	ss = src_get_session(dlg);
	if (!ss && !(ss = src_create_session(dlg, sset, media))) {
		LM_ERR("cannot create siprec session!\n");
		return -2;
	}
	ret = -2;

	if (srs_add_sdp_streams(msg, &ss->sdp, 1) < 0) {
		LM_ERR("cannot add body!\n");
		goto session_cleanup;
	}

	/* TODO: cleanup after msg */
	if (srec_tm.register_tmcb(msg, 0, TMCB_RESPONSE_OUT, tm_start_recording,
			ss, 0) <= 0) {
		LM_ERR("cannot register tm callbacks\n");
		goto session_cleanup;
	}
	ret = 1;
session_cleanup:
	return ret;
}

/*
 * example of an MI function
 */
static struct mi_root* mi_example(struct mi_root* cmd_tree, void* param)
{
	struct mi_node *node = cmd_tree->node.kids;
	int i;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	for (i = 1; node; node = node->next, i++)
		LM_DBG("MI parameter no. %d is %.*s\n", i,
				node->value.len, node->value.s);
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

