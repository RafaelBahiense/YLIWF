#pragma once

namespace mod {
    // CommonLib's pinned NativeLatentFunction assigns GetRawType<T> instead
    // of invoking it for integer results. Keep the fix in our own adapter;
    // the dependency checkout and engine's latent-call protocol stay unchanged.
    template <class Result, class... Args>
    class NativeLatent final : public RE::NativeFunction<bool(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID,
                                                              RE::StaticFunctionTag*, Args...)> {
        using Base = RE::NativeFunction<bool(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID,
                                             RE::StaticFunctionTag*, Args...)>;
        using Callback = RE::BSScript::LatentStatus (*)(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID,
                                                        RE::StaticFunctionTag*, Args...);

    public:
        NativeLatent(std::string_view name, std::string_view script, Callback callback)
            : Base(name, script,
                   [](RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::StaticFunctionTag*, Args...) {
                       return false;
                   }) {
            this->_stub = [callback](RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack,
                                     RE::StaticFunctionTag* tag, Args... args) {
                return callback(vm, stack, tag, args...) == RE::BSScript::kStarted;
            };
            this->_retType = RE::BSScript::GetRawType<Result>{}();
            this->_isLatent = true;
        }
    };

    template <class Result, class... Args>
    void RegisterLatent(RE::BSScript::IVirtualMachine* vm, std::string_view name,
                        RE::BSScript::LatentStatus (*callback)(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID,
                                                               RE::StaticFunctionTag*, Args...)) {
        vm->BindNativeMethod(new NativeLatent<Result, Args...>(name, mod::info::NativeScript, callback));
    }
}
