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

/**
 * \file cli/main.c Buxton command line interface
 *
 * Provides a CLI to Buxton through which a variety of operations can be
 * carried out
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "bt-daemon.h"
#include "hashmap.h"
#include "util.h"

static Hashmap *commands;
static BuxtonClient client;
static unsigned int arg_n = 1;
static char **arg_v;

/**
 * Store a command reference in Buxton CLI
 * @param type Type of data to operate on
 * @param params number of parameters
 * @return a boolean value, indicating success of the operation
 */
typedef bool (*command_method) (BuxtonDataType type, int params);

/**
 * Defines a command within the buxtonctl cli
 */
typedef struct Command {
	const char     *name; /**<name of the command*/
	const char     *description; /**<one line description of the command*/
	unsigned int   min_arguments; /**minimum number of arguments */
	unsigned int   max_arguments; /**<maximum number of arguments */
	const char     *usage; /**<correct usage of the command */
	command_method method; /**<pointer to a method */
	BuxtonDataType type; /**<type of data to operate on */
} Command;

/* Print help message */
static bool print_help(void)
{
	const char *key;
	Iterator iterator;
	Command *command;

	printf("buxtonctl: Usage\n\n");

	HASHMAP_FOREACH_KEY(command, key, commands, iterator) {
		printf("\t%10s - %s\n", key, command->description);
	};

	return true;
}

static void print_usage(Command *command)
{
	if (command->min_arguments == command->max_arguments)
		printf("%s takes %d arguments - %s\n", command->name, command->min_arguments, command->usage);
	else
		printf("%s takes at least %d arguments - %s\n", command->name, command->min_arguments, command->usage);
}

/* Set a value in Buxton */
static bool set_value(BuxtonDataType type, int params) {
	char *layer, *key, *value;
	BuxtonData set;

	layer = arg_v[arg_n + 1];
	key = arg_v[arg_n + 2];
	value = arg_v[arg_n + 3];
	bool ret = false;

	set.type = type;
	switch (set.type) {
		case STRING:
			set.store.d_string = value;
			break;
		case BOOLEAN:
			if (streq(value, "true"))
				set.store.d_boolean = true;
			else if (streq(value, "false"))
				set.store.d_boolean = false;
			else {
				printf("Accepted values are [true] [false]. Not updating\n");
				return false;
			}
			break;
		case FLOAT:
			set.store.d_float = strtof(value, NULL);
			if (errno) {
				printf("Invalid floating point value\n");
				return false;
			}
			break;
		case DOUBLE:
			set.store.d_double = strtod(value, NULL);
			if (errno) {
				printf("Invalid double precision value\n");
				return false;
			}
			break;
		case LONG:
			set.store.d_long = strtol(value, NULL, 10);
			if (errno) {
				printf("Invalid long integer value\n");
				return false;
			}
			break;
		case INT:
			set.store.d_int = strtol(value, NULL, 10);
			if (errno) {
				printf("Invalid integer\n");
				return false;
			}
			break;
		default:
			break;
	}
	ret = buxton_client_set_value(&client, layer, key, &set);
	if (!ret)
		printf("Failed to update key \'%s\' in layer '%s'\n", key, layer);
	return ret;
}


/* Get a value from Buxton */
static bool get_value(BuxtonDataType type, int params) {
	char *layer, *key;
	BuxtonData get;
	bool ret = true;
	char *prefix;

	if (params == 2) {
		layer = arg_v[arg_n + 1];
		key = arg_v[arg_n+2];
		asprintf(&prefix, "[%s] ", layer);
	} else {
		key = arg_v[arg_n + 1];
		asprintf(&prefix, " ");
	}

	if (params == 2) {
		if (!buxton_client_get_value_for_layer(&client, layer, key, &get)) {
			printf("Requested key was not found in layer \'%s\': %s\n", layer, key);
			ret = false;
			goto end;
		}
	} else {
		if (!buxton_client_get_value(&client, key, &get)) {
			printf("Requested key was not found: %s\n", key);
			ret = false;
			goto end;
		}
	}

	if (get.type != type) {
		const char *type_req, *type_got;
		type_req = buxton_type_as_string(type);
		type_got = buxton_type_as_string(get.type);
		printf("You requested a key with type \'%s\', but value is of type \'%s\'.\n\n", type_req, type_got);
		print_help();
		ret = false;
		goto end;
	}

	switch (get.type) {
		case STRING:
			printf("%s%s = %s\n", prefix, key, get.store.d_string);
			break;
		case BOOLEAN:
			if (get.store.d_boolean == true)
				printf("%s%s = true\n", prefix, key);
			else
				printf("%s%s = false\n", prefix, key);
			break;
		case FLOAT:
			printf("%s%s = %f\n", prefix, key, get.store.d_float);
			break;
		case DOUBLE:
			printf("%s%s = %f\n", prefix, key, get.store.d_double);
			break;
		case LONG:
			printf("%s%s = %ld\n", prefix, key, get.store.d_long);
			break;
		case INT:
			printf("%s%s = %d\n", prefix, key, get.store.d_int);
			break;
		default:
			printf("unknown type\n");
			ret = false;
			break;
	}
end:
	free(prefix);
	if (ret && get.type == STRING)
		free(get.store.d_string);

	return ret;
}

