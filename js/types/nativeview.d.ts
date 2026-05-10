// Type definitions for NativeView JS API
// Generated from js/src/public/api.js and js/src/modules/*.js

// Error codes surfaced through NVError and C payloads
type NVErrorCode =
  | 'ERR_TIMEOUT'
  | 'ERR_NOT_FOUND'
  | 'ERR_PERMISSION'
  | 'ERR_INVALID_ARG'
  | 'ERR_NOT_READY'
  | 'ERR_NOT_SUPPORTED'
  | 'ERR_VERSION_MISMATCH'
  | 'ERR_UNKNOWN'
  | 'ERR_IO';

declare class NVError extends Error {
  name: 'NVError';
  code: NVErrorCode;
  message: string;
  constructor(code: NVErrorCode, message?: string);
}

declare namespace NativeView {
  // Core meta
  const version: string;
  const wireVersion: number;

  // Core IPC surface
  function send(event: string, data?: any): void;
  function invoke<T = any>(event: string, data?: any, timeoutMs?: number): Promise<T>;
  function on<T = any>(event: string, fn: (data: T) => void): void;
  function off<T = any>(event: string, fn: (data: T) => void): void;
  function once<T = any>(event: string, fn: (data: T) => void): void;

  // Window module
  namespace window {
    function setTitle(title: string): Promise<{}>;
    function setSize(w: number, h: number): Promise<{}>;
    function getSize(): Promise<{ w: number; h: number }>;
    function setPosition(x: number, y: number): Promise<{}>;
    function getPosition(): Promise<{ x: number; y: number }>;
    function setMinSize(w: number, h: number): Promise<{}>;
    function center(): Promise<{}>;
    function minimize(): Promise<{}>;
    function maximize(): Promise<{}>;
    function restore(): Promise<{}>;
    function fullscreen(enable: boolean): Promise<{}>;
    function isFullscreen(): Promise<{ value: boolean }>;
    function close(): Promise<{}>;
    function focus(): Promise<{}>;
    function isFocused(): Promise<{ value: boolean }>;
    function setResizable(enable: boolean): Promise<{}>;
    function setAlwaysOnTop(enable: boolean): Promise<{}>;
    function setOpacity(value: number): Promise<{}>;
    function setZoomFactor(factor: number): Promise<{}>;

    // Push events (payloads may vary by platform; typed as unknown)
    function onResized(fn: (data: unknown) => void): void;
    function onMoved(fn: (data: unknown) => void): void;
    function onFocused(fn: (data: unknown) => void): void;
  }

  // App module
  namespace app {
    function quit(): void;
    function getVersion(): Promise<{ version: string }>;
    function getName(): Promise<{ name: string }>;
    function getDataDir(): Promise<{ path: string }>;
    function getExePath(): Promise<{ path: string }>;
    function getResourceDir(): Promise<{ path: string }>;
    function getPlatform(): Promise<{ platform: 'mac' | 'win' | 'linux' }>;
    function getLocale(): Promise<{ locale: string }>;
  }

  // FS module
  namespace fs {
    function readText(path: string): Promise<{ text: string }>;
    function writeText(path: string, text: string): Promise<{}>;
    function readBinary(path: string): Promise<{ bytes: string }>;
    function writeBinary(path: string, b64: string): Promise<{}>;
    function exists(path: string): Promise<{ exists: boolean }>;
    function remove(path: string): Promise<{}>;
    function move(src: string, dst: string): Promise<{}>;
    function copy(src: string, dst: string): Promise<{}>;
    function stat(path: string): Promise<{ size: number; isFile: boolean; isDir: boolean }>;
    function readDir(path: string): Promise<{ entries: any[] }>;
    function mkdir(path: string, recursive?: boolean): Promise<{}>;
    function rmdir(path: string, recursive?: boolean): Promise<{}>;
    function watch(path: string, id: number): Promise<{}>;
    function unwatch(id: number): Promise<{}>;

    // Push events
    function onChanged(fn: (data: { id: number; path: string }) => void): void;
  }

  // Dialog module
  namespace dialog {
    type FileFilter = { name: string; extensions: string[] };
    function openFile(title?: string, filters?: FileFilter[], multiple?: boolean): Promise<{ canceled: boolean; paths: string[] }>;
    function saveFile(title?: string, filters?: FileFilter[], defaultName?: string): Promise<{ path: string }>;
    function openFolder(title?: string): Promise<{ path: string }>;
    function message(title: string, body: string, type?: 'info' | 'warning' | 'error', buttons?: string[]): Promise<{}>;
    function confirm(title: string, body: string): Promise<{ confirmed: boolean }>;
  }

  // Clipboard module
  namespace clipboard {
    function readText(): Promise<{ text: string }>;
    function writeText(text: string): Promise<{}>;
    function readImage(): Promise<{ width: number; height: number; format: string; data: string }>;
    function writeImage(b64OrOpts: string | { format?: string; data: string }): Promise<{}>;
    function clear(): void;
    function hasText(): Promise<{ value: boolean }>;
    function hasImage(): Promise<{ value: boolean }>;
  }

  // Shell module
  namespace shell {
    function openUrl(url: string): void;
    function openPath(path: string): void;
    function showInFolder(path: string): void;
    function exec(cmd: string, args?: string[], cwd?: string): Promise<{ exitCode: number }>;
  }

  // Screen module
  namespace screen {
    type Display = { width: number; height: number; scale: number };
    function getPrimary(): Promise<Display>;
    function getAll(): Promise<{ displays: Display[] }>;
    function getCursorPos(): Promise<{ x: number; y: number }>;
  }

  // Notification module
  namespace notification {
    function show(id: number, title: string, body: string, icon?: string): Promise<{ id: number }>;
    function close(id: number): void;
    function onClicked(fn: (data: { id: number }) => void): void;
    function onClosed(fn: (data: { id: number }) => void): void;
  }

  // Tray module
  namespace tray {
    type MenuItem = {
      id: number;
      label?: string;
      enabled?: boolean;
      checked?: boolean;
      type?: 'normal' | 'separator' | 'checkbox' | 'submenu';
      submenu?: MenuItem[];
    };
    function create(id: number, icon: string, tooltip?: string): Promise<{ id: number }>;
    function destroy(id: number): void;
    function setIcon(id: number, icon: string): Promise<{}>;
    function setTooltip(id: number, tooltip: string): Promise<{}>;
    function setMenu(id: number, items: MenuItem[]): Promise<{}>;
    function onClicked(fn: (data: { id: number }) => void): void;
    function onMenuAction(fn: (data: { id: number; itemId: number }) => void): void;
  }

  // NVError exposed on the global API
  const NVError: {
    new (code: NVErrorCode, message?: string): NVError;
    prototype: NVError;
  };
}

declare const NativeView: {
  version: typeof NativeView.version;
  wireVersion: typeof NativeView.wireVersion;
  send: typeof NativeView.send;
  invoke: typeof NativeView.invoke;
  on: typeof NativeView.on;
  off: typeof NativeView.off;
  once: typeof NativeView.once;
  window: typeof NativeView.window;
  app: typeof NativeView.app;
  fs: typeof NativeView.fs;
  dialog: typeof NativeView.dialog;
  clipboard: typeof NativeView.clipboard;
  shell: typeof NativeView.shell;
  screen: typeof NativeView.screen;
  notification: typeof NativeView.notification;
  tray: typeof NativeView.tray;
  NVError: typeof NativeView.NVError;
};

export = NativeView;
export as namespace NativeView;
