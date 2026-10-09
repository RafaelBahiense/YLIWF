#pragma once

#include <type_traits>
#include "Strings.h"
#include "ControllerState.h"

namespace mod::controller::storage {
    using Script = RE::BSTSmartPointer<RE::BSScript::Object>;
    State& Get();

    inline ActiveOperation& Active() {
        return *Get().active;
    }

    void Configure(RE::TESQuest* quest);
    void RegisterSerialization();
    void Reset();
    void Loaded();
    bool Available();
    const std::string& LoadFailure();
    RE::Actor* Actor(std::uint64_t handle);
    Handle ActorHandle(RE::Actor* actor);
    void SetQueue(std::vector<Command> queue);
    void SetActive(std::optional<ActiveOperation> active);
    bool SetActor(std::uint64_t& destination, RE::Actor* actor);
    Caller CaptureCaller(RE::VMStackID stack, std::int32_t request);
    bool CurrentCaller(const Caller& caller);
    std::vector<RE::BGSRefAlias*> ExtraAliases();
    RE::TESGlobal* PartyCount();
    RE::TESGlobal* RecruitGate();

    inline RE::BSScript::Variable* Field(const Script& object, const char* name) {
        if (!object)
            return nullptr;
        auto* value = object->GetProperty(name);
        if (value)
            return value;
        if (auto* variable = object->GetVariable(name))
            return variable;
        // Saved/compiler spellings can differ in case. Resolve the actual name
        // before asking CommonLib for the remaining vanilla property storage.
        for (auto* type = object->GetTypeInfo(); type; type = type->GetParent()) {
            const auto* properties = type->GetPropertyIter();
            for (std::uint32_t i = 0; properties && i < type->GetNumProperties(); ++i)
                if (mod::strings::EqualsIgnoreCase(properties[i].name.c_str(), name))
                    return object->GetProperty(properties[i].name);
            const auto* variables = type->GetVariableIter();
            for (std::uint32_t i = 0; variables && i < type->GetNumVariables(); ++i)
                if (mod::strings::EqualsIgnoreCase(variables[i].name.c_str(), name))
                    return object->GetVariable(variables[i].name);
        }
        return nullptr;
    }

    template <class T>
    T Read(const Script& object, const char* name) {
        auto* value = Field(object, name);
        if (!value || value->GetType().GetRawType() == RE::BSScript::TypeInfo::RawType::kNone || value->IsNoneObject())
            return {};
        using Raw = RE::BSScript::TypeInfo::RawType;
        if constexpr (std::is_same_v<T, bool>) {
            if (!value->IsBool())
                return {};
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            if (!value->IsInt())
                return {};
        } else if constexpr (std::is_same_v<T, float>) {
            if (!value->IsFloat())
                return {};
        } else if constexpr (std::is_pointer_v<T>) {
            if (!value->IsObject())
                return {};
        } else if constexpr (std::is_same_v<T, RE::BSFixedString>) {
            if (!value->IsString())
                return {};
        } else if constexpr (std::is_same_v<T, std::vector<std::int32_t>>) {
            if (value->GetType().GetRawType() != Raw::kIntArray)
                return {};
        } else if constexpr (std::is_same_v<T, std::vector<float>>) {
            if (value->GetType().GetRawType() != Raw::kFloatArray)
                return {};
        } else {
            if (!value->IsObjectArray())
                return {};
        }
        return value->Unpack<T>();
    }

    template <class T>
    bool Write(const Script& object, const char* name, T value) {
        auto* field = Field(object, name);
        if (!field) {
            logger::error("Controller saved field {} is unavailable", name);
            return false;
        }
        RE::BSScript::Variable packed;
        packed.Pack(value);
        if (packed.GetType().GetRawType() == RE::BSScript::TypeInfo::RawType::kNone)
            return false;
        *field = std::move(packed);
        return true;
    }
}
