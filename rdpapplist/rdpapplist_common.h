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

#ifndef FREERDP_CHANNEL_RDPAPPLIST_COMMON_H
#define FREERDP_CHANNEL_RDPAPPLIST_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/api.h>
#include <rdpapplist_protocol.h>

FREERDP_LOCAL UINT rdpapplist_read_header(wStream* s, RDPAPPLIST_HEADER* header);
FREERDP_LOCAL UINT rdpapplist_write_header(wStream* s, const RDPAPPLIST_HEADER* header);

#endif /* FREERDP_CHANNEL_RDPAPPLIST_COMMON_H */
