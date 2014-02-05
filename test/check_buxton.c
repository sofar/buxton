/*
 * This file is part of buxton.
 *
 * Copyright (C) 2013 Intel Corporation
 *
 * buxton is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <check.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "backend.h"
#include "buxton.h"
#include "buxtonresponse.h"
#include "check_utils.h"
#include "direct.h"
#include "protocol.h"
#include "serialize.h"
#include "util.h"

#ifdef NDEBUG
#error "re-run configure with --enable-debug"
#endif

START_TEST(buxton_direct_open_check)
{
	BuxtonControl c;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	buxton_direct_close(&c);
}
END_TEST

START_TEST(buxton_direct_set_value_check)
{
	BuxtonControl c;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	BuxtonData data;
	_BuxtonKey key;
	key.layer = buxton_string_pack("test-gdbm");
	key.group = buxton_string_pack("bxt_test");
	key.name.value = NULL;
	key.type = STRING;

	c.client.uid = getuid();
	data.type = STRING;
	data.store.d_string = buxton_string_pack("bxt_test_value");
	fail_if(buxton_direct_set_value(&c, &key, &data, NULL) == false,
		"Setting value in buxton directly failed.");
	buxton_direct_close(&c);
}
END_TEST

START_TEST(buxton_direct_get_value_for_layer_check)
{
	BuxtonControl c;
	BuxtonData result;
	BuxtonString dlabel;
	_BuxtonKey key;
	key.layer = buxton_string_pack("test-gdbm");
	key.group = buxton_string_pack("bxt_test");
	key.name.value = NULL;
	key.type = STRING;

	c.client.uid = getuid();
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	fail_if(buxton_direct_get_value_for_layer(&c, &key, &result, &dlabel, NULL) == false,
		"Retrieving value from buxton gdbm backend failed.");
	fail_if(result.type != STRING,
		"Buxton gdbm backend returned incorrect result type.");
	//FIXME get label test figured out
	fail_if(strcmp(result.store.d_string.value, "bxt_test_value") != 0,
		"Buxton gdbm returned a different value to that set.");
	if (result.store.d_string.value)
		free(result.store.d_string.value);
	buxton_direct_close(&c);
}
END_TEST

START_TEST(buxton_direct_get_value_check)
{
	BuxtonControl c;
	BuxtonData data, result;
	BuxtonString dlabel;
	_BuxtonKey key;
	key.layer = buxton_string_pack("test-gdbm-user");
	key.group = buxton_string_pack("bxt_test");
	key.name.value = NULL;
	key.type = STRING;

	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");

	c.client.uid = getuid();
	data.type = STRING;
	data.store.d_string = buxton_string_pack("bxt_test_value2");
	fail_if(data.store.d_string.value == NULL,
		"Failed to allocate test string.");
	fail_if(buxton_direct_set_value(&c, &key, &data, NULL) == false,
		"Failed to set second value.");
	fail_if(buxton_direct_get_value(&c, &key, &result, &dlabel, NULL) == false,
		"Retrieving value from buxton gdbm backend failed.");
	fail_if(result.type != STRING,
		"Buxton gdbm backend returned incorrect result type.");
	//FIXME figure out label check
	fail_if(strcmp(result.store.d_string.value, "bxt_test_value2") != 0,
		"Buxton gdbm returned a different value to that set.");
	if (result.store.d_string.value)
		free(result.store.d_string.value);
	buxton_direct_close(&c);
}
END_TEST

START_TEST(buxton_memory_backend_check)
{
	BuxtonControl c;
	BuxtonData data, result;
	BuxtonString dlabel;
	_BuxtonKey key;
	key.layer = buxton_string_pack("temp");
	key.group = buxton_string_pack("bxt_mem_test");
	key.name.value = NULL;
	key.type = STRING;

	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");

	c.client.uid = getuid();
	data.type = STRING;
	data.store.d_string = buxton_string_pack("bxt_test_value");
	fail_if(buxton_direct_set_value(&c, &key, &data, NULL) == false,
		"Setting value in buxton memory backend directly failed.");
	fail_if(buxton_direct_get_value_for_layer(&c, &key, &result, &dlabel, NULL) == false,
		"Retrieving value from buxton memory backend directly failed.");
	//FIXME figure out label check
	fail_if(!streq(result.store.d_string.value, "bxt_test_value"),
		"Buxton memory returned a different value to that set.");
	buxton_direct_close(&c);
}
END_TEST

START_TEST(buxton_key_check)
{
	char *group = "group";
	char *name = "name";
	char *layer = "layer";
	BuxtonKey key;
	char *g;
	char *n;
	char *l;
	BuxtonDataType type = STRING;

	key = buxton_make_key(group, name, layer, type);
	fail_if(!key, "Failed to create buxton key");
	g = buxton_get_group(key);
	n = buxton_get_name(key);
	l = buxton_get_layer(key);
	fail_if(!g, "Failed to get group from key");
	fail_if(!n, "Failed to get name from key");
	fail_if(!l, "Failed to get layer from key");
	fail_if(buxton_get_type(key) != type,
		"Failed to get correct type from key");
	fail_if(!streq(g, group), "Got different group back from key");
	fail_if(!streq(n, name), "Got different name back from key");
	fail_if(!streq(l, layer), "Got different layer back from key");
	free(g);
	free(n);
	free(l);
	buxton_free_key(key);

	fail_if(buxton_make_key(NULL, name, layer, type), "Got key back with invalid group");
	fail_if(buxton_make_key(group, name, layer, BUXTON_TYPE_MIN),
		"Got key back with invalid type 1");
	fail_if(buxton_make_key(group, name, layer, BUXTON_TYPE_MAX),
		"Got key back with invalid type 2");

	key = buxton_make_key(group, NULL, layer, type);
	fail_if(!key, "Failed to create buxton key with empty name");
	fail_if(buxton_get_name(key), "Got name back with no name key");
	buxton_free_key(key);

	key = buxton_make_key(group, name, NULL, type);
	fail_if(!key, "Failed to create buxton key with empty layer");
	fail_if(buxton_get_layer(key), "Got layer back with no layer key");
	buxton_free_key(key);
}
END_TEST

START_TEST(buxton_set_label_check)
{
	BuxtonControl c;
	BuxtonString label = buxton_string_pack("System");
	_BuxtonKey key;
	key.layer = buxton_string_pack("test-gdbm");
	key.group = buxton_string_pack("bxt_test");
	key.name.value = NULL;
	key.type = STRING;

	c.client.uid = 0;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	fail_if(buxton_direct_set_label(&c, &key, &label) == false,
		"Failed to set label as root user.");

	c.client.uid = 1000;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	fail_if(buxton_direct_set_label(&c, &key, &label) == true,
		"Able to set label as non-root user.");

	buxton_direct_close(&c);
}
END_TEST

START_TEST(buxton_group_label_check)
{
	BuxtonControl c;
	BuxtonData result;
	BuxtonString dlabel;
	BuxtonString label = buxton_string_pack("System");
	_BuxtonKey key;
	key.layer = buxton_string_pack("test-gdbm");
	key.group = buxton_string_pack("test-group");
	key.name.value = NULL;
	key.type = STRING;


	c.client.uid = 0;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	fail_if(buxton_direct_set_label(&c, &key, &label) == false,
		"Failed to set group label.");
	fail_if(buxton_direct_get_value_for_layer(&c, &key, &result, &dlabel, NULL) == false,
		"Retrieving group label failed.");
	fail_if(!streq("System", dlabel.value),
		"Retrieved group label is incorrect.");

	free(dlabel.value);
	buxton_direct_close(&c);
}
END_TEST

START_TEST(buxton_name_label_check)
{
	BuxtonControl c;
	BuxtonData data, result;
	BuxtonString label, dlabel;
	_BuxtonKey key;

	/* create the group first, and validate the label */
	key.layer = buxton_string_pack("test-gdbm");
	key.group = buxton_string_pack("group-foo");
	key.name.value = NULL;
	label = buxton_string_pack("System");

	c.client.uid = 0;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	fail_if(buxton_direct_set_label(&c, &key, &label) == false,
		"Failed to set group label.");
	fail_if(buxton_direct_get_value_for_layer(&c, &key, &result, &dlabel, NULL) == false,
		"Retrieving group label failed.");
	fail_if(!streq("System", dlabel.value),
		"Retrieved group label is incorrect.");
	free(dlabel.value);
	free(result.store.d_string.value);

	/* then create the name (key), and validate the label */
	key.name = buxton_string_pack("name-foo");
	data.type = STRING;
	data.store.d_string = buxton_string_pack("value1-foo");
	fail_if(buxton_direct_set_value(&c, &key, &data, NULL) == false,
		"Failed to set key name-foo.");
	fail_if(buxton_direct_get_value_for_layer(&c, &key, &result, &dlabel, NULL) == false,
		"Failed to get value for name-foo 1");
	fail_if(!streq("value1-foo", result.store.d_string.value),
		"Retrieved key value is incorrect 1");
	fail_if(!streq(dlabel.value, "_"), "Failed to set default label");
	free(dlabel.value);
	free(result.store.d_string.value);
	fail_if(buxton_direct_set_label(&c, &key, &label) == false,
		"Failed to set name label.");
	fail_if(buxton_direct_get_value_for_layer(&c, &key, &result, &dlabel, NULL) == false,
		"Failed to get value for name-foo 2");
	fail_if(!streq("value1-foo", result.store.d_string.value),
		"Retrieved key value is incorrect 2");
	fail_if(!streq("System", dlabel.value),
		"Retrieved key label is incorrect.");
	free(dlabel.value);
	free(result.store.d_string.value);

	/* modify the same key, with a new value, and validate the label */
	data.store.d_string = buxton_string_pack("value2-foo");
	fail_if(buxton_direct_set_value(&c, &key, &data, NULL) == false,
		"Failed to modify key name-foo.");
	fail_if(buxton_direct_get_value_for_layer(&c, &key, &result, &dlabel, NULL) == false,
		"Failed to get new value for name-foo.");
	fail_if(!streq("value2-foo", result.store.d_string.value),
		"New key value is incorrect.");
	fail_if(!streq("System", dlabel.value),
		"Key label has been modified.");

	/* modify the key label directly once it has been created */
	fail_if(buxton_direct_set_label(&c, &key, &label) == false,
		"Failed to modify label on key.");

	free(dlabel.value);
	free(result.store.d_string.value);

	buxton_direct_close(&c);
}
END_TEST

