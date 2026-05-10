// NVError constructor and factories for nativeview.js
// Depends on constants.js being loaded before this file

function NVError(code, message) {
  this.name = "NVError";                        // error class name
  this.code = code || NV_ERR_UNKNOWN;           // stable error code string
  this.message = message || "unknown error";    // human-readable message
}
NVError.prototype = Object.create(Error.prototype);
NVError.prototype.constructor = NVError;

// Build NVError from a C error payload { code, msg }
function nv_error_from_payload(payload) {
  var c = payload && payload.code ? payload.code : NV_ERR_UNKNOWN;
  var m = payload && payload.msg  ? payload.msg  : "unknown error";
  return new NVError(c, m);
}

// Factories for common error cases
function nv_err_timeout(event)     { return new NVError(NV_ERR_TIMEOUT,       "timeout: " + String(event)); }
function nv_err_not_ready()        { return new NVError(NV_ERR_NOT_READY,     "bridge not ready"); }
function nv_err_version(got, want) { return new NVError(NV_ERR_VERSION,       "wire version mismatch: got " + String(got) + " want " + String(want)); }
function nv_err_invalid_arg(msg)   { return new NVError(NV_ERR_INVALID_ARG,   msg); }
