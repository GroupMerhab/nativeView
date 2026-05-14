/*
 * todo_app — public entry for C and foreign bindings (e.g. Nim).
 *
 * argv[1] optional UTF-8 database path; if argc < 2, defaults to ./todo_app.db
 */

#ifndef TODO_APP_H
#define TODO_APP_H

#ifdef __cplusplus
extern "C" {
#endif

int todo_app_main(int argc, char** argv);

#ifdef __cplusplus
}
#endif

#endif /* TODO_APP_H */