static int run_callback_test_value = 0;
static void run_callback_cb_test(BuxtonResponse response, void *data)
{
	_BuxtonResponse *r = (_BuxtonResponse *)response;
	BuxtonData *d;
	bool *b;

	fail_if(r->type != BUXTON_CONTROL_SET, "Unexpected response type");

	switch (run_callback_test_value) {
	case 0:
		/* first pass through */
		run_callback_test_value = 1;
		break;
	case 1:
		/* second pass through */
		fail_if(r->data->len != 1, "Failed setup array size");
		d = buxton_array_get(r->data, 0);
		fail_if(!d, "Failed to set array element");
		fail_if(d->type != INT32, "Failed to setup array element value");
		run_callback_test_value = 2;
		break;
	case 2:
		/* third pass through */
		b = (bool *)data;
		*b = true;
		break;
	default:
		fail("Unexpected test value");
		break;
	}
}
START_TEST(run_callback_check)
{
	bool data = false;
	BuxtonData list[] = {
		{INT32, {.d_int32 = 1}}
	};

	run_callback(NULL, (void *)&data, 1, list, BUXTON_CONTROL_SET);
	run_callback(run_callback_cb_test, NULL, 0, NULL, BUXTON_CONTROL_SET);
	fail_if(run_callback_test_value != 1,
		"Failed to update callback test value 1");
	run_callback(run_callback_cb_test, NULL, 1, list, BUXTON_CONTROL_SET);
	fail_if(run_callback_test_value != 2,
		"Failed to update callback test value 2");
	run_callback(run_callback_cb_test, (void *)&data, 0, NULL, BUXTON_CONTROL_SET);
	fail_if(!data, "Failed to update callback test value 3");
}
END_TEST

