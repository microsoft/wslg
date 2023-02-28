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

#ifndef FREERDP_CHANNEL_RDPAPPLIST_SERVER_RDPAPPLIST_H
#define FREERDP_CHANNEL_RDPAPPLIST_SERVER_RDPAPPLIST_H

#include "rdpapplist_protocol.h"

#include <freerdp/freerdp.h>
#include <freerdp/api.h>
#include <freerdp/types.h>

typedef struct _rdpapplist_server_private RdpAppListServerPrivate;
typedef struct _rdpapplist_server_context RdpAppListServerContext;

typedef UINT (*psRdpAppListOpen)(RdpAppListServerContext* context);
typedef UINT (*psRdpAppListClose)(RdpAppListServerContext* context);

typedef UINT (*psRdpAppListCaps)(RdpAppListServerContext* context, const RDPAPPLIST_SERVER_CAPS_PDU *caps);
typedef UINT (*psRdpAppListUpdate)(RdpAppListServerContext* context, const RDPAPPLIST_UPDATE_APPLIST_PDU *updateAppList);
typedef UINT (*psRdpAppListDelete)(RdpAppListServerContext* context, const RDPAPPLIST_DELETE_APPLIST_PDU *deleteAppList);
typedef UINT (*psRdpAppListDeleteProvider)(RdpAppListServerContext* context, const RDPAPPLIST_DELETE_APPLIST_PROVIDER_PDU *deleteAppListProvider);
typedef UINT (*psRdpAppListAssociateWindowId)(RdpAppListServerContext* context, const RDPAPPLIST_ASSOCIATE_WINDOW_ID_PDU *associateWindowId);

typedef UINT (*psRdpAppListClientCaps)(RdpAppListServerContext* context, const RDPAPPLIST_CLIENT_CAPS_PDU *clientCaps);

struct _rdpapplist_server_context
{
	void* custom;
	HANDLE vcm;

	psRdpAppListOpen Open;
	psRdpAppListClose Close;

	psRdpAppListCaps ApplicationListCaps;
	psRdpAppListUpdate UpdateApplicationList;
	psRdpAppListDelete DeleteApplicationList;
	psRdpAppListDeleteProvider DeleteApplicationListProvider;
	psRdpAppListAssociateWindowId AssociateWindowId;

	psRdpAppListClientCaps ApplicationListClientCaps;

	RdpAppListServerPrivate* priv;
	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API RdpAppListServerContext* rdpapplist_server_context_new(HANDLE vcm);
	FREERDP_API void rdpapplist_server_context_free(RdpAppListServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPAPPLIST_SERVER_RDPAPPLIST_H */
