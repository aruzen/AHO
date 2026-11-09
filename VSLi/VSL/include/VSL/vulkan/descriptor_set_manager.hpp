//
// Created by morimoto_hibiki on 2025/10/24.
//

#ifndef AHO_ALL_DESCRIPTOR_SET_MANAGER_H
#define AHO_ALL_DESCRIPTOR_SET_MANAGER_H

#include <utility>

#include "../define.hpp"

#include "descriptor.hpp"

namespace VSL_NAMESPACE::graphic_resource {
    /*
     * 珍しく仮想関数による抽象化で実装しているが
     * makeなどを仮想関数でポリモーフィズムをおこなわないとAHO側のEngineに影響が出そうだからである
     */

    struct ManagerInterface {
    private:
        friend struct lifetime::detail::DescriptorManagerAccess;
        std::shared_ptr<impl::GraphicResourceManagerImpl> _state;
        std::shared_ptr<impl::GraphicResourceManagerIdentity> _identity;

    protected:
        ManagerInterface();
        explicit ManagerInterface(LogicalDeviceAccessor device);
        ManagerInterface(const ManagerInterface& other);
        ManagerInterface& operator=(const ManagerInterface& other);
        ManagerInterface(ManagerInterface&& other) noexcept;
        ManagerInterface& operator=(ManagerInterface&& other) noexcept;
        // 派生型の所有フィールドも公開してから旧値を回収する。
        void swap_state(ManagerInterface& other) noexcept;
        // 所有メンバーを持つ派生型はdestructor冒頭でも呼ぶ。
        void retire_identity() noexcept;
        void destroy_pool(const Pool& pool);

    public:
        virtual ~ManagerInterface();
        explicit operator bool() const noexcept { return bool(_state); }
        LogicalDeviceAccessor get_device() const;
        // 戻り値のvectorのサイズが同じになることを保証します
        virtual std::tuple<std::vector<Pool>, std::vector<Resource>>
        allocate(std::vector<BindingLayout> layouts) = 0;

        template<can_convert_graphic_resource T>
        std::tuple<std::vector<Pool>, std::vector<Resource>>
        allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers);

        template<can_convert_graphic_resource T>
        std::tuple<std::vector<Pool>, std::vector<Resource>>
        allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers, std::vector<size_t> bindings);

        Pool make(std::map<Type, size_t> count, size_t other = 0);

        std::tuple<Pool, std::vector<Resource>>
        make(std::vector<BindingLayout> layouts, std::map<Type, size_t> count = {}, size_t other = 0);

        template<can_convert_graphic_resource T>
        std::tuple<Pool, std::vector<Resource>>
        make(std::vector<BindingLayout> layouts, std::vector<T> buffers);

        template<can_convert_graphic_resource T>
        std::tuple<Pool, std::vector<Resource>>
        make(std::vector<BindingLayout> layouts, std::vector<T> buffers, std::vector<size_t> bindings);

        virtual void destroy(const Pool& pool) = 0;
    };

    /*
     * allocateがLayoutごとにPoolを取ることを保証します。
     * 必要がなくなった時にPoolを解放しても他のリソースには影響がありません。
     * 一時的に使用するようなリソースに使用してください。
     */
    struct ExactAllocateManager : public ManagerInterface {
        ExactAllocateManager(LogicalDeviceAccessor device);
        void swap(ExactAllocateManager& other) noexcept { swap_state(other); }

        std::tuple<std::vector<Pool>, std::vector<Resource>> allocate(std::vector<BindingLayout> layouts) override;

        void destroy(const Pool& pool) override;
    };

    /*
     * 最初に大きめのPoolを作った後allocateではそのPoolから割り当てられます。
     * Poolの解放は慎重に行ってください。
     */
    struct OverAllocateManager : public ManagerInterface {
        std::map<Type, size_t> size;
        size_t other = 0;
        std::vector<Pool> pools;

        OverAllocateManager(LogicalDeviceAccessor device);
        OverAllocateManager(LogicalDeviceAccessor device, std::map<Type, size_t> size, size_t other = 0);
        OverAllocateManager(const OverAllocateManager& other);
        OverAllocateManager(OverAllocateManager&& other) noexcept;
        OverAllocateManager& operator=(const OverAllocateManager& other);
        OverAllocateManager& operator=(OverAllocateManager&& other) noexcept;
        ~OverAllocateManager() override;
        void swap(OverAllocateManager& other) noexcept;

        std::tuple<std::vector<Pool>, std::vector<Resource>> allocate(std::vector<BindingLayout> layouts) override;

        void destroy(const Pool& pool) override;
    };
} // namespace VSL_NAMESPACE::graphic_resource

namespace VSL_NAMESPACE {
    using GraphicResourceManager = graphic_resource::ExactAllocateManager;
}

// ==================================================================================

template<VSL_NAMESPACE::graphic_resource::can_convert_graphic_resource T>
std::tuple<std::vector<VSL_NAMESPACE::graphic_resource::Pool>, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::ManagerInterface::allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers) {
    auto [pools, resources] = allocate(std::move(layouts));

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        resources[i].update(buffers[i], (size_t)0);
    }
    return {pools, resources};
}

template<VSL_NAMESPACE::graphic_resource::can_convert_graphic_resource T>
std::tuple<std::vector<VSL_NAMESPACE::graphic_resource::Pool>, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::ManagerInterface::allocate(std::vector<BindingLayout> layouts, std::vector<T> buffers,
                                                            std::vector<size_t> bindings) {
    auto [pools, resources] = allocate(std::move(layouts));

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        if (i < bindings.size())
            resources[i].update(buffers[i], bindings[i]);
        else
            resources[i].update(buffers[i], 0);
    }

    return {pools, resources};
}

template<VSL_NAMESPACE::graphic_resource::can_convert_graphic_resource T>
std::tuple<VSL_NAMESPACE::graphic_resource::Pool, std::vector<VSL_NAMESPACE::graphic_resource::Resource>>
VSL_NAMESPACE::graphic_resource::ManagerInterface::make(std::vector<BindingLayout> layouts, std::vector<T> buffers) {
    auto [pool, resources] = make(layouts);

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        resources[i].update(buffers[i], (size_t)0);
    }
    return {pool, resources};
}

template<VSL_NAMESPACE::graphic_resource::can_convert_graphic_resource T>
std::tuple<VSL_NAMESPACE::GraphicResourcePool, std::vector<VSL_NAMESPACE::GraphicResource>>
VSL_NAMESPACE::graphic_resource::ManagerInterface::make(std::vector<BindingLayout> layouts, std::vector<T> buffers,
                                                        std::vector<size_t> bindings) {
    auto [pool, resources] = make(layouts);

    for (size_t i = 0; i < resources.size() && i < buffers.size(); i++) {
        if (i < bindings.size())
            resources[i].update(buffers[i], bindings[i]);
        else
            resources[i].update(buffers[i], 0);
    }

    return {pool, resources};
}

#endif //AHO_ALL_DESCRIPTOR_SET_MANAGER_H