START_TEST(send_message_check)
{
	_BuxtonClient client;
	BuxtonArray *out_list = NULL;
	BuxtonData *list = NULL;
	int server;
	uint8_t *dest = NULL;
	uint8_t *source = NULL;
	size_t size;
	BuxtonData data;

	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	fail_if(!setup_callbacks(),
		"Failed to setup callbacks");

	out_list = buxton_array_new();
	data.type = INT32;
	data.store.d_int32 = 0;
	fail_if(!buxton_array_add(out_list, &data),
		"Failed to add get response to array");
	size = buxton_serialize_message(&source, BUXTON_CONTROL_STATUS,
					0, out_list);
	fail_if(size == 0, "Failed to serialize message");
	fail_if(!send_message(&client, source, size, NULL, NULL, 0,
			      BUXTON_CONTROL_STATUS),
		"Failed to write message 1");

	cleanup_callbacks();
	buxton_array_free(&out_list, NULL);
	free(dest);
	free(list);
	close(server);
	close(client.fd);
}
END_TEST

static void handle_response_cb_test(_BuxtonResponse *response, void *data)
{
	bool *val = (bool *)data;
	fail_if(!(*val), "Got unexpected response data");
	*val = false;
}
START_TEST(handle_callback_response_check)
{
	_BuxtonClient client;
	BuxtonArray *out_list = NULL;
	uint8_t *dest = NULL;
	int server;
	size_t size;
	bool test_data;
	BuxtonData data;
	BuxtonData good[] = {
		{INT32, {.d_int32 = 0}}
	};
	BuxtonData good_unnotify[] = {
		{INT32,	 {.d_int32 = 0	 }},
		{STRING, {.d_string = {0}}},
		{INT64,	 {.d_int64 = 4	 }}
	};
	BuxtonData bad1[] = {
		{INT64, {.d_int64 = 1}}
	};
	BuxtonData bad2[] = {
		{INT32, {.d_int32 = 1}}
	};

	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	/* done just to create a callback to be used */
	fail_if(!setup_callbacks(),
		"Failed to initialeze response callbacks");
	out_list = buxton_array_new();
	data.type = INT32;
	data.store.d_int32 = 0;
	fail_if(!buxton_array_add(out_list, &data),
		"Failed to add data to array");
	size = buxton_serialize_message(&dest, BUXTON_CONTROL_STATUS,
					0, out_list);
	buxton_array_free(&out_list, NULL);
	fail_if(size == 0, "Failed to serialize message");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 1, BUXTON_CONTROL_SET),
		"Failed to send message 1");
	handle_callback_response(BUXTON_CONTROL_STATUS, 1, bad1, 1);
	fail_if(test_data, "Failed to set cb data non notify type");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 2, BUXTON_CONTROL_NOTIFY),
		"Failed to send message 2");
	handle_callback_response(BUXTON_CONTROL_STATUS, 2, bad1, 1);
	fail_if(test_data, "Failed to set notify bad1 data");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 3, BUXTON_CONTROL_NOTIFY),
		"Failed to send message 3");
	handle_callback_response(BUXTON_CONTROL_STATUS, 3, bad2, 1);
	fail_if(test_data, "Failed to set notify bad2 data");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 4, BUXTON_CONTROL_NOTIFY),
		"Failed to send message 4");
	handle_callback_response(BUXTON_CONTROL_STATUS, 4, good, 1);
	fail_if(!test_data, "Set notify good data");

	/* ensure we run callback on duplicate msgid */
	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 4, BUXTON_CONTROL_NOTIFY),
		"Failed to send message 4-2");
	handle_callback_response(BUXTON_CONTROL_STATUS, 4, good, 1);
	fail_if(test_data, "Failed to set notify duplicate msgid");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 5, BUXTON_CONTROL_NOTIFY),
		"Failed to send message 5");
	handle_callback_response(BUXTON_CONTROL_CHANGED, 4, good, 1);
	fail_if(test_data, "Failed to set changed data");

	/* ensure we don't remove callback on changed */
	test_data = true;
	handle_callback_response(BUXTON_CONTROL_CHANGED, 4, good, 1);
	fail_if(test_data, "Failed to set changed data");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 6, BUXTON_CONTROL_UNNOTIFY),
		"Failed to send message 6");
	printf("non_notify start\n");
	handle_callback_response(BUXTON_CONTROL_STATUS, 6, bad1, 1);
	fail_if(test_data, "Failed to set unnotify bad1 data");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 7, BUXTON_CONTROL_UNNOTIFY),
		"Failed to send message 7");
	handle_callback_response(BUXTON_CONTROL_STATUS, 7, bad2, 1);
	fail_if(test_data, "Failed to set unnotify bad2 data");

	test_data = true;
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 8, BUXTON_CONTROL_UNNOTIFY),
		"Failed to send message 8");
	handle_callback_response(BUXTON_CONTROL_STATUS, 8, good_unnotify, 1);
	fail_if(!test_data, "Set unnotify good data");

	test_data = true;
	handle_callback_response(BUXTON_CONTROL_CHANGED, 4, good, 1);
	fail_if(!test_data, "Didn't remove changed callback");

	cleanup_callbacks();
	free(dest);
	close(client.fd);
	close(server);
}
END_TEST

