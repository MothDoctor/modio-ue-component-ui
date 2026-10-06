/*
 *  Copyright (C) 2024 mod.io Pty Ltd. <https://mod.io>
 *
 *  This file is part of the mod.io UE Plugin.
 *
 *  Distributed under the MIT License. (See accompanying file LICENSE or
 *   view online at <https://github.com/modio/modio-ue/blob/main/LICENSE>)
 *
 */

// Suppress 4602 "#pragma pop_macro: 'LOCTEXT_NAMESPACE' no previous #pragma push_macro for this identifier"
// so  we don't get a warning around this when we do strict include builds
#pragma warning(push)
#pragma warning(disable : 4602)
#pragma pop_macro("LOCTEXT_NAMESPACE")
#pragma warning(pop)
