#pragma once

#include "MenuTabs.h"

namespace mod::ui {
    const yliwf::menu::API* MenuAPI();
    void RenderExtension(const yliwf::menu::Tab& tab);
#ifdef YLIWF_UI_PREVIEW
    void ResetPreviewTabs();
#endif
}
