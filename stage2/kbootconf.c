/*  kbootcfg.c - kboot.cfg parsing

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* See http://www.kernel.org/pub/linux/kernel/people/geoff/cell/ps3-linux-docs/ps3-linux-docs-08.06.09/mini-boot-conf.txt */

#include "types.h"
#include "string.h"
#include "kbootconf.h"
#include "debug.h"

char conf_buf[MAX_KBOOTCONF_SIZE];

struct kbootconf conf;

char *strip(char *buf)
{
	while (*buf == ' ' || *buf == '\t')
		buf++;
	char *end = buf + strlen(buf) - 1;
	while (*end == ' ' || *end == '\t')
		*end-- = 0;

	return buf;
}

void split(char *buf, char **left, char **right, char delim)
{
	char *p = strchr(buf, delim);

	if (p) {
		*p = 0;
		*left = strip(buf);
		*right = strip(p+1);
	} else {
		*left = strip(buf);
		*right = NULL;
	}
}

int kbootconf_parse(void)
{
	char *lp = conf_buf;
	char *dflt = NULL;
	char *dinitrd = NULL;
	char *droot = NULL;
	char tmpbuf[MAX_CMDLINE_SIZE];
	int lineno = 1;
	int i;

	memset(&conf, 0, sizeof(conf));

	conf.timeout = -1;

	while(*lp) {
		char *newline = strchr(lp, '\n');
		char *next;
		if (newline) {
			*newline = 0;
			next = newline+1;
		} else {
			next = lp+strlen(lp);
		}

		lp = strip(lp);
		if (!*lp)
			goto nextline;

		char *left, *right;

		split(lp, &left, &right, '=');
		if (!right) {
			printf("kboot.conf: parse error (line %d)\n", lineno);
			goto nextline;
		}

		while(*right == '"' || *right == '\'')
			right++;
		char *rend = right + strlen(right) - 1;
		while(*rend == '"' || *rend == '\'')
			*rend-- = 0;

		if (!strcmp(left, "timeout")) {
			conf.timeout = my_atoi(right);
		} else if (!strcmp(left, "default")) {
			dflt = right;
		} else if (!strcmp(left, "message")) {
			conf.msgfile = right;
		} else if (!strcmp(left, "initrd")) {
			dinitrd = right;
		} else if (!strcmp(left, "root")) {
			droot = right;
		} else if (!strcmp(left, "video")) {
			// TODO: use
		} else {
			if (strlen(right) > MAX_CMDLINE_SIZE) {
				printf("kboot.conf: maximum length exceeded (line %d)\n", lineno);
				goto nextline;
			}
			conf.kernels[conf.num_kernels].label = left;
			conf.kernels[conf.num_kernels].kernel = right;
			char *p = strchr(right, ' ');
			if (!p) {
				// kernel, no arguments
				conf.num_kernels++;
				goto nextline;
			}
			*p++ = 0;
			char *buf = p;
			char *root = NULL;
			char *initrd = NULL;
			tmpbuf[0] = 0;
			/* split commandline arguments and extract the useful bits */
			while (*p) {
				char *spc = strchr(p, ' ');
				if (spc)
					*spc++ = 0;
				else
					spc = p+strlen(p);
				if (*p == 0) {
					p = spc;
					continue;
				}

				char *arg, *val;
				split(p, &arg, &val, '=');
				if (!val) {
					strlcat(tmpbuf, arg, sizeof(tmpbuf));
					strlcat(tmpbuf, " ", sizeof(tmpbuf));
				} else if (!strcmp(arg, "root")) {
					root = val;
				} else if (!strcmp(arg, "initrd")) {
					initrd = val;
				} else {
					strlcat(tmpbuf, arg, sizeof(tmpbuf));
					strlcat(tmpbuf, "=", sizeof(tmpbuf));
					strlcat(tmpbuf, val, sizeof(tmpbuf));
					strlcat(tmpbuf, " ", sizeof(tmpbuf));
				}
				
				p = spc;
			}

			int len = strlen(tmpbuf);
			if (len && tmpbuf[len-1] == ' ')
				tmpbuf[--len] = 0;
			len++;

			// UGLY: tack on initrd and root onto tmpbuf, then copy it entirely
			// on top of the original buffer (avoids having to deal with malloc)
			conf.kernels[conf.num_kernels].parameters = buf;
			if (initrd) {
				strlcpy(tmpbuf+len, initrd, sizeof(tmpbuf)-len);
				conf.kernels[conf.num_kernels].initrd = buf + len;
				len += strlen(initrd)+1;
			}
			if (root) {
				strlcpy(tmpbuf+len, root, sizeof(tmpbuf)-len);
				conf.kernels[conf.num_kernels].root = buf + len;
				len += strlen(root)+1;
			}
			memcpy(buf, tmpbuf, len);
			conf.num_kernels++;
		}

nextline:
		lp = next;
		lineno++;
	}

	conf.default_idx = 0;
	for (i=0; i<conf.num_kernels; i++) {
		if (dflt && !strcmp(conf.kernels[i].label, dflt))
			conf.default_idx = i;
		if (!conf.kernels[i].initrd && dinitrd)
			conf.kernels[i].initrd = dinitrd;
		if (!conf.kernels[i].root && droot)
			conf.kernels[i].root = droot;
		if (conf.kernels[i].initrd && !conf.kernels[i].root)
			conf.kernels[i].root = "/dev/ram0";
	}

	printf("==== kboot.conf dump ====\n");
	if (conf.timeout != -1)
		printf("Timeout: %d\n", conf.timeout);
	if (conf.msgfile)
		printf("Message: %s\n", conf.msgfile);

	for (i=0; i<conf.num_kernels; i++) {
		printf("Entry #%d '%s'", i, conf.kernels[i].label);
		if (conf.default_idx == i)
			printf(" (default):\n");
		else
			printf(":\n");
		printf(" Kernel: %s\n", conf.kernels[i].kernel);
		if (conf.kernels[i].initrd)
			printf(" Initrd: %s\n", conf.kernels[i].initrd);
		if (conf.kernels[i].root)
			printf(" Root: %s\n", conf.kernels[i].root);
		if (conf.kernels[i].parameters)
			printf(" Parameters: %s\n", conf.kernels[i].parameters);
	}

	printf("=========================\n");
	return conf.num_kernels;
}
