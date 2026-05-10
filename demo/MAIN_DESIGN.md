NativeView Mega Demo — main.c Design

Responsibilities
- Create nv_app.
- Configure window: 1200x800, title “NativeView Mega Demo”, resizable=1, devtools=1.
- Resolve demo/index.html on disk and load via nv_load_url using a file:// URL.
- Register close handler that calls nv_app_quit.
- Show the window and run the app event loop.

Startup Flow
- nv_app_create → nv_window_create with the config above.
- Register callbacks:
  - on_ready: optional log.
  - on_message: bridge dispatcher if present.
  - on_close: nv_app_quit(app).
- Resolve absolute path to demo/index.html (see Platform Path Resolution).
- URL-encode path minimally (space/#/%), build file://<abs> URL.
- nv_load_url(window, url); nv_window_show(window); nv_app_run(app).
- On exit, destroy app.

Constraints
- The demo must load from a local file path; no embedded HTML or inline content.
- The URL builder must tolerate spaces and common special characters.

Platform Path Resolution
- Inputs: argv[0] as a fallback where applicable.
- Output: absolute path to demo/index.html.

macOS
- Preferred: NSBundle mainBundle resourcePath → append “demo/index.html”.
- Fallback: dirname of argv[0]:
  - realpath(argv[0]) → strip filename → join “../demo/index.html” when running from build tree,
    or locate a sibling “demo/” next to the executable when bundled.

Windows
- GetModuleFileName(NULL, buf, …) → PathRemoveFileSpec(buf).
- Append “\\demo\\index.html” and normalize to absolute.
- Convert to URL by replacing backslashes with forward slashes before prefixing file://.

Linux
- Preferred: readlink("/proc/self/exe", buf, …) → dirname.
- Fallback: if readlink fails, use argv[0] then realpath.
- Append “/demo/index.html” and normalize.

Error Handling
- If index.html cannot be located:
  - Log a clear error to stderr.
  - Still show the window and run the loop so failures are visible, or exit with non‑zero if required.

Security/Robustness Notes
- Do not execute arbitrary input; only load local demo/index.html.
- Avoid environment‑dependent paths; always normalize with realpath when available.

Success Criteria
- App starts with a single window, loads demo/index.html over file://, and remains responsive until closed.
