#include <doctest/doctest.h>
#include "ui/MenuTabRegistry.h"
#include <cstdio>
#include <cstring>

namespace {
    void __stdcall Draw(void*, const yliwf::menu::Context*) {}

    void __stdcall OtherDraw(void* context, const yliwf::menu::Context*) {
        if (context)
            ++*static_cast<std::uint32_t*>(context);
    }

    yliwf::menu::Tab Tab() {
        yliwf::menu::Tab tab;
        std::snprintf(tab.id, sizeof(tab.id), "test.tab");
        std::snprintf(tab.title, sizeof(tab.title), "Test tab");
        tab.render = Draw;
        return tab;
    }
}

TEST_CASE("Tab registration copies labels and recognizes identical registrations") {
    mod::menu_tabs::Registry registry;
    auto tab = Tab();
    const auto first = registry.Add(tab);
    CHECK(first.handle == 1);
    CHECK(first.inserted);
    CHECK(registry.Add(tab).handle == first.handle);
    CHECK_FALSE(registry.Add(tab).inserted);
    std::snprintf(tab.title, sizeof(tab.title), "Changed");
    CHECK(registry.Add(tab).handle == 0);
    CHECK(std::strcmp(registry.At(0)->title, "Test tab") == 0);
    tab = Tab();
    tab.render = OtherDraw;
    CHECK(registry.Add(tab).handle == 0);
    tab = Tab();
    std::uint32_t context = 0;
    tab.context = &context;
    CHECK(registry.Add(tab).handle == 0);
    CHECK(registry.At(1) == nullptr);
}

TEST_CASE("Tab registration rejects invalid ABI labels and missing renderers") {
    auto tab = Tab();
    REQUIRE(mod::menu_tabs::Valid(tab));
    tab.size = 0;
    CHECK_FALSE(mod::menu_tabs::Valid(tab));
    tab = Tab();
    tab.version++;
    CHECK_FALSE(mod::menu_tabs::Valid(tab));
    tab = Tab();
    tab.render = nullptr;
    CHECK_FALSE(mod::menu_tabs::Valid(tab));
    tab = Tab();
    tab.id[0] = 0;
    CHECK_FALSE(mod::menu_tabs::Valid(tab));
    tab = Tab();
    std::memset(tab.title, 'x', sizeof(tab.title));
    CHECK_FALSE(mod::menu_tabs::Valid(tab));
}

TEST_CASE("Tab registry capacity does not replace existing callbacks") {
    mod::menu_tabs::Registry registry;
    for (std::uint32_t i = 0; i < yliwf::menu::MaxTabs; ++i) {
        auto tab = Tab();
        std::snprintf(tab.id, sizeof(tab.id), "tab.%u", i);
        CHECK(registry.Add(tab).handle == i + 1);
    }
    auto excess = Tab();
    CHECK(registry.Add(excess).handle == 0);
    CHECK(registry.At(0)->render == Draw);
    CHECK(registry.At(yliwf::menu::MaxTabs) == nullptr);
}
