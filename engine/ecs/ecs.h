#pragma once
#include <cstdint>
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace eng::ecs {
    using Entity = std::uint32_t;

    struct IStorage { virtual ~IStorage() = default; };

    template<typename T>
    struct Storage : IStorage {
        std::unordered_map<Entity, T> data;
    };

    class Registry {
        std::unordered_map<std::type_index, std::unique_ptr<IStorage>> stores;
        Entity next{1};
    public:
        Entity create() { return next++; }
        template<typename T>
        T& emplace(Entity e, T value = {}) {
            auto& s = getOrCreate<T>();
            return s.data.emplace(e, std::move(value)).first->second;
        }
        template<typename T>
        Storage<T>& getOrCreate() {
            auto it = stores.find(std::type_index(typeid(T)));
            if (it == stores.end()) {
                it = stores.emplace(std::type_index(typeid(T)), std::make_unique<Storage<T>>()).first;
            }
            return *static_cast<Storage<T>*>(it->second.get());
        }
    };
}