/**
 * Entry point into buxtonctl
 * @param argc Number of arguments passed
 * @param argv An array of string arguments
 * @returns EXIT_SUCCESS if the operation succeeded, otherwise EXIT_FAILURE
 */
int main(int argc, char **argv)
{
	bool ret = false;
	Command c_get_string, c_set_string;
	Command c_get_bool, c_set_bool;
	Command c_get_float, c_set_float;
	Command c_get_double, c_set_double;
	Command c_get_int, c_set_int;
	Command c_get_long, c_set_long;
	Command c_help;
	Command *command;
	int params;
	arg_v = argv;

	/* Build a command list */
	commands = hashmap_new(string_hash_func, string_compare_func);

	/* Strings */
	c_get_string = (Command) { "get-string", "Get a string value by key",
				   1, 2, "[layer] [key]", &get_value, STRING };
	hashmap_put(commands, c_get_string.name, &c_get_string);

	c_set_string = (Command) { "set-string", "Set a key with a string value",
				   3, 3, "[layer] [key] [value]", &set_value, STRING };
	hashmap_put(commands, c_set_string.name, &c_set_string);

	/* Booleans */
	c_get_bool = (Command) { "get-bool", "Get a boolean value by key",
				   1, 2, "[layer] [key]", &get_value, BOOLEAN };
	hashmap_put(commands, c_get_bool.name, &c_get_bool);

	c_set_bool = (Command) { "set-bool", "Set a key with a boolean value",
				   3, 3, "[layer] [key] [value]", &set_value, BOOLEAN };
	hashmap_put(commands, c_set_bool.name, &c_set_bool);

	/* Floats */
	c_get_float = (Command) { "get-float", "Get a float point value by key",
				   1, 2, "[layer] [key]", &get_value, FLOAT };
	hashmap_put(commands, c_get_float.name, &c_get_float);

	c_set_float = (Command) { "set-float", "Set a key with a floating point value",
				   3, 3, "[layer] [key] [value]", &set_value, FLOAT };
	hashmap_put(commands, c_set_float.name, &c_set_float);

	/* Doubles */
	c_get_double = (Command) { "get-double", "Get a double precision value by key",
				   1, 2, "[layer] [key]", &get_value, DOUBLE };
	hashmap_put(commands, c_get_double.name, &c_get_double);

	c_set_double = (Command) { "set-double", "Set a key with a double precision value",
				   3 , 3, "[layer] [key] [value]", &set_value, DOUBLE };
	hashmap_put(commands, c_set_double.name, &c_set_double);

	/* Longs */
	c_get_long = (Command) { "get-long", "Get a long integer value by key",
				   1, 2, "[layer] [key]", &get_value, LONG };
	hashmap_put(commands, c_get_long.name, &c_get_long);

	c_set_long = (Command) { "set-long", "Set a key with a long integer value",
				   3, 3, "[layer] [key] [value]", &set_value, LONG };
	hashmap_put(commands, c_set_long.name, &c_set_long);

	/* Integers */
	c_set_int = (Command) { "set-int", "Set a key with an integer value",
				3, 3, "[layer] [key] [value]", &set_value, INT };
	hashmap_put(commands, c_set_int.name, &c_set_int);

	c_get_int = (Command) { "get-int", "Get an integer value by key",
				1, 2, "[layer] [key]", &get_value, INT };
	hashmap_put(commands, c_get_int.name, &c_get_int);

	/* Help */
	c_help = (Command) { "help", "Print this help message",
			     0, 0, NULL, NULL, 0 };
	hashmap_put(commands, c_help.name, &c_help);

	if (argc > 1 && strncmp(argv[1], "--direct", 8) == 0) {
		if (geteuid() != 0) {
			printf("Only root may use --direct\n");
			goto end;
		}
		client.direct = true;
		arg_n++;
	}

	if (argc < 3) {
		/* Todo: print usage if a valid command name was given */
		print_help();
		goto end;
	}

	params = argc-arg_n-1;

	if ((command = hashmap_get(commands, argv[arg_n])) == NULL) {
		printf("Unknown command: %s\n", argv[arg_n]);
		goto end;
	}

	/* We now execute the command */
	if (command->method == NULL) {
		printf("Not yet implemented: %s\n", command->name);
		goto end;
	}

	if (strncmp(command->name, "help", 4) == 0) {
		/* Ensure we cleanup and abort when using help */
		command->method(0,params);
		ret = true;
		goto end;
	}

	if (argc < arg_n + command->min_arguments + 1 ||
	    argc > arg_n + command->max_arguments + 1) {
		print_usage(command);
		print_help();
		ret = false;
		goto end;
	}

	if (client.direct) {
		if (!buxton_direct_open(&client)){
			buxton_log("Failed to directly talk to Buxton\n");
			ret = false;
			goto end;
		}
	} else {
		if (!buxton_client_open(&client)) {
			buxton_log("Failed to talk to Buxton\n");
			ret = false;
			goto end;
		}
	}

	/* Connected to buxton_client, execute method */
	ret = command->method(command->type, params);

end:
	hashmap_free(commands);
	buxton_client_close(&client);
	if (ret)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

/*
 * Editor modelines  -  http://www.wireshark.org/tools/modelines.html
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