START_TEST(buxton_wire_handle_response_check)
{
	_BuxtonClient client;
	BuxtonArray *out_list = NULL;
	int server;
	uint8_t *dest = NULL;
	size_t size;
	BuxtonData data;
	bool test_data = true;

	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	/* done just to create a callback to be used */
	fail_if(!setup_callbacks(),
		"Failed to initialeze get response callbacks");
	out_list = buxton_array_new();
	data.type = INT32;
	data.store.d_int32 = 0;
	fail_if(!buxton_array_add(out_list, &data),
		"Failed to add get response to array");
	size = buxton_serialize_message(&dest, BUXTON_CONTROL_STATUS,
					0, out_list);
	buxton_array_free(&out_list, NULL);
	fail_if(size == 0, "Failed to serialize message");
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 0, BUXTON_CONTROL_STATUS),
		"Failed to send message");

	/* server */
	fail_if(!_write(server, dest, size),
		"Failed to send get response");

	/* client */
	fail_if(buxton_wire_handle_response(&client) != 1,
		"Failed to handle response correctly");
	fail_if(test_data, "Failed to update data");

	cleanup_callbacks();
	free(dest);
	close(client.fd);
	close(server);
}
END_TEST

START_TEST(buxton_wire_get_response_check)
{
	_BuxtonClient client;
	BuxtonArray *out_list = NULL;
	int server;
	uint8_t *dest = NULL;
	size_t size;
	BuxtonData data;
	bool test_data = true;

	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	/* done just to create a callback to be used */
	fail_if(!setup_callbacks(),
		"Failed to initialeze callbacks");
	out_list = buxton_array_new();
	data.type = INT32;
	data.store.d_int32 = 0;
	fail_if(!buxton_array_add(out_list, &data),
		"Failed to add data to array");
	size = buxton_serialize_message(&dest, BUXTON_CONTROL_STATUS,
					0, out_list);
	buxton_array_free(&out_list, NULL);
	fail_if(size == 0, "Failed to serialize message");
	fail_if(!send_message(&client, dest, size, handle_response_cb_test,
			      &test_data, 0, BUXTON_CONTROL_STATUS),
		"Failed to send message");

	/* server */
	fail_if(!_write(server, dest, size),
		"Failed to send get response");

	/* client */
	fail_if(!buxton_wire_get_response(&client),
		"Failed to handle response correctly");
	fail_if(test_data, "Failed to update data");

	cleanup_callbacks();
	free(dest);
	close(client.fd);
	close(server);
}
END_TEST

