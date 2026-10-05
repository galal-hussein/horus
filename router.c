//#include "horus.h"
//
//#include <papago.h>
//#include <rattler.h>
//#include <stdlib.h>
//#include <tomlc17.h>
//
//// nodes_handler handle calls to /api/nodes and return back the data
//static void nodes_handler([[maybe_unused]] papago_request_t *req,
//                          papago_response_t *res, void *userdata) {
//
//        horus_server_t *horus_server = (horus_server_t *)userdata;
//        if (horus_server == NULL) {
//                exit(EXIT_FAILURE);
//        }
//
//        char *config_json = horus_jsonify_nodes(horus_server);
//        if (config_json == NULL) {
//                exit(EXIT_FAILURE);
//        }
//
//        papago_res_json(res, config_json);
//}
//
//static void setup_routes(horus_server_t* horus_server) {
//
//
//}
