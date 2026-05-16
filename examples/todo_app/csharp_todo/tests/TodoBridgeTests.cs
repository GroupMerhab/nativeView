// Port of c_todo/tests/test_todo_bridge.c / nim_todo/tests/test_todo_bridge.nim
// SPDX-License-Identifier: Apache-2.0

using System.Text.Json;
using Jamharah.Todo;
using Xunit;

namespace Jamharah.Todo.Tests;

public sealed class TodoBridgeTests
{
    [Fact]
    public void Bridge_roundtrip_categories_and_todos()
    {
        var s = TodoStoreApi.StoreOpen("");
        Assert.NotNull(s);
        try
        {
            var last = new Capture();
            Assert.Equal(0, TodoBridge.BridgeHandleWire(s,
                """{"e":"todo","s":0,"d":{"action":"categories.list"}}""", last.Emit));
            Assert.Equal("categories.list", last.Event);
            Assert.Equal("[]", last.Json);

            Assert.Equal(0, TodoBridge.BridgeHandleWire(s,
                """{"e":"todo","s":0,"d":{"action":"categories.create","payload":{"name":"A","color":"#112233"}}}""",
                last.Emit));
            Assert.Equal("categories.list", last.Event);
            Assert.Equal(1, CountJsonArrayElems(last.Json));

            Assert.Equal(0, TodoBridge.BridgeHandleWire(s,
                """{"e":"todo","s":0,"d":{"action":"todos.create","payload":{"title":"T1","description":"","status":"pending","priority":"medium","categoryId":1,"parentId":0,"dueDate":0}}}""",
                last.Emit));
            Assert.Equal("todos.list", last.Event);
            Assert.Equal(1, CountJsonArrayElems(last.Json));

            var arr = JsonSerializer.Deserialize<JsonElement>(last.Json);
            Assert.Equal(JsonValueKind.Array, arr.ValueKind);
            Assert.True(arr.GetArrayLength() >= 1);
            var tid = arr[0].GetProperty("id").GetInt64();

            var delbuf =
                "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.delete\",\"payload\":{\"id\":" + tid + "}}}";
            Assert.Equal(0, TodoBridge.BridgeHandleWire(s, delbuf, last.Emit));
            Assert.Equal(0, CountJsonArrayElems(last.Json));
        }
        finally
        {
            TodoStoreApi.StoreClose(s);
        }
    }

    private static int CountJsonArrayElems(string arrJson)
    {
        var v = JsonSerializer.Deserialize<JsonElement>(arrJson);
        return v.ValueKind == JsonValueKind.Array ? v.GetArrayLength() : -1;
    }

    private sealed class Capture
    {
        public string Event { get; private set; } = "";
        public string Json { get; private set; } = "";

        public void Emit(string ev, string json)
        {
            Event = ev;
            Json = json;
        }
    }
}
