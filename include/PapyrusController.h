#pragma once

// Optional CommonLib helpers for adapters that call existing quest controllers.
// This header does not expose engine types through the host's DLL ABI.
#include "RE/Skyrim.h"
#include <algorithm>
#include <optional>
#include <string_view>

namespace yliwf::sdk::papyrus {
    using Object = RE::BSTSmartPointer<RE::BSScript::Object>;

    inline bool Equal(std::string_view left, std::string_view right) {
        auto lower = [](unsigned char value) { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; };
        return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
                                                         [&](char a, char b) { return lower(a) == lower(b); });
    }

    inline Object Bound(RE::TESQuest* quest, const char* script) {
        Object object;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (quest && policy)
            vm->FindBoundObject(policy->GetHandleForObject(RE::FormType::Quest, quest), script, object);
        return object && object->IsInitialized() ? object : Object{};
    }

    inline RE::BSScript::Variable* Field(const Object& object, const char* name) {
        if (!object)
            return nullptr;
        if (auto* value = object->GetProperty(name))
            return value;
        for (auto* type = object->GetTypeInfo(); type; type = type->GetParent()) {
            const auto* properties = type->GetPropertyIter();
            for (std::uint32_t i = 0; properties && i < type->GetNumProperties(); ++i)
                if (Equal(properties[i].name.c_str(), name))
                    return object->GetProperty(properties[i].name);
        }
        return nullptr;
    }

    inline std::optional<bool> Boolean(const Object& object, const char* name) {
        auto* value = Field(object, name);
        return value && value->IsBool() ? std::optional(value->GetBool()) : std::nullopt;
    }

    inline bool HasMethod(const Object& object, const char* name) {
        if (!object)
            return false;
        for (auto* type = object->GetTypeInfo(); type; type = type->GetParent()) {
            const auto* functions = type->GetMemberFuncIter();
            for (std::uint32_t i = 0; functions && i < type->GetNumMemberFuncs(); ++i)
                if (functions[i].func && Equal(functions[i].func->GetName().c_str(), name) &&
                    functions[i].func->GetParamCount() == 0)
                    return true;
        }
        return false;
    }
}
