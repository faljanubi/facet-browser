#include "stdafx.h"
#include "version.h"

DECLARE_COMPONENT_VERSION(
	FB_NAME,
	FB_VERSION,
	FB_NAME " " FB_VERSION "\n"
	"Created by " FB_AUTHOR "\n\n"
	"Gives quick access to Media Library content with customizable list views: "
	"linked panes, multiple columns, a folder column, album art views, statistics, "
	"search and filters.\n\n"
	"Works like the Facets component by Frank Bicking (2007-2011), rebuilt as a native "
	"64-bit component with support for the foobar2000 2.x dark mode. "
	"Independent reimplementation; it shares no code with Facets.\n\n"
	"Free software under the GNU GPL version 3 or later, with no warranty.\n"
	"Source code: https://github.com/faljanubi/facet-browser"
);

VALIDATE_COMPONENT_FILENAME("foo_facet_browser.dll");

FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;
