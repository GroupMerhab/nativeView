// Library version (JS side)
var NV_VERSION = "0.1.0";

// Wire protocol version (bumped only on breaking changes)
var NV_WIRE_VERSION = 1;

// Minimum compatible C library version
var NV_MIN_C_VERSION = "0.1.0";

// Default request timeout in milliseconds
var NV_TIMEOUT_DEFAULT_MS = 5000;

// Handshake-specific timeout in milliseconds
var NV_TIMEOUT_HANDSHAKE_MS = 3000;

// Wire key: event name string
var NV_KEY_EVENT = "e";

// Wire key: data payload object
var NV_KEY_DATA = "d";

// Wire key: sequence id integer
var NV_KEY_SEQ = "s";

// Wire key: success flag (replies only)
var NV_KEY_OK = "ok";

// Sequence id reserved for push events (no reply)
var NV_SEQ_PUSH = 0;

// Error code: request exceeded JS-side timeout
var NV_ERR_TIMEOUT = "ERR_TIMEOUT";

// Error code: resource or path does not exist
var NV_ERR_NOT_FOUND = "ERR_NOT_FOUND";

// Error code: operation denied by C side
var NV_ERR_PERMISSION = "ERR_PERMISSION";

// Error code: bad argument type or value
var NV_ERR_INVALID_ARG = "ERR_INVALID_ARG";

// Error code: bridge not connected yet
var NV_ERR_NOT_READY = "ERR_NOT_READY";

// Error code: operation not available on this platform
var NV_ERR_NOT_SUPPORTED = "ERR_NOT_SUPPORTED";

// Error code: wire or API version mismatch
var NV_ERR_VERSION = "ERR_VERSION_MISMATCH";

// Error code: unclassified error
var NV_ERR_UNKNOWN = "ERR_UNKNOWN";
