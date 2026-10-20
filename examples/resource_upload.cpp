#include <AHO/x/resource/texture.hpp>
#include <stdexcept>

// 管理するTextureとGPUFriendly Bufferを再利用し、グリフの矩形だけを追加する。
VSL_NAMESPACE::Completion uploadGlyph(
    VSL_NAMESPACE::CommandManager manager,
    AHO_NAMESPACE::x::resource::Texture& atlas,
    AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::GPUFriendly>& upload,
    std::span<const std::byte> rgba,
    VSL_NAMESPACE::ImageRegion region) {
    if (!upload.wait())
        throw std::logic_error("Submit or cancel the previous recording before writing");
    upload.assign(rgba);
    auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
    phase << atlas.update(upload, region);
    if (phase.submit() != VSL_NAMESPACE::ComputePhase::SubmitResult::Success)
        throw std::runtime_error("Texture upload submission failed");
    return phase.completion();
}

namespace {
    struct BufferCustomization {
        auto map(VSL_NAMESPACE::StagingBuffer<>& storage) const { return storage.data(); }
    };
} // namespace

// VSLで用意したバッファも同じAHOの転送経路で使う。
VSL_NAMESPACE::Completion uploadCustomBuffer(
    VSL_NAMESPACE::CommandManager manager,
    AHO_NAMESPACE::resource::Buffer<AHO_NAMESPACE::resource::BufferType::OnGPU>& destination,
    VSL_NAMESPACE::StagingBuffer<> source) {
    auto custom = AHO_NAMESPACE::resource::make_custom_buffer(std::move(source), BufferCustomization{});
    auto phase = manager.startPhase<VSL_NAMESPACE::ComputePhase>();
    phase << destination.upload(custom);
    if (phase.submit() != VSL_NAMESPACE::ComputePhase::SubmitResult::Success)
        throw std::runtime_error("Buffer upload submission failed");
    return phase.completion();
}
