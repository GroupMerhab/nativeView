/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* rebuilt file */
#include <stddef.h>
#include "nv_op_registry.h"
#include "nv_window_manager.h"
#include "nv_window.h"
#include "nv_json.h"
#include "nv_arena.h"
#include "nv_op_window.h"
#include "nv_op_window_hotkey.h"
#include "nv_op_app.h"
#include "nv_op_fs.h"
#include "nv_op_dialog.h"
#include "nv_op_clipboard.h"
#include "nv_op_shell.h"
#include "nv_op_screen.h"
#include "nv_op_notification.h"
#include "nv_op_tray.h"
#include "nv_op_windows.h"
#include "nv_op_ipc_bus.h"
#include "nv_ipc_internal.h"

static const nv_ipc_op_entry_t NV_ALL_OPS[] = {
  /* window.* */
  { "window.setTitle",      nv_op_window_set_title       },
  { "window.setSize",       nv_op_window_set_size        },
  { "window.getSize",       nv_op_window_get_size        },
  { "window.setPosition",   nv_op_window_set_position    },
  { "window.getPosition",   nv_op_window_get_position    },
  { "window.setMinSize",    nv_op_window_set_min_size    },
  { "window.center",        nv_op_window_center          },
  { "window.minimize",      nv_op_window_minimize        },
  { "window.maximize",      nv_op_window_maximize        },
  { "window.restore",       nv_op_window_restore         },
  { "window.fullscreen",    nv_op_window_fullscreen      },
  { "window.isFullscreen",  nv_op_window_is_fullscreen   },
  { "window.close",         nv_op_window_close           },
  { "window.focus",         nv_op_window_focus           },
  { "window.isFocused",     nv_op_window_is_focused      },
  { "window.setResizable",  nv_op_window_set_resizable   },
  { "window.setAlwaysOnTop",nv_op_window_set_always_on_top},
  { "window.setOpacity",    nv_op_window_set_opacity     },
  { "window.setZoomFactor", nv_op_window_set_zoom_factor },
  { "window.setContextMenu", nv_op_window_set_context_menu },
  { "window.registerHotkey", nv_op_window_register_hotkey },
  { "window.unregisterHotkey", nv_op_window_unregister_hotkey },

  /* app.* */
  { "app.quit",             nv_op_app_quit               },
  { "app.getVersion",       nv_op_app_get_version        },
  { "app.getName",          nv_op_app_get_name           },
  { "app.getDataDir",       nv_op_app_get_data_dir       },
  { "app.getExePath",       nv_op_app_get_exe_path       },
  { "app.getResourceDir",   nv_op_app_get_resource_dir   },
  { "app.getPlatform",      nv_op_app_get_platform       },
  { "app.getLocale",        nv_op_app_get_locale         },
  { "app.handshake",        nv_op_app_handshake          },
  { "app.setMenu",          nv_op_app_set_menu           },

  /* fs.* */
  { "fs.readText",          nv_op_fs_read_text           },
  { "fs.writeText",         nv_op_fs_write_text          },
  { "fs.readBinary",        nv_op_fs_read_binary         },
  { "fs.writeBinary",       nv_op_fs_write_binary        },
  { "fs.exists",            nv_op_fs_exists              },
  { "fs.remove",            nv_op_fs_remove              },
  { "fs.move",              nv_op_fs_move                },
  { "fs.copy",              nv_op_fs_copy                },
  { "fs.stat",              nv_op_fs_stat                },
  { "fs.readDir",           nv_op_fs_read_dir            },
  { "fs.mkdir",             nv_op_fs_mkdir               },
  { "fs.rmdir",             nv_op_fs_rmdir               },
  { "fs.watch",             nv_op_fs_watch               },
  { "fs.unwatch",           nv_op_fs_unwatch             },

  /* dialog.* */
  { "dialog.openFile",      nv_op_dialog_open_file       },
  { "dialog.saveFile",      nv_op_dialog_save_file       },
  { "dialog.openFolder",    nv_op_dialog_open_folder     },
  { "dialog.message",       nv_op_dialog_message         },
  { "dialog.confirm",       nv_op_dialog_confirm         },

  /* clipboard.* */
  { "clipboard.readText",   nv_op_clipboard_read_text    },
  { "clipboard.writeText",  nv_op_clipboard_write_text   },
  { "clipboard.readImage",  nv_op_clipboard_read_image   },
  { "clipboard.writeImage", nv_op_clipboard_write_image  },
  { "clipboard.clear",      nv_op_clipboard_clear        },
  { "clipboard.hasText",    nv_op_clipboard_has_text     },
  { "clipboard.hasImage",   nv_op_clipboard_has_image    },

  /* shell.* */
  { "shell.openUrl",        nv_op_shell_open_url         },
  { "shell.openPath",       nv_op_shell_open_path        },
  { "shell.showInFolder",   nv_op_shell_show_in_folder   },
  { "shell.exec",           nv_op_shell_exec             },

  /* screen.* */
  { "screen.getPrimary",    nv_op_screen_get_primary     },
  { "screen.getAll",        nv_op_screen_get_all         },
  { "screen.getCursorPos",  nv_op_screen_get_cursor      },

  /* notification.* */
  { "notification.show",    nv_op_notification_show      },
  { "notification.close",   nv_op_notification_close     },
  { "notification.clicked", nv_op_notification_clicked_push },
  { "notification.closed",  nv_op_notification_closed_push },

  /* tray.* */
  { "tray.create",          nv_op_tray_create            },
  { "tray.destroy",         nv_op_tray_destroy           },
  { "tray.setIcon",         nv_op_tray_set_icon          },
  { "tray.setTooltip",      nv_op_tray_set_tooltip       },
  { "tray.setMenu",         nv_op_tray_set_menu          },
  { "tray.clicked",         nv_op_tray_clicked_push      },
  { "tray.menuAction",      nv_op_tray_menu_action_push  },

  /* windows.* */
  { "windows.open",         nv_op_windows_open           },
  { "windows.close",        nv_op_windows_close          },
  { "windows.focus",        nv_op_windows_focus          },
  { "windows.list",         nv_op_windows_list           },
  { "windows.getInfo",      nv_op_windows_get_info       },
  { "windows.setTitle",     nv_op_windows_set_title      },

  /* ipc_bus.* */
  { "ipc_bus.send",         nv_op_ipc_bus_send           },

  { NULL, NULL } /* sentinel */
};

#define NV_OPS_COUNT (sizeof(NV_ALL_OPS)/sizeof(NV_ALL_OPS[0]) - 1)
_Static_assert(NV_OPS_COUNT == 83, "op count mismatch — update this assert");

NV_INTERNAL void nv_op_registry_init(void) {
  nv_wm_init();
  size_t count = 0;
  while (NV_ALL_OPS[count].name != NULL) {
    count++;
  }
  nv_ipc_register_ops(NV_ALL_OPS, count);
}
