#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpapplist.common")

#include "rdpapplist_common.h"

#define RDPAPPLIST_HEADER_SIZE 8U

/**
 * Reads an RDPAPPLIST header from the stream.
 *
 * The header format is:
 *   UINT32 cmdId
 *   UINT32 length
 *
 * @param s The input stream.
 * @param header The destination header structure.
 *
 * @return CHANNEL_RC_OK on success, otherwise a Win32 error code.
 */
UINT rdpapplist_read_header(wStream* s, RDPAPPLIST_HEADER* header)
{
	if (!s || !header)
	{
		WLog_ERR(TAG, "header parsing failed: invalid argument");
		return ERROR_INVALID_PARAMETER;
	}

	const size_t remaining = Stream_GetRemainingLength(s);
	if (remaining < RDPAPPLIST_HEADER_SIZE)
	{
		WLog_ERR(TAG,
		         "header parsing failed: not enough data (remaining=%" PRIuz
		         ", required=%u)",
		         remaining, RDPAPPLIST_HEADER_SIZE);
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, header->cmdId);
	Stream_Read_UINT32(s, header->length);

	return CHANNEL_RC_OK;
}

/**
 * Writes an RDPAPPLIST header to the stream.
 *
 * The header format is:
 *   UINT32 cmdId
 *   UINT32 length
 *
 * @param s The output stream.
 * @param header The source header structure.
 *
 * @return CHANNEL_RC_OK on success, otherwise a Win32 error code.
 */
UINT rdpapplist_write_header(wStream* s, const RDPAPPLIST_HEADER* header)
{
	if (!s || !header)
	{
		WLog_ERR(TAG, "header write failed: invalid argument");
		return ERROR_INVALID_PARAMETER;
	}

	if (Stream_GetRemainingLength(s) < RDPAPPLIST_HEADER_SIZE)
	{
		WLog_ERR(TAG, "header write failed: insufficient stream capacity");
		return ERROR_INSUFFICIENT_BUFFER;
	}

	Stream_Write_UINT32(s, header->cmdId);
	Stream_Write_UINT32(s, header->length);

	return CHANNEL_RC_OK;
}
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
 *     http://www.apache.org/licenses/LICENSE-2.0
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

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpapplist.common")

#include "rdpapplist_common.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpapplist_read_header(wStream* s, RDPAPPLIST_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "header parsing failed: not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, header->cmdId);
	Stream_Read_UINT32(s, header->length);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpapplist_write_header(wStream* s, const RDPAPPLIST_HEADER* header)
{
	Stream_Write_UINT32(s, header->cmdId);
	Stream_Write_UINT32(s, header->length);
	return CHANNEL_RC_OK;
}
