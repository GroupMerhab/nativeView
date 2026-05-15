/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

public final class TodoStoreTodoRow {
  public final long id;
  public final String title;
  public final String description;
  public final String status;
  public final String priority;
  public final boolean categoryIdIsNull;
  public final long categoryId;
  public final boolean parentIdIsNull;
  public final long parentId;
  public final boolean dueDateIsNull;
  public final long dueDate;
  public final int position;
  public final long createdAt;
  public final long updatedAt;

  public TodoStoreTodoRow(
      long id,
      String title,
      String description,
      String status,
      String priority,
      boolean categoryIdIsNull,
      long categoryId,
      boolean parentIdIsNull,
      long parentId,
      boolean dueDateIsNull,
      long dueDate,
      int position,
      long createdAt,
      long updatedAt) {
    this.id = id;
    this.title = title;
    this.description = description;
    this.status = status;
    this.priority = priority;
    this.categoryIdIsNull = categoryIdIsNull;
    this.categoryId = categoryId;
    this.parentIdIsNull = parentIdIsNull;
    this.parentId = parentId;
    this.dueDateIsNull = dueDateIsNull;
    this.dueDate = dueDate;
    this.position = position;
    this.createdAt = createdAt;
    this.updatedAt = updatedAt;
  }
}
