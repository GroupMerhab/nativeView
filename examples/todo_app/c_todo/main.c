/*
 * todo_app — C entry; delegates to todo_app_main (shared with Nim bindings).
 */

#include "todo_app.h"

int main(int argc, char** argv) { return todo_app_main(argc, argv); }