START_TEST(buxton_wire_set_value_check)
{
	_BuxtonClient client;
	int server;
	size_t size;
	BuxtonData *list = NULL;
	uint8_t buf[4096];
	ssize_t r;
	_BuxtonKey key;
	BuxtonControlMessage msg;
	uint64_t msgid;

	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	fail_if(!setup_callbacks(),
		"Failed to initialeze callbacks");

	key.layer = buxton_string_pack("layer");
	key.group = buxton_string_pack("group");
	key.name = buxton_string_pack("name");
	key.type = STRING;

	fail_if(buxton_wire_set_value(&client, &key, "value", NULL,
				      NULL) != true,
		"Failed to properly set value");

	r = read(server, buf, 4096);
	fail_if(r < 0, "Read from client failed");
	size = buxton_deserialize_message(buf, &msg, (size_t)r, &msgid, &list);
	fail_if(size != 4, "Failed to get valid message from buffer");
	fail_if(msg != BUXTON_CONTROL_SET,
		"Failed to get correct control type");
	fail_if(list[0].type != STRING, "Failed to set correct layer type");
	fail_if(list[1].type != STRING, "Failed to set correct group type");
	fail_if(list[2].type != STRING, "Failed to set correct name type");
	fail_if(list[3].type != STRING, "Failed to set correct value type");
	fail_if(!streq(list[0].store.d_string.value, "layer"),
		"Failed to set correct layer");
	fail_if(!streq(list[1].store.d_string.value, "group"),
		"Failed to set correct group");
	fail_if(!streq(list[2].store.d_string.value, "name"),
		"Failed to set correct name");
	fail_if(!streq(list[3].store.d_string.value, "value"),
		"Failed to set correct value");

	free(list[0].store.d_string.value);
	free(list[1].store.d_string.value);
	free(list[2].store.d_string.value);
	free(list[3].store.d_string.value);
	free(list);
	cleanup_callbacks();
	close(client.fd);
	close(server);
}
END_TEST

