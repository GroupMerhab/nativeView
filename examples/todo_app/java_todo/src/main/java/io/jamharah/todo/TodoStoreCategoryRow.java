/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

public final class TodoStoreCategoryRow {
  public final long id;
  public final String name;
  public final String color;

  public TodoStoreCategoryRow(long id, String name, String color) {
    this.id = id;
    this.name = name;
    this.color = color;
  }
}
