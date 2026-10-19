#include <VSL/vsl.hpp>
#include <VSL/vulkan/phase.hpp>
#include <AHO/image_readback.hpp>
#include <array>
#include <cstring>
#include <optional>
#include <stdexcept>

struct ReadbackExampleResult {
    std::uint32_t objectId = 0;
    std::uint64_t pickedFrame = 0;
    AHO_NAMESPACE::ImagePixels screenshot;
};

// 頂点シェーダーはgl_VertexIndexで三角形を生成し、フラグメントシェーダーはlocation 0にvec4、1にuintを出力する。
// テスト用triangle.vert / picking.fragと同じ入出力を想定する。
ReadbackExampleResult renderReadbackExample(VSL_NAMESPACE::LogicalDeviceAccessor device,
                                            VSL_NAMESPACE::CommandManager manager,
                                            VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Vertex> vertex,
                                            VSL_NAMESPACE::Shader<VSL_NAMESPACE::ShaderType::Fragment> fragment) {
    using namespace VSL_NAMESPACE;
    namespace pl = VSL_NAMESPACE::pipeline_layout;
    constexpr std::uint32_t width = 1280, height = 720;
    using OutputImage = Image<ImageType::ColorAttachment | ImageType::TransferSrc>;

    // 画像、RenderPass、FrameBuffer、Pipelineは一度作って再利用する。
    OutputImage color(device, width, height, data_format::UnsignedNormalized8RGBA);
    OutputImage ids(device, width, height, data_format::UnsignedInt);
    RenderPassDescription description;
    const auto colorId = description.addColor({.format = color.getFormat(), .finalLayout = ImageLayout::General});
    const auto objectId = description.addColor({.format = ids.getFormat(), .finalLayout = ImageLayout::General});
    RenderPass pass(device, description);
    auto target = FrameBuffer::fromAttachments(pass, width, height, {
                                                                        {objectId, ids}, {colorId, color}, // 登録順に合わせる必要はない。
                                                                    });
    Viewport viewport(0, 0, float(width), float(height));
    Scissor scissor(0, 0, float(width), float(height));
    PipelineLayout layout(device, pl::ShaderGroup("color-and-id", {vertex, fragment}),
                          pl::VertexInput{}, pl::InputAssembly{}, pl::Rasterization{pl::CullingMode::None},
                          pl::Multisample{}, pl::ColorBlend(std::vector<pl::ColorBlendAttachment>{
                                                 {.enabled = false},
                                                 {.enabled = false},
                                             }),
                          pl::DynamicState{}, viewport, scissor);
    GraphicsPipeline pipeline(layout, pass);

    // 2フレーム分のコマンドバッファを交互に再利用する。各Phaseで前回の送信完了を確認する。
    std::array<CommandManager, 2> frames{manager.independent(), manager.independent()};
    AHO_NAMESPACE::ImageReadback readback(manager, 0, 2); // 読み戻し用クラスは1つ、同時に保持できる結果は2件。
    std::optional<AHO_NAMESPACE::ImageReadbackTicket> picking, screenshot;
    AHO_NAMESPACE::ImagePixels selectedPixel;
    ReadbackExampleResult result;
    auto collect = [&] {
        if (picking && picking->ready()) {
            picking->read(selectedPixel);
            result.pickedFrame = picking->info().frameId;
            picking.reset(); // 回収後にスロットを返す。
        }
        if (screenshot && screenshot->ready()) {
            screenshot->read(result.screenshot);
            screenshot.reset();
        }
    };

    for (std::uint64_t frame = 0; frame < 3; ++frame) {
        collect();
        auto phase = frames[frame % frames.size()].startPhase<ComputePhase>();
        auto stream = phase << command::RenderPassBegin::withClears(target, {
                                                                                {objectId, ClearUint{}},
                                                                                {colorId, ClearColor{0, 0, 0, 1}},
                                                                            });
        stream << pipeline << viewport << scissor << command::Draw(3) << command::RenderPassEnd{};
        if (!picking)
            picking = readback.record(phase, ids, ImageLayout::General, {640, 360, 1, 1}, frame);
        if (frame == 0)
            screenshot = readback.record(phase, color, ImageLayout::General, {}, frame);
        if (phase.submit() != ComputePhase::SubmitResult::Success)
            throw std::runtime_error("Frame submission failed");
        // 描画と2件の読み戻しを1回の送信で行う。
        // スロット不足ならrecordはnulloptを返し、その要求は受理されない。
    }
    // 例の終了時だけ未回収の結果を待つ。通常のループではcollectでreadyを確認する。
    if (picking) {
        picking->read(selectedPixel);
        result.pickedFrame = picking->info().frameId;
    }
    if (screenshot)
        screenshot->read(result.screenshot);
    if (selectedPixel.bytes.size() == sizeof(result.objectId))
        std::memcpy(&result.objectId, selectedPixel.bytes.data(), sizeof(result.objectId));
    return result; // GPU利用中の描画リソースはframesの破棄時に完了を待ってから解放する。
}