START_TEST(buxton_wire_set_label_check)
{
	_BuxtonClient client;
	int server;
	size_t size;
	BuxtonData *list = NULL;
	uint8_t buf[4096];
	ssize_t r;
	_BuxtonKey key;
	BuxtonString value;
	BuxtonControlMessage msg;
	uint64_t msgid;

	client.uid = 0;
	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	fail_if(!setup_callbacks(),
		"Failed to initialize callbacks");

	key.layer = buxton_string_pack("layer");
	key.group = buxton_string_pack("group");
	key.name.value = NULL;
	value = buxton_string_pack("_");
	fail_if(buxton_wire_set_label(&client, &key, &value, NULL,
				      NULL) != true,
		"Failed to properly set label");

	r = read(server, buf, 4096);
	fail_if(r < 0, "Read from client failed");
	size = buxton_deserialize_message(buf, &msg, (size_t)r, &msgid, &list);
	fail_if(size != 3, "Failed to get valid message from buffer");
	fail_if(msg != BUXTON_CONTROL_SET_LABEL,
		"Failed to get correct control type");
	fail_if(list[0].type != STRING, "Failed to set correct layer type");
	fail_if(list[1].type != STRING, "Failed to set correct group type");
	fail_if(list[2].type != STRING, "Failed to set correct label type");
	fail_if(!streq(list[0].store.d_string.value, "layer"),
		"Failed to set correct layer");
	fail_if(!streq(list[1].store.d_string.value, "group"),
		"Failed to set correct group");
	fail_if(!streq(list[2].store.d_string.value, "_"),
		"Failed to set correct label");

	free(list[0].store.d_string.value);
	free(list[1].store.d_string.value);
	free(list[2].store.d_string.value);
	free(list);
	cleanup_callbacks();
	close(client.fd);
	close(server);
}
END_TEST

START_TEST(buxton_wire_get_value_check)
{
	_BuxtonClient client;
	int server;
	size_t size;
	BuxtonData *list = NULL;
	uint8_t buf[4096];
	ssize_t r;
	_BuxtonKey key;
	BuxtonControlMessage msg;
	uint64_t msgid;

	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	fail_if(!setup_callbacks(),
		"Failed to initialeze callbacks");

	key.layer = buxton_string_pack("layer");
	key.group = buxton_string_pack("group");
	key.name = buxton_string_pack("name");
	key.type = STRING;
	fail_if(buxton_wire_get_value(&client, &key, NULL,
				      NULL) != true,
		"Failed to properly get value 1");

	r = read(server, buf, 4096);
	fail_if(r < 0, "Read from client failed 1");
	size = buxton_deserialize_message(buf, &msg, (size_t)r, &msgid, &list);
	fail_if(size != 4, "Failed to get valid message from buffer 1");
	fail_if(msg != BUXTON_CONTROL_GET,
		"Failed to get correct control type 1");
	fail_if(list[0].type != STRING, "Failed to set correct layer type 1");
	fail_if(list[1].type != STRING, "Failed to set correct group type 1");
	fail_if(list[2].type != STRING, "Failed to set correct name type 1");
	fail_if(list[3].type != UINT32, "Failed to set correct type type 1");
	fail_if(!streq(list[0].store.d_string.value, "layer"),
		"Failed to set correct layer 1");
	fail_if(!streq(list[1].store.d_string.value, "group"),
		"Failed to set correct group 1");
	fail_if(!streq(list[2].store.d_string.value, "name"),
		"Failed to set correct name 1");
	fail_if(list[3].store.d_uint32 != STRING,
		"Failed to set correct type 1");

	free(list[0].store.d_string.value);
	free(list[1].store.d_string.value);
	free(list[2].store.d_string.value);
	free(list);

	key.layer.value = NULL;
	fail_if(buxton_wire_get_value(&client, &key, NULL,
				      NULL) != true,
		"Failed to properly get value 2");

	r = read(server, buf, 4096);
	fail_if(r < 0, "Read from client failed 2");
	size = buxton_deserialize_message(buf, &msg, (size_t)r, &msgid, &list);
	fail_if(size != 3, "Failed to get valid message from buffer 2");
	fail_if(msg != BUXTON_CONTROL_GET,
		"Failed to get correct control type 2");
	fail_if(list[0].type != STRING, "Failed to set correct group type 2");
	fail_if(list[1].type != STRING, "Failed to set correct name type 2");
	fail_if(list[2].type != UINT32, "Failed to set correct type type 2");
	fail_if(!streq(list[0].store.d_string.value, "group"),
		"Failed to set correct group 2");
	fail_if(!streq(list[1].store.d_string.value, "name"),
		"Failed to set correct name 2");
	fail_if(list[2].store.d_uint32 != STRING,
		"Failed to set correct type 2");

	free(list[0].store.d_string.value);
	free(list[1].store.d_string.value);
	free(list);

	cleanup_callbacks();
	close(client.fd);
	close(server);
}
END_TEST

