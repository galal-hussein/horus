#include "horus.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/typecheck-gcc.h>
#include <jansson.h>
#include <papago.h>
#include <rattler.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tomlc17.h>

static void error(const char *msg) {
        fprintf(stderr, "ERROR: %s\n", msg);
        fflush(stderr);
        exit(1);
}

horus_server_t *horus_new() {
        horus_server_t *horus_server = malloc(sizeof(horus_server_t));
        return horus_server;
}

void horus_free(horus_server_t *server) {
        if (!server) {
                return;
        }
        free(server->nodes);
        free(server);
}

static void parse_nodes(horus_server_t *server, toml_result_t config_data) {
        horus_node_t *horus_nodes;

        toml_datum_t nodes = toml_seek(config_data.toptab, "nodes");
        if (nodes.type != TOML_ARRAY) {
                error("missing or invalid 'nodes' property in the config "
                      "file");
        }

        horus_nodes = malloc(sizeof(horus_node_t) * nodes.u.arr.size);

        for (int i = 0; i < nodes.u.arr.size; i++) {
                horus_node_t horus_dev;
                toml_datum_t node = nodes.u.arr.elem[i];

                // for each node there must be a name and ip, url is optional.
                toml_datum_t node_name = toml_seek(node, "name");
                if (node_name.type != TOML_STRING) {
                        error("missing or invalid 'name' for node");
                }
                horus_dev.name = malloc(strlen(node_name.u.s));
                strcpy(horus_dev.name, node_name.u.s);

                toml_datum_t node_ip = toml_seek(node, "ip");
                if (node_ip.type != TOML_STRING) {
                        error("missing or invalid 'ip' for node");
                }
                horus_dev.ip = malloc(strlen(node_ip.u.s));
                strcpy(horus_dev.ip, node_ip.u.s);

                horus_dev.url = NULL;
                toml_datum_t node_url = toml_seek(node, "url");
                if (node_url.type != TOML_UNKNOWN) {
                        if (node_url.type != TOML_STRING) {
                                error("invalid 'url' for node");
                        }
                        horus_dev.url = strdup(node_url.u.s);
                }

                horus_nodes[i] = horus_dev;
        }

        // set nodes and size
        server->nodes = horus_nodes;
        server->nodes_size = nodes.u.arr.size;
}

static void parse_commands(horus_server_t *server, toml_result_t config_data) {
        // parse commands
        toml_datum_t commands = toml_seek(config_data.toptab, "commands");
        if (commands.type != TOML_ARRAY && commands.type != TOML_UNKNOWN) {
                error("invalid 'commands' property in the config file");
        }

        horus_command_t *horus_commands;
        horus_commands = malloc(sizeof(horus_command_t) * commands.u.arr.size);

        for (int i = 0; i < commands.u.arr.size; i++) {
                horus_command_t horus_command;
                toml_datum_t command = commands.u.arr.elem[i];

                // for each command there must be a name and exec sudo is optional
                toml_datum_t command_name = toml_seek(command, "name");
                if (command_name.type != TOML_STRING) {
                        error("missing or invalid 'name' for command");
                }
                horus_command.name = malloc(strlen(command_name.u.s));
                strcpy(horus_command.name, command_name.u.s);

                toml_datum_t command_exec = toml_seek(command, "exec");
                if (command_exec.type != TOML_STRING) {
                        error("missing or invalid 'exec' for command");
                }
                horus_command.exec = malloc(strlen(command_exec.u.s));
                strcpy(horus_command.exec, command_exec.u.s);

                toml_datum_t command_sudo = toml_seek(command, "sudo");
                if (command_sudo.type != TOML_BOOLEAN) {
                        error("missing or invalid 'sudo' for command");
                }
                horus_command.sudo = command_exec.u.boolean;

                horus_commands[i] = horus_command;
        }

        // set nodes and size
        server->commands = horus_commands;
        server->commands_size = commands.u.arr.size;
}

void horus_parse_config(horus_server_t *server, const char *config_path) {
        if (server == NULL) {
                error("null server detected");
        }

        toml_result_t config_data = toml_parse_file_ex(config_path);
        if (!config_data.ok) {
                error(config_data.errmsg);
        }

        parse_nodes(server, config_data);

        parse_commands(server, config_data);

        toml_free(config_data);
}

char *horus_jsonify_nodes(horus_server_t *server) {
        // TODO: Add guard rails for the empty nodes array
        json_t *nodes_array = json_array();
        for (size_t i = 0; i < server->nodes_size; i++) {
                json_t *node = json_pack(
                    "{s:s, s:s, s:s*, s:b}", "name", server->nodes[i].name,
                    "ip", server->nodes[i].ip, "url", server->nodes[i].url,
                    "live", server->nodes[i].live);
                if (node == NULL ||
                    json_array_append_new(nodes_array, node) != 0) {
                        json_decref(node);
                        error("failed to add node to JSON nodes");
                }
        }
        json_t *root = json_pack("{s:o}", "nodes", nodes_array);
        char *nodes_json = json_dumps(root, 0);

        json_decref(root);
        return nodes_json;
}

static size_t noop_cb([[maybe_unused]] char *ptr, [[maybe_unused]] size_t size,
                      size_t nmemb, [[maybe_unused]] void *userdata) {
        return nmemb;
}

void horus_nodes_status(horus_server_t *server) {
        CURL *curl;
        for (size_t i = 0; i < server->nodes_size; i++) {
                curl = curl_easy_init();
                if (curl == NULL) {
                        return;
                }
                horus_node_t node = server->nodes[i];
                if (!node.url) {
                        char url[256];
                        snprintf(url, sizeof(url), "http://%s", node.ip);
                        curl_easy_setopt(curl, CURLOPT_URL, url);
                } else {
                        curl_easy_setopt(curl, CURLOPT_URL, node.url);
                }

                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, CURLFOLLOW_ALL);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, noop_cb);

                CURLcode res = curl_easy_perform(curl);
                if (res == CURLE_OK) {
                        server->nodes[i].live = true;
                }
                curl_easy_cleanup(curl);
        }
}

char *horus_read_command_message(const char *message, size_t length) {
        // TODO: Handle error messages
        char const *type_start = strstr(message, "\"type\":");

        if (type_start == NULL) {
                return NULL;
        }

        if (strstr(type_start, "\"command\"") == NULL) {
                return NULL;
        }

        // check if its an command message
        // search for command
        char const *command_start = strstr(message, "\"name\":");
        if (command_start == NULL) {
                return NULL;
        }
        command_start = strchr(command_start, ':') + 1;
        if (command_start == NULL) {
                return NULL;
        }

        while (*command_start == ' ' || *command_start == '"') {
                command_start++;
                if (command_start - message > length) {
                        return NULL;
                }
        }
        char const *command_end = strchr(command_start, '"');
        if (command_end == NULL) {
                return NULL;
        }

        char *command;
        size_t len = command_end - command_start;
        command = malloc(len);
        memcpy(command, command_start, len);
        command[len] = '\0';

        return command;
}
