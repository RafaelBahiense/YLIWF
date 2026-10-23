#pragma once

#include "FollowerAdapter.h"
#include "PapyrusController.h"
#include "AdapterCommandState.h"
#include "SKSE/SKSE.h"
#include <atomic>
#include <functional>

namespace yliwf::sdk::papyrus {
    // Module-local helper; no STL or engine types cross the host API. Keep the
    // instance alive for the process lifetime, like the registered adapter.
    class Commands {
    public:
        using Verify = std::function<bool(ActorID, Command)>;

        bool Pending(ActorID actor) const {
            return state.Pending(actor);
        }

        void Invalidate() {
            state.Invalidate();
        }

        StartResult Dispatch(Object object, const char* method, const API* host, AdapterID adapter, ActorID actor,
                             Command command, RequestID request, Verify verify) {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm || !object || !host || !host->complete || !adapter || !actor || !request || !verify ||
                !HasMethod(object, method) || !state.Begin(actor, request))
                return StartResult::Rejected;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
                new Completion(*this, object, host, adapter, actor, command, request, std::move(verify))};
            if (vm->DispatchMethodCall(object, method, RE::MakeFunctionArguments(), callback))
                return StartResult::Pending;
            state.Finish(actor, request, state.Generation());
            return StartResult::Rejected;
        }

    private:
        class Completion final : public RE::BSScript::IStackCallbackFunctor {
        public:
            Completion(Commands& owner, const Object& controller, const API* host, AdapterID adapter, ActorID actor,
                       Command command, RequestID request, Verify verify)
                : owner(owner),
                  host(host),
                  adapter(adapter),
                  actor(actor),
                  command(command),
                  request(request),
                  generation(owner.state.Generation()),
                  verify(std::move(verify)),
                  object(controller) {}

            void operator()(RE::BSScript::Variable) override {
                if (returned.exchange(true))
                    return;
                if (auto* tasks = SKSE::GetTaskInterface())
                    tasks->AddTask([owner = &owner, host = host, adapter = adapter, actor = actor, command = command,
                                    request = request, generation = generation, verify = verify] {
                        // Session and actor state are accessed only on the game
                        // thread. A late callback must not clear a new call.
                        if (!owner->state.Finish(actor, request, generation))
                            return;
                        const bool success = verify(actor, command);
                        host->complete(adapter, request, success ? 1u : 0u,
                                       success ? "Owner method returned and follower state matches."
                                               : "Owner method returned without reaching the requested state.");
                    });
            }

            void SetObject(const Object& value) override {
                object = value;
            }

        private:
            Commands& owner;
            const API* host;
            AdapterID adapter;
            ActorID actor;
            Command command;
            RequestID request;
            std::uint64_t generation;
            Verify verify;
            Object object;
            std::atomic_bool returned{false};
        };

        CommandState state;
    };
}