START_TEST(buxton_wire_unset_value_check)
{
	_BuxtonClient client;
	int server;
	size_t size;
	BuxtonData *list = NULL;
	uint8_t buf[4096];
	ssize_t r;
	_BuxtonKey key;
	BuxtonControlMessage msg;
	uint64_t msgid;

	setup_socket_pair(&(client.fd), &server);
	fail_if(fcntl(client.fd, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");
	fail_if(fcntl(server, F_SETFL, O_NONBLOCK),
		"Failed to set socket to non blocking");

	fail_if(!setup_callbacks(),
		"Failed to initialeze callbacks");

	key.layer = buxton_string_pack("layer");
	key.group = buxton_string_pack("group");
	key.name = buxton_string_pack("name");
	key.type = STRING;
	fail_if(!buxton_wire_unset_value(&client, &key, NULL,
					 NULL),
		"Failed to properly unset value");

	r = read(server, buf, 4096);
	fail_if(r < 0, "Read from client failed");
	size = buxton_deserialize_message(buf, &msg, (size_t)r, &msgid, &list);
	fail_if(size != 4, "Failed to get valid message from buffer");
	fail_if(msg != BUXTON_CONTROL_UNSET,
		"Failed to get correct control type");
	fail_if(list[0].type != STRING, "Failed to set correct layer type");
	fail_if(list[1].type != STRING, "Failed to set correct group type");
	fail_if(list[2].type != STRING, "Failed to set correct name type");
	fail_if(list[3].type != UINT32, "Failed to set correct type type");
	fail_if(!streq(list[0].store.d_string.value, "layer"),
		"Failed to set correct layer");
	fail_if(!streq(list[1].store.d_string.value, "group"),
		"Failed to set correct group");
	fail_if(!streq(list[2].store.d_string.value, "name"),
		"Failed to set correct group");
	fail_if(list[3].store.d_uint32 != STRING,
		"Failed to set correct type");

	free(list[0].store.d_string.value);
	free(list[1].store.d_string.value);
	free(list[2].store.d_string.value);
	free(list);

	cleanup_callbacks();
	close(client.fd);
	close(server);
}
END_TEST

static Suite *
buxton_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("buxton");

	tc = tcase_create("buxton_client_lib_functions");
	tcase_add_test(tc, buxton_direct_open_check);
	tcase_add_test(tc, buxton_direct_set_value_check);
	tcase_add_test(tc, buxton_direct_get_value_for_layer_check);
	tcase_add_test(tc, buxton_direct_get_value_check);
	tcase_add_test(tc, buxton_memory_backend_check);
	tcase_add_test(tc, buxton_key_check);
	tcase_add_test(tc, buxton_set_label_check);
	tcase_add_test(tc, buxton_group_label_check);
	tcase_add_test(tc, buxton_name_label_check);
	suite_add_tcase(s, tc);

	tc = tcase_create("buxton_protocol_functions");
	tcase_add_test(tc, run_callback_check);
	tcase_add_test(tc, handle_callback_response_check);
	tcase_add_test(tc, send_message_check);
	tcase_add_test(tc, buxton_wire_handle_response_check);
	tcase_add_test(tc, buxton_wire_get_response_check);
	tcase_add_test(tc, buxton_wire_set_value_check);
	tcase_add_test(tc, buxton_wire_set_label_check);
	tcase_add_test(tc, buxton_wire_get_value_check);
	tcase_add_test(tc, buxton_wire_unset_value_check);
	suite_add_tcase(s, tc);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	putenv("BUXTON_CONF_FILE=" ABS_TOP_BUILDDIR "/test/test.conf");
	s = buxton_suite();
	sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


/*
 * Editor modelines  -	http://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
