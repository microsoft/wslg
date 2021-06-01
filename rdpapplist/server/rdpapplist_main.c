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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rdpapplist_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/log.h>

#include "rdpapplist_server.h"
#include "rdpapplist_common.h"

#define TAG CHANNELS_TAG("rdpapplist.server")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpapplist_recv_caps_pdu(wStream* s, RdpAppListServerContext* context)
{
	UINT32 error = CHANNEL_RC_OK;
	RDPAPPLIST_CLIENT_CAPS_PDU pdu;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_ERR(TAG, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.version); /* version (2 bytes) */
	if (Stream_GetRemainingLength(s) < RDPAPPLIST_LANG_SIZE)
	{
		WLog_ERR(TAG, "not enough data!");
		return ERROR_INVALID_DATA;
	}
	Stream_Read(s, &pdu.clientLanguageId[0], RDPAPPLIST_LANG_SIZE);

	if (context)
		IFCALLRET(context->ApplicationListClientCaps, error, context, &pdu);

	return error;
}

static UINT rdpapplist_server_receive_pdu(RdpAppListServerContext* context, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	size_t beg, end;
	RDPAPPLIST_HEADER header;
	beg = Stream_GetPosition(s);

	if ((error = rdpapplist_read_header(s, &header)))
	{
		WLog_ERR(TAG, "rdpapplist_read_header failed with error %" PRIu32 "!", error);
		return error;
	}

	switch (header.cmdId)
	{
		case RDPAPPLIST_CMDID_CAPS:
			if ((error = rdpapplist_recv_caps_pdu(s, context)))
				WLog_ERR(TAG,
				         "rdpapplist_recv_caps_pdu "
				         "failed with error %" PRIu32 "!",
				         error);

			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			WLog_WARN(TAG, "Received unknown PDU type: %" PRIu32 "", header.cmdId);
			break;
	}

	end = Stream_GetPosition(s);

	if (end != (beg + header.length))
	{
		WLog_ERR(TAG, "Unexpected RDPAPPLIST pdu end: Actual: %d, Expected: %" PRIu32 "", end,
		         (beg + header.length));
		Stream_SetPosition(s, (beg + header.length));
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpapplist_server_handle_messages(RdpAppListServerContext* context)
{
	DWORD BytesReturned;
	void* buffer;
	UINT ret = CHANNEL_RC_OK;
	RdpAppListServerPrivate* priv = context->priv;
	wStream* s = priv->input_stream;

	/* Check whether the dynamic channel is ready */
	if (!priv->isReady)
	{
		if (WTSVirtualChannelQuery(priv->rdpapplist_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			if (GetLastError() == ERROR_NO_DATA)
				return ERROR_NO_DATA;

			WLog_ERR(TAG, "WTSVirtualChannelQuery failed");
			return ERROR_INTERNAL_ERROR;
		}

		priv->isReady = *((BOOL*)buffer);
		WTSFreeMemory(buffer);
	}

	/* Consume channel event only after the dynamic channel is ready */
	if (priv->isReady)
	{
		Stream_SetPosition(s, 0);

		if (!WTSVirtualChannelRead(priv->rdpapplist_channel, 0, NULL, 0, &BytesReturned))
		{
			if (GetLastError() == ERROR_NO_DATA)
				return ERROR_NO_DATA;

			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		if (BytesReturned < 1)
			return CHANNEL_RC_OK;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		if (WTSVirtualChannelRead(priv->rdpapplist_channel, 0, (PCHAR)Stream_Buffer(s), Stream_Capacity(s),
		                          &BytesReturned) == FALSE)
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		Stream_SetLength(s, BytesReturned);
		Stream_SetPosition(s, 0);

		while (Stream_GetPosition(s) < Stream_Length(s))
		{
			if ((ret = rdpapplist_server_receive_pdu(context, s)))
			{
				WLog_ERR(TAG,
				         "rdpapplist_server_receive_pdu "
				         "failed with error %" PRIu32 "!",
				         ret);
				return ret;
			}
		}
	}

	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static DWORD WINAPI rdpapplist_server_thread_func(LPVOID arg)
{
	RdpAppListServerContext* context = (RdpAppListServerContext*)arg;
	RdpAppListServerPrivate* priv = context->priv;
	DWORD status;
	DWORD nCount;
	HANDLE events[8];
	UINT error = CHANNEL_RC_OK;
	nCount = 0;
	events[nCount++] = priv->stopEvent;
	events[nCount++] = priv->channelEvent;

	while (TRUE)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		/* Stop Event */
		if (status == WAIT_OBJECT_0)
			break;

		if ((error = rdpapplist_server_handle_messages(context)))
		{
			WLog_ERR(TAG, "rdpapplist_server_handle_messages failed with error %" PRIu32 "", error);
			break;
		}
	}

	ExitThread(error);
	return error;
}

/**
 * Function description
 * Create new stream for single rdpapplist packet. The new stream length
 * would be required data length + header. The header will be written
 * to the stream before return.
 *
 * @param cmdId
 * @param length - data length without header
 *
 * @return new stream
 */
static wStream* rdpapplist_server_single_packet_new(UINT32 cmdId, UINT32 length)
{
	UINT error;
	RDPAPPLIST_HEADER header;
	wStream* s = Stream_New(NULL, RDPAPPLIST_HEADER_SIZE + length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto error;
	}

	header.cmdId = cmdId;
	header.length = RDPAPPLIST_HEADER_SIZE + length;

	if ((error = rdpapplist_write_header(s, &header)))
	{
		WLog_ERR(TAG, "Failed to write header with error %" PRIu32 "!", error);
		goto error;
	}

	return s;
error:
	Stream_Free(s, TRUE);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpapplist_server_packet_send(RdpAppListServerContext* context, wStream* s)
{
	UINT ret;
	ULONG written;

	if (!WTSVirtualChannelWrite(context->priv->rdpapplist_channel, (PCHAR)Stream_Buffer(s),
	                            Stream_GetPosition(s), &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		ret = ERROR_INTERNAL_ERROR;
		goto out;
	}

	if (written < Stream_GetPosition(s))
	{
		WLog_WARN(TAG, "Unexpected bytes written: %" PRIu32 "/%" PRIuz "", written,
		          Stream_GetPosition(s));
	}

	ret = CHANNEL_RC_OK;
out:
	Stream_Free(s, TRUE);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpapplist_send_caps(RdpAppListServerContext *context, const RDPAPPLIST_SERVER_CAPS_PDU *caps)
{
	if (caps->appListProviderName.length > RDPAPPLIST_MAX_STRING_SIZE)
	{
		WLog_ERR(TAG, "rdpapplist_send_caps: appProviderName is too large.");
		return ERROR_INVALID_DATA;
	}

	int len = 2 + // version.
		  2 + caps->appListProviderName.length;

	wStream* s = rdpapplist_server_single_packet_new(RDPAPPLIST_CMDID_CAPS, len);

	if (!s)
	{
		WLog_ERR(TAG, "rdpapplist_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, caps->version);
	Stream_Write_UINT16(s, caps->appListProviderName.length);
	Stream_Write(s, caps->appListProviderName.string,
	             caps->appListProviderName.length);
	return rdpapplist_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpapplist_send_update_applist(RdpAppListServerContext* context, const RDPAPPLIST_UPDATE_APPLIST_PDU *updateAppList)
{
	UINT32 len = 4; // flags.
	if (updateAppList->flags & RDPAPPLIST_FIELD_ID)
	{
		if (updateAppList->appId.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, "rdpapplist_send_update_applist: appId is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + updateAppList->appId.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_GROUP)
	{
		if (updateAppList->appGroup.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, "rdpapplist_send_update_applist: appGroup is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + updateAppList->appGroup.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_EXECPATH)
	{
		if (updateAppList->appExecPath.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, "rdpapplist_send_update_applist: appExecPath is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + updateAppList->appExecPath.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_WORKINGDIR)
	{
		if (updateAppList->appWorkingDir.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, "rdpapplist_send_update_applist: appWorkingDir is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + updateAppList->appWorkingDir.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_DESC)
	{
		if (updateAppList->appDesc.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, "rdpapplist_send_update_applist: appDesc is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + updateAppList->appDesc.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_ICON)
	{
		if (!updateAppList->appIcon)
		{
			WLog_ERR(TAG, "rdpapplist_send_update_applist icon flag is set, but appIcon is NULL.");
			return ERROR_INVALID_DATA;
		}
		len += (7 * 4 + updateAppList->appIcon->iconBitsLength);
	}

	wStream* s = rdpapplist_server_single_packet_new(RDPAPPLIST_CMDID_UPDATE_APPLIST, len);

	if (!s)
	{
		WLog_ERR(TAG, "rdpapplist_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, updateAppList->flags);
	if (updateAppList->flags & RDPAPPLIST_FIELD_ID)
	{
		Stream_Write_UINT16(s, updateAppList->appId.length);
		Stream_Write(s, updateAppList->appId.string,
		             updateAppList->appId.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_GROUP)
	{
		Stream_Write_UINT16(s, updateAppList->appGroup.length);
		Stream_Write(s, updateAppList->appGroup.string,
		             updateAppList->appGroup.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_EXECPATH)
	{
		Stream_Write_UINT16(s, updateAppList->appExecPath.length);
		Stream_Write(s, updateAppList->appExecPath.string,
		             updateAppList->appExecPath.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_WORKINGDIR)
	{
		Stream_Write_UINT16(s, updateAppList->appWorkingDir.length);
		Stream_Write(s, updateAppList->appWorkingDir.string,
		             updateAppList->appWorkingDir.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_DESC)
	{
		Stream_Write_UINT16(s, updateAppList->appDesc.length);
		Stream_Write(s, updateAppList->appDesc.string,
		             updateAppList->appDesc.length);
	}
	if (updateAppList->flags & RDPAPPLIST_FIELD_ICON)
	{
		Stream_Write_UINT32(s, updateAppList->appIcon->flags);
		Stream_Write_UINT32(s, updateAppList->appIcon->iconWidth);
		Stream_Write_UINT32(s, updateAppList->appIcon->iconHeight);
		Stream_Write_UINT32(s, updateAppList->appIcon->iconStride);
		Stream_Write_UINT32(s, updateAppList->appIcon->iconBpp);
		Stream_Write_UINT32(s, updateAppList->appIcon->iconFormat);
		Stream_Write_UINT32(s, updateAppList->appIcon->iconBitsLength);
		Stream_Write(s, updateAppList->appIcon->iconBits,
			updateAppList->appIcon->iconBitsLength);
	}

	return rdpapplist_server_packet_send(context, s);
}

static UINT rdpapplist_send_delete_applist(RdpAppListServerContext* context, const RDPAPPLIST_DELETE_APPLIST_PDU *deleteAppList)
{
	UINT32 len = 4; // flags
	if (deleteAppList->flags & RDPAPPLIST_FIELD_ID)
	{
		if (deleteAppList->appId.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, " rdpapplist_send_delete_applist: appId is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + deleteAppList->appId.length);
	}
	if (deleteAppList->flags & RDPAPPLIST_FIELD_GROUP)
	{
		if (deleteAppList->appGroup.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, " rdpapplist_send_delete_applist: appGroup is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + deleteAppList->appGroup.length);
	}

	wStream* s = rdpapplist_server_single_packet_new(RDPAPPLIST_CMDID_DELETE_APPLIST, len);

	if (!s)
	{
		WLog_ERR(TAG, "rdpapplist_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, deleteAppList->flags);
	if (deleteAppList->flags & RDPAPPLIST_FIELD_ID)
	{
		Stream_Write_UINT16(s, deleteAppList->appId.length);
		Stream_Write(s, deleteAppList->appId.string,
		             deleteAppList->appId.length);
	}
	if (deleteAppList->flags & RDPAPPLIST_FIELD_GROUP)
	{
		Stream_Write_UINT16(s, deleteAppList->appGroup.length);
		Stream_Write(s, deleteAppList->appGroup.string,
		             deleteAppList->appGroup.length);
	}
	return rdpapplist_server_packet_send(context, s);
}

static UINT rdpapplist_send_delete_applist_provider(RdpAppListServerContext* context, const RDPAPPLIST_DELETE_APPLIST_PROVIDER_PDU *deleteAppListProvider)
{
	UINT32 len = 4; // flags.
	if (deleteAppListProvider->flags & RDPAPPLIST_FIELD_PROVIDER)
	{
		if (deleteAppListProvider->appListProviderName.length > RDPAPPLIST_MAX_STRING_SIZE)
		{
			WLog_ERR(TAG, " rdpapplist_send_delete_applist: appProviderName is too large.");
			return ERROR_INVALID_DATA;
		}
		len += (2 + deleteAppListProvider->appListProviderName.length);
	}

	wStream* s = rdpapplist_server_single_packet_new(RDPAPPLIST_CMDID_DELETE_APPLIST_PROVIDER, len);

	if (!s)
	{
		WLog_ERR(TAG, "rdpapplist_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, deleteAppListProvider->flags);
	if (deleteAppListProvider->flags & RDPAPPLIST_FIELD_PROVIDER)
	{
		Stream_Write_UINT16(s, deleteAppListProvider->appListProviderName.length);
		Stream_Write(s, deleteAppListProvider->appListProviderName.string,
		             deleteAppListProvider->appListProviderName.length);
	}
	return rdpapplist_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpapplist_server_open(RdpAppListServerContext* context)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	RdpAppListServerPrivate* priv = context->priv;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	void* buffer;
	buffer = NULL;
	priv->SessionId = WTS_CURRENT_SESSION;

	if (WTSQuerySessionInformationA(context->vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		rc = ERROR_INTERNAL_ERROR;
		goto out_close;
	}

	priv->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	priv->rdpapplist_channel = (HANDLE)WTSVirtualChannelOpenEx(priv->SessionId, RDPAPPLIST_DVC_CHANNEL_NAME,
	                                                     WTS_CHANNEL_OPTION_DYNAMIC);

	if (!priv->rdpapplist_channel)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed!");
		rc = GetLastError();
		goto out_close;
	}

	/* Query for channel event handle */
	if (!WTSVirtualChannelQuery(priv->rdpapplist_channel, WTSVirtualEventHandle, &buffer,
	                            &BytesReturned) ||
	    (BytesReturned != sizeof(HANDLE)))
	{
		WLog_ERR(TAG,
		         "WTSVirtualChannelQuery failed "
		         "or invalid returned size(%" PRIu32 ")",
		         BytesReturned);

		if (buffer)
			WTSFreeMemory(buffer);

		rc = ERROR_INTERNAL_ERROR;
		goto out_close;
	}

	CopyMemory(&priv->channelEvent, buffer, sizeof(HANDLE));
	WTSFreeMemory(buffer);

	if (priv->thread == NULL)
	{
		if (!(priv->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			rc = ERROR_INTERNAL_ERROR;
		}

		if (!(priv->thread =
		          CreateThread(NULL, 0, rdpapplist_server_thread_func, (void*)context, 0, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			CloseHandle(priv->stopEvent);
			priv->stopEvent = NULL;
			rc = ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
out_close:
	WTSVirtualChannelClose(priv->rdpapplist_channel);
	priv->rdpapplist_channel = NULL;
	priv->channelEvent = NULL;
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpapplist_server_close(RdpAppListServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	RdpAppListServerPrivate* priv = context->priv;

	if (priv->thread)
	{
		SetEvent(priv->stopEvent);

		if (WaitForSingleObject(priv->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		CloseHandle(priv->thread);
		CloseHandle(priv->stopEvent);
		priv->thread = NULL;
		priv->stopEvent = NULL;
	}

	if (priv->rdpapplist_channel)
	{
		WTSVirtualChannelClose(priv->rdpapplist_channel);
		priv->rdpapplist_channel = NULL;
	}

	return error;
}

RdpAppListServerContext* rdpapplist_server_context_new(HANDLE vcm)
{
	RdpAppListServerContext* context;
	RdpAppListServerPrivate* priv;
	context = (RdpAppListServerContext*)calloc(1, sizeof(RdpAppListServerContext));

	if (!context)
	{
		WLog_ERR(TAG, "rdpapplist_server_context_new(): calloc RdpAppListServerContext failed!");
		return NULL;
	}

	priv = context->priv = (RdpAppListServerPrivate*)calloc(1, sizeof(RdpAppListServerPrivate));

	if (!context->priv)
	{
		WLog_ERR(TAG, "rdpapplist_server_context_new(): calloc RdpAppListServerPrivate failed!");
		goto out_free;
	}

	priv->input_stream = Stream_New(NULL, 4);

	if (!priv->input_stream)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto out_free_priv;
	}

	context->vcm = vcm;
	context->Open = rdpapplist_server_open;
	context->Close = rdpapplist_server_close;
	context->ApplicationListCaps = rdpapplist_send_caps;
	context->UpdateApplicationList = rdpapplist_send_update_applist;
	context->DeleteApplicationList = rdpapplist_send_delete_applist;
	context->DeleteApplicationListProvider = rdpapplist_send_delete_applist_provider;
	priv->isReady = FALSE;
	return context;
out_free_priv:
	free(context->priv);
out_free:
	free(context);
	return NULL;
}

void rdpapplist_server_context_free(RdpAppListServerContext* context)
{
	if (!context)
		return;

	rdpapplist_server_close(context);

	if (context->priv)
	{
		Stream_Free(context->priv->input_stream, TRUE);
		free(context->priv);
	}

	free(context);
}
