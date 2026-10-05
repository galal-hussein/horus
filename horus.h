#ifndef __HORUS_H
#define __HORUS_H

#include <papago.h>
#include <stdlib.h>

#define HORUS_WEB_DIST_DIR "web/dist"
#define HORUS_VERSION "v1.0.0"
#define HORUS_CONFIG_FLAG "config"
#define HORUS_UPDATE_INTERVAL_FLAG "update-interval"
#define HORUS_DEFAULT_CONFIG_FILE "horus.toml"
#define HORUS_DEFAULT_UPDATE_INTERVAL 5

#define HORUS_WS_ERR_INVALID_MSG_FORMAT                                        \
        "{\"type\":\"error\","                                                 \
        "\"message\":\"Invalid message format\"}"
#define HORUS_WS_ERR_MISSING_COMMAND_NAME                                      \
        "{\"type\":\"error\","                                                 \
        "\"message\":\"Missing command name\"}"
#define HORUS_WS_ERR_FAILED_ENCODING_JSON                                      \
        "{\"type\":\"error\","                                                 \
        "\"message\":\"Failed to encode Horus nodes to JSON\"}"

typedef struct horus_node_t horus_node_t;
typedef struct horus_command_t horus_command_t;
typedef struct horus_server_t horus_server_t;

struct horus_node_t {
        char *name;
        char *ip;
        char *url;
        bool live;
};

struct horus_command_t {
        char *name;
        char *exec;
        bool sudo;
};

struct horus_server_t {
        horus_node_t *nodes;
        horus_command_t *commands;
        size_t nodes_size;
        size_t commands_size;
};

horus_server_t *horus_new();

void horus_parse_config(horus_server_t *server, const char *config_path);

char *horus_jsonify_nodes(horus_server_t *server);

void horus_free(horus_server_t *server);

void horus_nodes_status(horus_server_t *server);

char *horus_read_command_message(const char *message, size_t length);

#endif /** end __HORUS_H */
