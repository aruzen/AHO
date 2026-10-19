#include <VSL/vulkan/stages/vertex_input.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <ranges>
#include <algorithm>
#include <limits>
#include <utility>
#include <VSL/exceptions.hpp>

namespace {
    std::uint32_t shape_stride(const vsl::pipeline_layout::VertexInputShapeDefinition& definition) {
        std::uint32_t next_offset = 0, extent = 0;
        for (const auto& layout : definition.layouts) {
            const auto offset = layout.offset == std::uint32_t(-1) ? next_offset : layout.offset;
            if (layout.format.size() > std::numeric_limits<std::uint32_t>::max() - offset)
                throw vsl::exceptions::RuntimeException("VertexInput", "Vertex attribute extent exceeds uint32 range");
            next_offset = offset + layout.format.size();
            extent = std::max(extent, next_offset);
        }
        return definition.size == std::uint32_t(-1) ? extent : definition.size;
    }
} // namespace

VSL_NAMESPACE::pipeline_layout::VertexInput::VertexInput() {}

VSL_NAMESPACE::pipeline_layout::VertexInput::VertexInput(std::initializer_list<VertexInputShapeDefinition> definitions) {
    add(definitions);
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(data_format::___Format format) {
    definitions.emplace_back(VertexInputShapeDefinition{.layouts = {VertexInputLayoutDefinition{.format = format}}});
    return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(std::initializer_list<data_format::___Format> formats) {
    this->definitions.reserve(this->definitions.size() + formats.size());
    for (VertexInputShapeDefinition def :
         formats | std::views::transform([](data_format::___Format format) { VertexInputShapeDefinition result{ .layouts = { VertexInputLayoutDefinition{ format } } }; return result; }))
        this->definitions.push_back(def);
    return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add_shape(std::initializer_list<data_format::___Format> formats) {
    VertexInputShapeDefinition def;
    def.layouts.reserve(formats.size());
    auto itr = formats.begin();
    for (const auto& format : formats)
        def.layouts.push_back(VertexInputLayoutDefinition{format});
    def.layouts.shrink_to_fit();
    definitions.push_back(def);
    return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(VertexInputShapeDefinition definition) {
    this->definitions.push_back(definition);
    return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(std::initializer_list<VertexInputShapeDefinition> definitions) {
    this->definitions.reserve(this->definitions.size() + definitions.size());
    this->definitions.insert(this->definitions.begin(), definitions.begin(), definitions.end());
    return *this;
}

size_t VSL_NAMESPACE::pipeline_layout::VertexInput::requirements_size() {
    size_t size = 0;
    for (auto& def : definitions) {
        const auto stride = shape_stride(def);
        if (stride > std::numeric_limits<size_t>::max() - size)
            throw vsl::exceptions::RuntimeException("VertexInput", "Vertex input requirements overflow");
        size += stride;
    }
    return size;
}

void VSL_NAMESPACE::pipeline_layout::VertexInput::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl) {
    auto& info = *pl._data->info;

    using VIBDs = std::vector<vk::VertexInputBindingDescription>;
    using VIADs = std::vector<vk::VertexInputAttributeDescription>;

    VIBDs vertexBindingDescriptions;
    VIADs vertexAttributeDescriptions;

    std::uint32_t nextBinding = 0, nextLocation = 0;
    for (auto& def : this->definitions) {
        vk::VertexInputBindingDescription vertexBindingDescription;
        vertexBindingDescription.binding = def.binding != (std::uint32_t)-1 ? def.binding : nextBinding;
        nextBinding = vertexBindingDescription.binding + 1;

        vertexBindingDescription.stride = shape_stride(def);
        vertexBindingDescription.inputRate = (vk::VertexInputRate)def.updateTiming;
        vertexBindingDescriptions.push_back(vertexBindingDescription);

        std::uint32_t nextOffset = 0;
        for (auto& layout : def.layouts) {
            /*
            loggingln("size     :", layout.format.size());
            loggingln("location :", layout.location);
            loggingln("binding  :", def.binding);*/

            vk::VertexInputAttributeDescription vertexAttributeDescription;
            vertexAttributeDescription.location = layout.location != (std::uint32_t)-1 ? layout.location : nextLocation;
            nextLocation = vertexAttributeDescription.location + 1;
            vertexAttributeDescription.binding = vertexBindingDescription.binding;
            vertexAttributeDescription.offset = layout.offset != (std::uint32_t)-1 ? layout.offset : nextOffset;
            vertexAttributeDescription.format = (vk::Format)layout.format.type();
            vertexAttributeDescriptions.push_back(vertexAttributeDescription);
            /*
            loggingln("offset   :", vertexAttributeDescription.offset);
            loggingln("location :", vertexAttributeDescription.location);
            loggingln("binding  :", vertexAttributeDescription.binding); */
            nextOffset = vertexAttributeDescription.offset + layout.format.size();
        }
        // loggingln("");
    }

    /*
    vertexBindingDescriptions.push_back(vk::VertexInputBindingDescription{
        .binding = 0, .stride = sizeof(float) * 5, .inputRate = vk::VertexInputRate::eVertex
        });
    vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription{
        .location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = 0
        });
    vertexAttributeDescriptions.push_back(vk::VertexInputAttributeDescription{
        .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = 8
        });
    */

    std::any nextBindings = std::move(vertexBindingDescriptions);
    std::any nextAttributes = std::move(vertexAttributeDescriptions);
    auto& bindingStorage = info.pool["vertexBindingDescriptions"];
    auto& attributeStorage = info.pool["vertexAttributeDescriptions"];
    bindingStorage.swap(nextBindings);
    attributeStorage.swap(nextAttributes);
    const auto& bindings = std::any_cast<const VIBDs&>(bindingStorage);
    const auto& attributes = std::any_cast<const VIADs&>(attributeStorage);
    info.vertexInput.vertexBindingDescriptionCount = bindings.size();
    info.vertexInput.pVertexBindingDescriptions = bindings.data();
    info.vertexInput.vertexAttributeDescriptionCount = attributes.size();
    info.vertexInput.pVertexAttributeDescriptions = attributes.data();
}
