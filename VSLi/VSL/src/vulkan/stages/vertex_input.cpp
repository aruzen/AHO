#include <VSL/vulkan/stages/vertex_input.hpp>
#include <VSL/vulkan/_pimpls.hpp>

#include <ranges>
#include <algorithm>
#include <limits>
#include <utility>
#include <VSL/exceptions.hpp>

namespace {
    std::uint32_t shape_stride(const VSL_NAMESPACE::pipeline_layout::VertexInputShapeDefinition& definition) {
        std::uint32_t next_offset = 0, extent = 0;
        for (const auto& layout : definition.layouts) {
            const auto offset = layout.offset == std::uint32_t(-1) ? next_offset : layout.offset;
            if (layout.format.size() > std::numeric_limits<std::uint32_t>::max() - offset)
                throw VSL_NAMESPACE::exceptions::RuntimeException("VertexInput", "Vertex attribute extent exceeds uint32 range");
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

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(data_format::Format format) {
    definitions.emplace_back(VertexInputShapeDefinition{.layouts = {VertexInputLayoutDefinition{.format = format}}});
    return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(std::initializer_list<data_format::Format> formats) {
    this->definitions.reserve(this->definitions.size() + formats.size());
    for (VertexInputShapeDefinition def :
         formats | std::views::transform([](data_format::Format format) { VertexInputShapeDefinition result{ .layouts = { VertexInputLayoutDefinition{ format } } }; return result; }))
        this->definitions.push_back(def);
    return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add_shape(std::initializer_list<data_format::Format> formats) {
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
            throw VSL_NAMESPACE::exceptions::RuntimeException("VertexInput", "Vertex input requirements overflow");
        size += stride;
    }
    return size;
}

void VSL_NAMESPACE::pipeline_layout::VertexInput::injection(VSL_NAMESPACE::PipelineLayoutEditAccessor pl) {
    auto& info = *pl.data_->info;

    using VIBDs = std::vector<vk::VertexInputBindingDescription>;
    using VIADs = std::vector<vk::VertexInputAttributeDescription>;

    VIBDs vertex_binding_descriptions;
    VIADs vertex_attribute_descriptions;

    std::uint32_t next_binding = 0, next_location = 0;
    for (auto& def : this->definitions) {
        vk::VertexInputBindingDescription vertex_binding_description;
        vertex_binding_description.binding = def.binding != (std::uint32_t)-1 ? def.binding : next_binding;
        next_binding = vertex_binding_description.binding + 1;

        vertex_binding_description.stride = shape_stride(def);
        vertex_binding_description.inputRate = (vk::VertexInputRate)def.update_timing;
        vertex_binding_descriptions.push_back(vertex_binding_description);

        std::uint32_t next_offset = 0;
        for (auto& layout : def.layouts) {
            /*
            loggingln("size     :", layout.format.size());
            loggingln("location :", layout.location);
            loggingln("binding  :", def.binding);*/

            vk::VertexInputAttributeDescription vertex_attribute_description;
            vertex_attribute_description.location = layout.location != (std::uint32_t)-1 ? layout.location : next_location;
            next_location = vertex_attribute_description.location + 1;
            vertex_attribute_description.binding = vertex_binding_description.binding;
            vertex_attribute_description.offset = layout.offset != (std::uint32_t)-1 ? layout.offset : next_offset;
            vertex_attribute_description.format = (vk::Format)layout.format.type();
            vertex_attribute_descriptions.push_back(vertex_attribute_description);
            /*
            loggingln("offset   :", vertexAttributeDescription.offset);
            loggingln("location :", vertexAttributeDescription.location);
            loggingln("binding  :", vertexAttributeDescription.binding); */
            next_offset = vertex_attribute_description.offset + layout.format.size();
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

    std::any next_bindings = std::move(vertex_binding_descriptions);
    std::any next_attributes = std::move(vertex_attribute_descriptions);
    auto& binding_storage = info.pool["vertexBindingDescriptions"];
    auto& attribute_storage = info.pool["vertexAttributeDescriptions"];
    binding_storage.swap(next_bindings);
    attribute_storage.swap(next_attributes);
    const auto& bindings = std::any_cast<const VIBDs&>(binding_storage);
    const auto& attributes = std::any_cast<const VIADs&>(attribute_storage);
    info.vertex_input.vertexBindingDescriptionCount = bindings.size();
    info.vertex_input.pVertexBindingDescriptions = bindings.data();
    info.vertex_input.vertexAttributeDescriptionCount = attributes.size();
    info.vertex_input.pVertexAttributeDescriptions = attributes.data();
}
