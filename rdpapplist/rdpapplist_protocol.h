/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPXXXX Remote Application List Virtual Channel Extension
 *
 * Copyright 2020 Microsoft
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_RDPAPPLIST_H
#define FREERDP_CHANNEL_RDPAPPLIST_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/rail.h>

#define RDPAPPLIST_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::RemoteApplicationList"

/* Version 4
 * - add RDPAPPLIST_SERVER_CAPS_PDU.appListProviderUniqueId field.
 * - add RDPAPPLIST_CMDID_ASSOCIATE_WINDOW_ID_PUD message.
 */
#define RDPAPPLIST_CHANNEL_VERSION 4

#define RDPAPPLIST_CMDID_CAPS 0x00000001
#define RDPAPPLIST_CMDID_UPDATE_APPLIST 0x00000002
#define RDPAPPLIST_CMDID_DELETE_APPLIST 0x00000003
#define RDPAPPLIST_CMDID_DELETE_APPLIST_PROVIDER 0x00000004
#define RDPAPPLIST_CMDID_ASSOCIATE_WINDOW_ID 0x00000005

#define RDPAPPLIST_ICON_FORMAT_PNG 0x0001
#define RDPAPPLIST_ICON_FORMAT_BMP 0x0002
#define RDPAPPLIST_ICON_FORMAT_SVG 0x0003

#define RDPAPPLIST_FIELD_ID         0x00000001
#define RDPAPPLIST_FIELD_GROUP      0x00000002
#define RDPAPPLIST_FIELD_EXECPATH   0x00000004
#define RDPAPPLIST_FIELD_DESC       0x00000008
#define RDPAPPLIST_FIELD_ICON       0x00000010
#define RDPAPPLIST_FIELD_PROVIDER   0x00000020
#define RDPAPPLIST_FIELD_WORKINGDIR 0x00000040
#define RDPAPPLIST_FIELD_WINDOW_ID  0x00000080

/* RDPAPPLIST_UPDATE_APPLIST_PDU */
#define RDPAPPLIST_HINT_NEWID      0x00010000 /* new appId vs update existing appId. */
#define RDPAPPLIST_HINT_SYNC       0x00100000 /* In sync mode (use with _NEWID). */
#define RDPAPPLIST_HINT_SYNC_START 0x00200000 /* Sync appId start (use with _SYNC). */
#define RDPAPPLIST_HINT_SYNC_END   0x00400000 /* Sync appId end (use with _SYNC). */
/* Client should remove any app entry those are not reported between _START and _END during sync mode */

#define RDPAPPLIST_HEADER_SIZE 8

#define RDPAPPLIST_LANG_SIZE 32

#define RDPAPPLIST_MAX_STRING_SIZE 512

struct _RDPAPPLIST_HEADER
{
	UINT32 cmdId;
	UINT32 length;
};

typedef struct _RDPAPPLIST_HEADER RDPAPPLIST_HEADER;

struct _RDPAPPLIST_SERVER_CAPS_PDU
{
	UINT16 version;
	RAIL_UNICODE_STRING appListProviderName; /* name of app list provider. */
	RAIL_UNICODE_STRING appListProviderUniqueId; /* added from version 4 */
};

typedef struct _RDPAPPLIST_SERVER_CAPS_PDU RDPAPPLIST_SERVER_CAPS_PDU;

struct _RDPAPPLIST_CLIENT_CAPS_PDU
{
	UINT16 version;
	/* ISO 639 (Language name) and ISO 3166 (Country name) connected with '_', such as en_US, ja_JP */
	char clientLanguageId[RDPAPPLIST_LANG_SIZE];
};

typedef struct _RDPAPPLIST_CLIENT_CAPS_PDU RDPAPPLIST_CLIENT_CAPS_PDU;

struct _RDPAPPLIST_ICON_DATA {
	UINT32 flags;
	UINT32 iconWidth;
	UINT32 iconHeight;
	UINT32 iconStride;
	UINT32 iconBpp;
	UINT32 iconFormat; /* RDPAPPLIST_ICON_FORMAT_* */
	UINT32 iconBitsLength; /* size of buffer pointed by iconBits. */
	VOID *iconBits; /* icon image data */
			/* For BMP, image data only */
			/* For PNG, entire PNG file including headers */
			/* For SVG, entire SVG file including headers */
		 	/* For 32bpp image, alpha-channel works as mask */
};

typedef struct _RDPAPPLIST_ICON_DATA RDPAPPLIST_ICON_DATA;

/* Create or update application program link in client */

struct _RDPAPPLIST_UPDATE_APPLIST_PDU
{
	UINT32 flags;
	RAIL_UNICODE_STRING appId; /* Identifier of application to be added
				      to client's Start Menu. This is used as
				      the file name of link (.lnk) at Start Menu. */
	RAIL_UNICODE_STRING appGroup; /* name of app group. */
	RAIL_UNICODE_STRING appExecPath; /* Path to server side executable. */
	RAIL_UNICODE_STRING appWorkingDir; /* Working directory to run the executable in. */
	RAIL_UNICODE_STRING appDesc; /* UI friendly description of application. */
	RDPAPPLIST_ICON_DATA *appIcon;
};

typedef struct _RDPAPPLIST_UPDATE_APPLIST_PDU RDPAPPLIST_UPDATE_APPLIST_PDU;

/* Delete specififed application program link from client */

struct _RDPAPPLIST_DELETE_APPLIST_PDU
{
	UINT32 flags;
	RAIL_UNICODE_STRING appId; /* Identifier of application to be removed
				      from client's Start Menu. */
	RAIL_UNICODE_STRING appGroup; /* name of app group. */
};

typedef struct _RDPAPPLIST_DELETE_APPLIST_PDU RDPAPPLIST_DELETE_APPLIST_PDU;

/* Delete all application program link under specififed provider from client */

struct _RDPAPPLIST_DELETE_APPLIST_PROVIDER_PDU
{
	UINT32 flags;
	RAIL_UNICODE_STRING appListProviderName; /* name of app list provider to be removed
						    from client's Start Menu. */
};

typedef struct _RDPAPPLIST_DELETE_APPLIST_PROVIDER_PDU RDPAPPLIST_DELETE_APPLIST_PROVIDER_PDU;

struct _RDPAPPLIST_ASSOCIATE_WINDOW_ID_PDU
{
	UINT32 flags;
	UINT32 appWindowId; /* window id of application */
	RAIL_UNICODE_STRING appId; /* Identifier of application to add taskbar property. */
	RAIL_UNICODE_STRING appGroup; /* Identifier of group where application belonging to. */
	RAIL_UNICODE_STRING appExecPath; /* Path to server side executable. */
	RAIL_UNICODE_STRING appDesc; /* UI friendly description of application. */
};

typedef struct _RDPAPPLIST_ASSOCIATE_WINDOW_ID_PDU RDPAPPLIST_ASSOCIATE_WINDOW_ID_PDU;

#endif /* FREERDP_CHANNEL_RDPAPPLIST_H */
