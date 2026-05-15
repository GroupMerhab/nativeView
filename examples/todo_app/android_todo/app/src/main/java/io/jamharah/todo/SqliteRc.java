/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

/** SQLite result codes used by {@link AndroidTodoStore} (subset of {@code sqlite3.h}). */
public final class SqliteRc {
  public static final int SQLITE_OK = 0;
  public static final int SQLITE_ERROR = 1;
  public static final int SQLITE_CONSTRAINT = 19;
  public static final int SQLITE_MISUSE = 21;
  public static final int SQLITE_RANGE = 25;

  private SqliteRc() {}
}
