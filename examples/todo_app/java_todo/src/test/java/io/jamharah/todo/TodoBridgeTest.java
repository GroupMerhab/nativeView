/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import org.junit.jupiter.api.Test;

class TodoBridgeTest {

  private static int countJsonArrayElems(String arrJson) {
    JsonElement v = JsonParser.parseString(arrJson);
    if (v == null || !v.isJsonArray()) {
      return -1;
    }
    JsonArray a = v.getAsJsonArray();
    return a.size();
  }

  @Test
  void todoBridgeWire() {
    TodoStore s = TodoStore.storeOpen("");
    assertTrue(s != null, "store");

    String[] last = new String[2];

    java.util.function.BiConsumer<String, String> capture =
        (ev, j) -> {
          last[0] = ev;
          last[1] = j;
        };

    String wireList = "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"categories.list\"}}";
    assertEquals(0, TodoBridge.bridgeHandleWire(s, wireList, capture));
    assertEquals("categories.list", last[0]);
    assertEquals("[]", last[1]);

    String wireMk =
        "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"categories.create\",\"payload\":{\"name\":\"A\",\"color\":\"#112233\"}}}";
    assertEquals(0, TodoBridge.bridgeHandleWire(s, wireMk, capture));
    assertEquals("categories.list", last[0]);
    assertEquals(1, countJsonArrayElems(last[1]));

    String wireTodo =
        "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.create\",\"payload\":{\"title\":\"T1\",\"description\":\"\",\"status\":\"pending\",\"priority\":\"medium\",\"categoryId\":1,\"parentId\":0,\"dueDate\":0}}}";
    assertEquals(0, TodoBridge.bridgeHandleWire(s, wireTodo, capture));
    assertEquals("todos.list", last[0]);
    assertEquals(1, countJsonArrayElems(last[1]));

    long tid =
        JsonParser.parseString(last[1]).getAsJsonArray().get(0).getAsJsonObject().get("id").getAsLong();

    String delbuf =
        "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.delete\",\"payload\":{\"id\":" + tid + "}}}";
    assertEquals(0, TodoBridge.bridgeHandleWire(s, delbuf, capture));
    assertEquals(0, countJsonArrayElems(last[1]));

    TodoStore.storeClose(s);
  }
}
