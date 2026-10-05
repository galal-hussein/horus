#include "horus.h"

#include <bits/time.h>
#include <papago.h>
#include <pthread.h>
#include <rattler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tomlc17.h>
#include <unistd.h>

static horus_server_t *horus_server = NULL;

// nodes_handler handle calls to /api/nodes and send list of nodes
// as a response
static void nodes_handler([[maybe_unused]] papago_request_t *req,
                          papago_response_t *res, void *userdata) {
        horus_server_t *horus_server = (horus_server_t *)userdata;
        if (horus_server == NULL) {
                exit(EXIT_FAILURE);
        }

        char *config_json = horus_jsonify_nodes(horus_server);
        if (config_json == NULL) {
                exit(EXIT_FAILURE);
        }

        papago_res_json(res, config_json);
}

struct broadcast_args {
        papago_t *server;
        horus_server_t *horus_server;
        size_t interval;
};

static void *broadcast_nodes(void *arguments) {
        for (;;) {
                struct broadcast_args *args =
                    ((struct broadcast_args *)arguments);
                if (args == NULL) {
                        return args;
                }

                // TODO: implement this using clock_nanosleep
                sleep(args->interval);

                horus_nodes_status(args->horus_server);

                char *config_json = horus_jsonify_nodes(args->horus_server);
                if (config_json == NULL) {
                        papago_ws_broadcast(args->server,
                                            HORUS_WS_ERR_FAILED_ENCODING_JSON);
                }

                papago_ws_broadcast(args->server, config_json);

                free(config_json);
        }

        return NULL;
}

static int start_nodes_healthcheck(papago_t *server,
                                   horus_server_t *horus_server,
                                   size_t interval) {
        // uptime thread will send the status of the nodes configured
        pthread_t uptime_thread;
        struct broadcast_args *args = malloc(sizeof(struct broadcast_args));
        args->horus_server = horus_server;
        args->server = server;
        args->interval = interval;

        int err = pthread_create(&uptime_thread, NULL, broadcast_nodes, args);

        return err;
}

static void ws_on_connect(papago_ws_connection_t *conn) {}

static void ws_on_message(papago_ws_connection_t *conn, const char *message,
                          [[maybe_unused]] size_t length, bool is_binary) {
        if (is_binary) {
                return;
        }

        char *command = horus_read_command_message(message, length);
        if (command == NULL) {
                papago_ws_send(conn, HORUS_WS_ERR_INVALID_MSG_FORMAT);
                return;
        }

        for (size_t i = 0; i < horus_server->commands_size; i++) {
                if (strcmp(horus_server->commands[i].name, command) == 0) {
                        // found a command configured
                }
        }
}

static void ws_on_close(papago_ws_connection_t *conn) {}

static void ws_on_error(papago_ws_connection_t *conn, const char *error) {}

static void start_cmd_action(rattler_cmd *cmd, [[maybe_unused]] int argc,
                             [[maybe_unused]] char **argv) {
        horus_server = horus_new();
        const char *config_file = rattler_flag_string(cmd, HORUS_CONFIG_FLAG);

        // initialize a server and set nodes
        horus_parse_config(horus_server, config_file);

        papago_t *server = papago_new();

        // setup routes
        papago_ws_endpoint(server, "/ws", ws_on_connect, ws_on_message,
                           ws_on_close, ws_on_error);

        papago_route(server, PAPAGO_GET, "/api/nodes", nodes_handler,
                     horus_server);

        papago_route(server, PAPAGO_GET, "/*", papago_serve_static_handler,
                     server);

        const size_t horus_update_interval =
            rattler_flag_int(cmd, HORUS_UPDATE_INTERVAL_FLAG);

        int ret = start_nodes_healthcheck(server, horus_server,
                                          horus_update_interval);
        if (ret != 0) {
                printf("returned false %d", ret);
                // TODO: Structured logging
                papago_destroy(server);
                free(horus_server);
                return;
        }

        papago_config_t config = papago_default_config();
        config.static_dir = HORUS_WEB_DIST_DIR;

        papago_start(server, &config);

        // free server and horus server
        papago_destroy(server);
        horus_free(horus_server);
}

int main(int argc, char **argv) {
        rattler_cmd *root_cmd = rattler_new_command(
            "horus [command]", "Local LAN Portal",
            "HORUS connects all local lan nodes through one portal");
        rattler_set_version(root_cmd, HORUS_VERSION);

        rattler_cmd *start_cmd =
            rattler_new_command("start", "start HORUS portal", "");

        rattler_flags_string(start_cmd, HORUS_CONFIG_FLAG, 'c',
                             HORUS_DEFAULT_CONFIG_FILE,
                             "HORUS configuration file");
        rattler_flags_int(
            start_cmd, HORUS_UPDATE_INTERVAL_FLAG, 'u',
            HORUS_DEFAULT_UPDATE_INTERVAL,
            "Interval which the server updates the node statuses");

        start_cmd->cmd = start_cmd_action;
        rattler_add_command(root_cmd, start_cmd);

        int rc = rattler_execute(root_cmd, argc, argv);

        // free memory of root command
        rattler_free(root_cmd);

        return rc;
}
