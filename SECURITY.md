# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | Yes (pre-1.0; security fixes on best effort) |
| < 0.1   | No |

Once nativeview reaches 1.0, this table will list supported release branches.

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

Use one of these channels:

1. **GitHub Security Advisories (preferred)**  
   Open the repository on GitHub → **Security** → **Report a vulnerability**.  
   This keeps the report private until a fix is ready.

2. **Email fallback**  
   If GitHub Advisories are unavailable, email **security@jamharah.com** with:
   - A description of the issue
   - Steps to reproduce
   - Impact assessment (if known)
   - Your contact for follow-up

Replace the email address with your team's real security contact before publication.

## Response Timeline

| Stage | Target |
|-------|--------|
| Acknowledgment | Within 7 days |
| Initial assessment | Within 14 days |
| Fix or mitigation | Within 30 days for confirmed issues (complex issues may take longer) |

We will coordinate disclosure with reporters and credit them in the advisory when desired.

## Scope

**In scope**

- Memory safety issues in nativeview C/C++/Objective-C code (UAF, buffer overflow, etc.)
- Privilege escalation via bridge ops (`fs.*`, `shell.exec`, file dialogs, IPC routing)
- Origin / sandbox bypass in WebView message delivery
- JNI / platform bridge bugs that allow unintended native access

**Out of scope**

- Vulnerabilities in third-party WebView engines (WebKit, Chromium/WebView2) — report to the vendor
- Issues in application code that embeds nativeview without following sandbox guidance
- Denial-of-service via resource exhaustion in local dev HTTP server (`nv-http`) unless exploitable remotely in a typical embedder setup

## Secure Embedding Guidelines

- Treat `shell.exec` and broad `fs.*` access as privileged; restrict in production apps.
- Validate paths and URLs before passing them to native ops.
- On Android, use `allow_origin()` for non-`file://` content.
- Keep WebView2 / WebKit / system WebView components updated on end-user machines.
