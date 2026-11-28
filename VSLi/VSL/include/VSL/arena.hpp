#pragma once
#include "define.hpp"

#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace VSL_NAMESPACE {
    // 寿命を共にする資源のImplを、固定容量の連続したCPU領域へ配置する。
    //
    // - emplace()が返すshared_ptrはArena全体の寿命を共有する(aliasing)。
    //   Arenaと配置した全資源のshared_ptrが消えた時点で、構築の逆順に破棄する。
    // - 配置後に移動・拡張しない。容量を超えるとstd::length_errorを送出する。
    // - Vulkanの型を知らない。資源ごとの生成・破棄はImplのinitとデストラクタが担う。
    // - emplace()はスレッド安全ではない。
    class Arena {
        static constexpr std::size_t BASE_ALIGNMENT = 64;

        struct Header {
            void (*destroy)(void*) noexcept;
            void* object;
            Header* previous;
        };

        struct Block {
            std::byte* storage;
            std::size_t capacity;
            std::size_t used = 0;
            std::size_t count = 0;
            Header* last = nullptr;
            bool constructing = false;

            explicit Block(std::size_t capacity)
                : storage(static_cast<std::byte*>(::operator new(capacity, std::align_val_t{BASE_ALIGNMENT}))), capacity(capacity) {}
            Block(const Block&) = delete;
            Block& operator=(const Block&) = delete;
            ~Block() {
                for (auto* header = last; header; header = header->previous)
                    header->destroy(header->object);
                ::operator delete(storage, std::align_val_t{BASE_ALIGNMENT});
            }
        };

        static constexpr std::size_t AlignUp(std::size_t value, std::size_t alignment) {
            return (value + alignment - 1) / alignment * alignment;
        }

        std::shared_ptr<Block> _block;

    public:
        explicit Arena(std::size_t capacity) : _block(std::make_shared<Block>(capacity)) {}
        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;
        // 移動元は空になり、以後の操作はできない。
        Arena(Arena&&) noexcept = default;
        Arena& operator=(Arena&&) noexcept = default;

        // Tを既定構築し、init(T&)で初期化する。initが例外を送出した場合は
        // Tを破棄して領域を巻き戻し、例外をそのまま伝える。
        template<typename T, typename Init>
        std::shared_ptr<T> emplace(Init&& init);

        // 型Tをn個配置するのに必要な最大バイト数。容量の見積もりに使う。
        template<typename T>
        static constexpr std::size_t BytesFor(std::size_t n = 1) {
            return n * ((alignof(Header) - 1) + sizeof(Header) + (alignof(T) - 1) + sizeof(T));
        }

        std::size_t capacity() const { return _block->capacity; }
        std::size_t used() const { return _block->used; }
        std::size_t count() const { return _block->count; }
    };

    template<typename T, typename Init>
    std::shared_ptr<T> Arena::emplace(Init&& init) {
        static_assert(alignof(T) <= BASE_ALIGNMENT, "Arena cannot satisfy this alignment");
        static_assert(std::is_nothrow_destructible_v<T>);
        auto& block = *_block;
        if (block.constructing)
            throw std::logic_error("Arena::emplace cannot be nested inside init");
        const auto header_offset = AlignUp(block.used, alignof(Header));
        const auto object_offset = AlignUp(header_offset + sizeof(Header), alignof(T));
        const auto end = object_offset + sizeof(T);
        if (end > block.capacity)
            throw std::length_error("Arena capacity exceeded");

        auto* object = std::construct_at(reinterpret_cast<T*>(block.storage + object_offset));
        block.constructing = true;
        try {
            std::forward<Init>(init)(*object);
        } catch (...) {
            block.constructing = false;
            std::destroy_at(object);
            throw;
        }
        block.constructing = false;
        auto* header = std::construct_at(reinterpret_cast<Header*>(block.storage + header_offset),
                                         Header{[](void* p) noexcept { std::destroy_at(static_cast<T*>(p)); }, object, block.last});
        block.last = header;
        block.used = end;
        ++block.count;
        return std::shared_ptr<T>(_block, object);
    }

    // heapとarenaで共通の生成入口。arenaがnullptrならheapへ配置する。
    template<typename T, typename Init>
    std::shared_ptr<T> place(Arena* arena, Init&& init) {
        if (arena)
            return arena->emplace<T>(std::forward<Init>(init));
        auto object = std::make_shared<T>();
        std::forward<Init>(init)(*object);
        return object;
    }

    // 資源fromが資源toを参照するためのshared_ptrを返す。
    // 同じ所有単位(同じArena)なら循環を避けて非所有、別の所有単位なら所有する。
    //
    // 非所有でも安全な理由: 同じArena内は構築の逆順に破棄する。構築時に結んだ参照の先は
    // 必ず先に構築されているため、fromのデストラクタ中も生存している。構築後に結んだ参照は
    // 生存の保持だけに使い、デストラクタから辿ってはならない。
    template<typename T, typename U>
    std::shared_ptr<T> link(const std::shared_ptr<U>& from, std::shared_ptr<T> to) noexcept {
        if (from && to && !from.owner_before(to) && !to.owner_before(from))
            return std::shared_ptr<T>(std::shared_ptr<T>{}, to.get());
        return to;
    }

    // link()で得た参照を外部へ渡すとき、所有を持つshared_ptrへ戻す。
    // 非所有の参照はfromと同じArenaにあるため、fromの所有で包めばよい。
    template<typename T, typename U>
    std::shared_ptr<T> expose(const std::shared_ptr<U>& from, const std::shared_ptr<T>& to) noexcept {
        if (to && to.use_count() == 0)
            return std::shared_ptr<T>(from, to.get());
        return to;
    }
} // namespace VSL_NAMESPACE
