// Port of c_todo/tests/test_todo_bridge.c / nim_todo/tests/test_todo_bridge.nim

use std::cell::RefCell;

use rust_todo::todo_bridge::bridge_handle_wire;
use rust_todo::todo_store::{store_close, store_open};

fn count_json_array_elems(arr_json: &str) -> isize {
    let v: serde_json::Value = serde_json::from_str(arr_json).expect("json");
    match v {
        serde_json::Value::Array(a) => a.len() as isize,
        _ => -1,
    }
}

#[test]
fn todo_bridge_wire() {
    let s = store_open("").expect("store");

    let last = RefCell::new((String::new(), String::new()));

    let mut capture = |ev: String, j: String| {
        let mut b = last.borrow_mut();
        b.0 = ev;
        b.1 = j;
    };

    let wire_list = r#"{"e":"todo","s":0,"d":{"action":"categories.list"}}"#;
    assert_eq!(bridge_handle_wire(&s, wire_list, &mut capture), 0);
    assert_eq!(last.borrow().0, "categories.list");
    assert_eq!(last.borrow().1, "[]");

    let wire_mk = r##"{"e":"todo","s":0,"d":{"action":"categories.create","payload":{"name":"A","color":"#112233"}}}"##;
    assert_eq!(bridge_handle_wire(&s, wire_mk, &mut capture), 0);
    assert_eq!(last.borrow().0, "categories.list");
    assert_eq!(count_json_array_elems(&last.borrow().1), 1);

    let wire_todo = r#"{"e":"todo","s":0,"d":{"action":"todos.create","payload":{"title":"T1","description":"","status":"pending","priority":"medium","categoryId":1,"parentId":0,"dueDate":0}}}"#;
    assert_eq!(bridge_handle_wire(&s, wire_todo, &mut capture), 0);
    assert_eq!(last.borrow().0, "todos.list");
    assert_eq!(count_json_array_elems(&last.borrow().1), 1);

    let tid = {
        let j = last.borrow().1.clone();
        let arr: serde_json::Value = serde_json::from_str(&j).expect("parse todos");
        arr[0]["id"].as_i64().expect("id")
    };

    let delbuf = format!(
        r#"{{"e":"todo","s":0,"d":{{"action":"todos.delete","payload":{{"id":{tid}}}}}}}"#
    );
    assert_eq!(bridge_handle_wire(&s, &delbuf, &mut capture), 0);
    assert_eq!(count_json_array_elems(&last.borrow().1), 0);

    store_close(s);
}
