/*
 * Copyright (C) 2011 matt mooney <mfm@muteddisk.com>
 *               2005-2007 Takahiro Hirofuchi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "usbip_common.h"
#include "usbip_network.h"

#include "usbip_windows.h"

#include "usbip.h"

static const char usbip_list_usage_string[] =
	"usbip list [-p|--parsable] <args>\n"
	"    -p, --parsable         Parsable list format\n"
	"    -r, --remote=<host>    List the exported USB devices on <host>\n"
	"    -l, --local            List the local USB devices\n"
	;

void usbip_list_usage(void)
{
	printf("usage: %s", usbip_list_usage_string);
}

static int query_exported_devices(int sockfd)
{
	int ret;
	unsigned int i;
	int j;
	struct op_devlist_reply rep;
	uint16_t code = OP_REP_DEVLIST;

	memset(&rep, 0, sizeof(rep));

	ret = usbip_send_op_common(sockfd, OP_REQ_DEVLIST, 0);
	if (ret < 0) {
		err("send op_common");
		return -1;
	}

	ret = usbip_recv_op_common(sockfd, &code);
	if (ret < 0) {
		err("recv op_common");
		return -1;
	}

	ret = usbip_recv(sockfd, (void *) &rep, sizeof(rep));
	if (ret < 0) {
		err("recv op_devlist");
		return -1;
	}

	PACK_OP_DEVLIST_REPLY(0, &rep);
	dbg("exportable %d devices", rep.ndev);

	for (i=0; i < rep.ndev; i++) {
		char product_name[100];
		char class_name[100];
		struct usbip_usb_device udev;

		memset(&udev, 0, sizeof(udev));

		ret = usbip_recv(sockfd, (void *) &udev, sizeof(udev));
		if (ret < 0) {
			err("recv usbip_usb_device[%d]", i);
			return -1;
		}
		pack_usb_device(0, &udev);

		usbip_names_get_product(product_name, sizeof(product_name),
					udev.idVendor, udev.idProduct);
		usbip_names_get_class(class_name, sizeof(class_name),
				      udev.bDeviceClass, udev.bDeviceSubClass,
				      udev.bDeviceProtocol);

		printf("%8s: %s\n", udev.busid, product_name);
		printf("%8s: %s\n", " ", udev.path);
		printf("%8s: %s\n", " ", class_name);

		for (j=0; j < udev.bNumInterfaces; j++) {
			struct usbip_usb_interface uinf;

			ret = usbip_recv(sockfd, (void *) &uinf, sizeof(uinf));
			if (ret < 0) {
				err("recv usbip_usb_interface[%d]", j);
				return -1;
			}

			pack_usb_interface(0, &uinf);
			usbip_names_get_class(class_name, sizeof(class_name),
					      uinf.bInterfaceClass,
					      uinf.bInterfaceSubClass,
					      uinf.bInterfaceProtocol);

			printf("%8s: %2d - %s\n", " ", j, class_name);
		}

		printf("\n");
	}

	return rep.ndev;
}

static int show_exported_devices(char *host)
{
	int ret;
	int sockfd;

	sockfd = usbip_net_tcp_connect(host, USBIP_PORT_STRING);
	if (sockfd < 0) {
		err("unable to connect to %s port %s: %s\n", host,
		    USBIP_PORT_STRING, gai_strerror(sockfd));
		return -1;
	}
	dbg("connected to %s port %s\n", host, USBIP_PORT_STRING);

	printf("- %s\n", host);

	ret = query_exported_devices(sockfd);
	if (ret < 0) {
		err("query");
		return -1;
	}

	closesocket(sockfd);
	return 0;
}

int usbip_list(int argc, char *argv[])
{
	static const struct option opts[] = {
		{ "parsable", no_argument, NULL, 'p' },
		{ "remote", required_argument, NULL, 'r' },
#if 0
		{ "local", no_argument, NULL, 'l' },
#endif
		{ NULL, 0, NULL, 0 }
	};
	bool parsable = false;
	int opt;
	int ret = -1;

	if (usbip_names_init(USBIDS_FILE))
		err("failed to open %s", USBIDS_FILE);

	for (;;) {
		opt = getopt_long(argc, argv, "pr:l", opts, NULL);

		if (opt == -1)
			break;

		switch (opt) {
		case 'p':
			parsable = true;
			break;
		case 'r':
			ret = show_exported_devices(optarg);
			goto out;
#if 0
		case 'l':
			ret = list_devices(parsable);
			goto out;
#endif
		default:
			goto err_out;
		}
	}

err_out:
	usbip_list_usage();
out:
	usbip_names_free();

	return ret;
}
