#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

// foobar2000 SDK + ATL/WTL
#include <helpers/foobar2000+atl.h>

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>
#include <helpers/CListControlFb2kColors.h>
#include <libPPUI/CListControlOwnerData.h>
#include <libPPUI/win32_op.h>
#include <libPPUI/win32_utility.h>
#include <libPPUI/wtl-pp.h>

#include <SDK/library_callbacks.h>
#include <SDK/search_tools.h>
#include <SDK/album_art.h>
#include <SDK/playlist.h>
#include <SDK/titleformat.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <shlwapi.h>
#include <wincodec.h>
