/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2012-2013 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/*
 * BUILD:
 *
 *      cc -o minimal minimal.c -lcouchbase
 *      cl /DWIN32 /Iinclude minimal.c lib\libcouchbase.lib
 *
 * RUN:
 *
 *      valgrind -v --tool=memcheck  --leak-check=full --show-reachable=yes ./minimal
 *      ./minimal <host:port> <bucket> <passwd>
 *      mininal.exe <host:port> <bucket> <passwd>
 */
#include <stdio.h>
#include <libcouchbase/couchbase.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#define PRIu64 "I64u"
#else
#include <inttypes.h>
#endif

#include <sys/time.h>

static inline void print_timestamp() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    printf("%ld:%d\n", tv.tv_sec, // seconds
        tv.tv_usec // microseconds
    );
}

static void error_callback(lcb_t instance, lcb_error_t error, const char *errinfo)
{
    fprintf(stderr, "ERROR: %s (0x%x), %s\n",
            lcb_strerror(instance, error), error, errinfo);
    exit(EXIT_FAILURE);
}

static void store_callback(lcb_t instance, const void *cookie,
                           lcb_storage_t operation,
                           lcb_error_t error,
                           const lcb_store_resp_t *item)
{
    if (error == LCB_SUCCESS) {
        print_timestamp();
        fprintf(stderr, "STORED \"");
        fwrite(item->v.v0.key, sizeof(char), item->v.v0.nkey, stderr);
        fprintf(stderr, "\" CAS: %"PRIu64"\n", item->v.v0.cas);
    } else {
        fprintf(stderr, "STORE ERROR: %s (0x%x)\n",
                lcb_strerror(instance, error), error);
        if (error == 0xd) {
        	fprintf(stderr, "key[");
            fwrite(item->v.v0.key, sizeof(char), item->v.v0.nkey, stderr);
            fprintf(stderr, "]\n");
        }
        exit(EXIT_FAILURE);
    }
    (void)cookie;
    (void)operation;
}

static void get_callback(lcb_t instance, const void *cookie, lcb_error_t error,
                         const lcb_get_resp_t *item)
{
    if (error == LCB_SUCCESS) {
        print_timestamp();
        fprintf(stderr, "GOT \"");
        fwrite(item->v.v0.key, sizeof(char), item->v.v0.nkey, stderr);
        fprintf(stderr, "\" CAS: %"PRIu64" FLAGS:0x%x SIZE:%lu\n",
                item->v.v0.cas, item->v.v0.flags, (unsigned long)item->v.v0.nbytes);
        fwrite(item->v.v0.bytes, sizeof(char), item->v.v0.nbytes, stderr);
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "GET ERROR: %s (0x%x)\n",
                lcb_strerror(instance, error), error);
    }
    (void)cookie;
}

lcb_error_t form_command(lcb_error_t err, lcb_t instance, const char * const key, const int count) {
	lcb_store_cmd_t cmd;
	const lcb_store_cmd_t* commands[1];
	commands[0] = &cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.v.v0.operation = LCB_SET;
	cmd.v.v0.key = "foo";
    if (key) cmd.v.v0.key = key;
	cmd.v.v0.nkey = 3;
	cmd.v.v0.bytes = "bar";
	cmd.v.v0.nbytes = 3;
    print_timestamp();
    for (int i=0; i < count; i++)
        err = lcb_store(instance, NULL, 1, commands);
	return err;
}

lcb_error_t form_command_fset(lcb_error_t err, lcb_t instance, const char *const key) {
	lcb_store_cmd_t cmd;
	const lcb_store_cmd_t* commands[1];
	commands[0] = &cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.v.v0.operation = LCB_FSET;
	cmd.v.v0.key = "fra";
	cmd.v.v0.key = key;
	cmd.v.v0.nkey = 3;
	cmd.v.v0.bytes = "123";
	cmd.v.v0.nbytes = 3;
	cmd.v.v0.flags = 1; // offset
	cmd.v.v0.exptime = (lcb_time_t)1; // length
	err = lcb_store(instance, NULL, 1, commands);
	return err;
}

lcb_error_t form_command_get(int count, lcb_error_t err, lcb_t instance,
		char* key) {
	lcb_get_cmd_t cmd;
	const lcb_get_cmd_t* commands[1];
	commands[0] = &cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.v.v0.key = "foo";
	if (key)
		cmd.v.v0.key = key;

	cmd.v.v0.nkey = 3;
	print_timestamp();
	for (int i = 0; i < count; i++) {
		err = lcb_get(instance, NULL, 1, commands);

	}
	return err;
}

int main(int argc, char *argv[])
{
    char *method = NULL;
    char *key = NULL;
    lcb_error_t err;
    lcb_t instance;
    struct lcb_create_st create_options;
    struct lcb_create_io_ops_st io_opts;

    io_opts.version = 0;
    io_opts.v.v0.type = LCB_IO_OPS_DEFAULT;
    io_opts.v.v0.cookie = NULL;

    memset(&create_options, 0, sizeof(create_options));
    err = lcb_create_io_ops(&create_options.v.v0.io, &io_opts);
    if (err != LCB_SUCCESS) {
        fprintf(stderr, "Failed to create IO instance: %s\n",
                lcb_strerror(NULL, err));
        return 1;
    }

    if (argc > 1) {
        create_options.v.v0.host = argv[1];
    }
    if (argc > 2) {
        create_options.v.v0.user = argv[2];
        create_options.v.v0.bucket = argv[2];
    }
    if (argc > 3) {
        create_options.v.v0.passwd = argv[3];
    }
    if (argc > 4) {
        method = argv[4];
    }
    if (argc > 5) {
        key = argv[5];
    }
    int count = 1;
    if (argc > 6) {
        count = atoi(argv[6]);
    }
    err = lcb_create(&instance, &create_options);
    if (err != LCB_SUCCESS) {
        fprintf(stderr, "Failed to create libcouchbase instance: %s\n",
                lcb_strerror(NULL, err));
        return 1;
    }
    (void)lcb_set_error_callback(instance, error_callback);
    /* Initiate the connect sequence in libcouchbase */
    if ((err = lcb_connect(instance)) != LCB_SUCCESS) {
        fprintf(stderr, "Failed to initiate connect: %s\n",
                lcb_strerror(NULL, err));
        lcb_destroy(instance);
        return 1;
    }
    (void)lcb_set_get_callback(instance, get_callback);
    (void)lcb_set_store_callback(instance, store_callback);
    /* Run the event loop and wait until we've connected */
    lcb_wait(instance);
    if (method == NULL || strcmp(method, "set") == 0)
    {
		err = form_command(err, instance, key, count);
        if (err != LCB_SUCCESS) {
            fprintf(stderr, "Failed to store: %s\n", lcb_strerror(NULL, err));
            return 1;
        }

        form_command(err, instance, "fra", 1);

    }
    lcb_wait(instance);

    lcb_wait(instance);
    if (method == NULL || strcmp(method, "fset") == 0)
    {
    	err = form_command_fset(err, instance, "fra");
    	if (err != LCB_SUCCESS) {
    		fprintf(stderr, "Failed to fragment store: %s\n", lcb_strerror(NULL, err));
    		return 1;
    	}
    }
    if (method == NULL || strcmp(method, "get") == 0)
    {
		err = form_command_get(count, err, instance, key);
        if (err != LCB_SUCCESS) {
            fprintf(stderr, "Failed to get: %s\n", lcb_strerror(NULL, err));
            return 1;
        }
    }
    lcb_wait(instance);
    lcb_destroy(instance);

    return 0;
}
