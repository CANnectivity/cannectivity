/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/shell/shell.h>

static int cmd_gs_usb_vid(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "USB VID: 0x%04x", CONFIG_TEST_USB_VID);

	return 0;
}

static int cmd_gs_usb_pid(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "USB PID: 0x%04x", CONFIG_TEST_USB_PID);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gs_usb_cmds,
	SHELL_CMD(vid, NULL,
		"Get USB VID\n"
		"Usage: gs_usb vid",
		cmd_gs_usb_vid),
	SHELL_CMD(pid, NULL,
		"Get USB PID\n"
		"Usage: gs_usb pid",
		cmd_gs_usb_pid),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(gs_usb, &sub_gs_usb_cmds, "gs_usb test commands", NULL);
