# Wire contract parity tests

Shared JVM tests for the JSON bridge wire format (`e`, `d`, `s` fields).

**Not a language binding** — sources are compiled into:

- `bindings/android` — `./gradlew test`
- `bindings/java` — `mvn test`

Main class: `io.jamharah.nativeview.parity.WireContractParityTest`

Keeps Android and desktop Java wire parsing aligned without requiring Android APIs in the test sources.

See [docs/bindings.md](../../docs/bindings.md).
