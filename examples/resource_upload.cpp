#include <AHO/x/resource/texture.hpp>
#include <stdexcept>

// 管理するTextureとUploadBufferを再利用し、グリフの矩形だけを追加する。
VSL_NAMESPACE::Completion uploadGlyph(
    VSL_NAMESPACE::CommandManager manager,
    AHO_NAMESPACE::x::resource::Texture& atlas,
    AHO_NAMESPACE::x::resource::UploadBuffer& upload,
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
