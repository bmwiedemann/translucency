/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2001 Bernhard M. Wiedemann - All Rights Reserved
 *
 *   This file is part of the translucency kernel module.
 *   translucency is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License
 *   see file COPYING for more details
 *
 * ----------------------------------------------------------------------- */

#include "base.h"

static int redir_setpath(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	char backup[ps+1];
	int error;
	strncpy(backup, table->data, ps);
	error=proc_dostring(table,write,filp,buffer,lenp);
	if(write && !error) {
		release_vars();
		error=init_vars(frombuf, tobuf);
		if(error) { //no error on wrong last component
			printk(KERN_ERR "path %s not found\n",(char *)table->data);
			strncpy(table->data, backup, ps);
			error=init_vars(frombuf, tobuf);
		}
	}
	return error;
}

struct ctl_table redirection_entry_table[] = {
 {0x89194730, "uid", &uid, sizeof(uid), 0644, NULL, &proc_dointvec},
 {0x89194731, "gid", &gid, sizeof(gid), 0644, NULL, &proc_dointvec},
 {0x89194732, "from", &frombuf, ps-1, 0644, NULL, &redir_setpath},
 {0x89194733, "to", &tobuf, ps-1, 0644, NULL, &redir_setpath},
 {0x89194734, "flags", &flags, sizeof(flags), 0644, NULL, &proc_dointvec},
 {0}
};
struct ctl_table_header *redirection_table_header;
struct ctl_table redirection_table[] = {
 {0x89194729, "translucency", NULL, 0, 0555, redirection_entry_table},
 {0}
};

