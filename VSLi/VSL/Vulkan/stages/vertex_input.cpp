#ifdef _MSC_VER
#include "pch.h"
#endif
#include "vertex_input.h"
#include "../_pimpls.h"

#include <ranges>

VSL_NAMESPACE::pipeline_layout::VertexInput::VertexInput() {}

VSL_NAMESPACE::pipeline_layout::VertexInput::VertexInput(std::initializer_list<VertexInputShapeDefinition> definitions) {
	add(definitions);
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(data_format::___Format format)
{
    definitions.emplace_back(VertexInputShapeDefinition{.layouts = {VertexInputLayoutDefinition{.format = format}}});
	return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(std::initializer_list<data_format::___Format> formats)
{
	this->definitions.reserve(this->definitions.size() + formats.size());
    for (VertexInputShapeDefinition def:
            formats
            | std::views::transform([](data_format::___Format format)
				{ VertexInputShapeDefinition result{ .layouts = { VertexInputLayoutDefinition{ format } } }; return result;  }))
		this->definitions.push_back(def);
	return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add_shape(std::initializer_list<data_format::___Format> formats)
{
	VertexInputShapeDefinition def;
	def.layouts.reserve(formats.size());
	auto itr = formats.begin();
	for (const auto& format : formats)
		def.layouts.push_back(VertexInputLayoutDefinition{ format });
	def.layouts.shrink_to_fit();
	definitions.push_back(def);
	return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(VertexInputShapeDefinition definition)
{
	this->definitions.push_back(definition);
	return *this;
}

VSL_NAMESPACE::pipeline_layout::VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(std::initializer_list<VertexInputShapeDefinition> definitions)
{
	this->definitions.reserve(this->definitions.size() + definitions.size());
	this->definitions.insert(this->definitions.begin(), definitions.begin(), definitions.end());
	return *this;
}

size_t VSL_NAMESPACE::pipeline_layout::VertexInput::requirements_size() {
    size_t size = 0;
    for (auto &def: definitions) {
        if (def.size != (std::uint32_t) -1)
            size += def.size;
        else
            for (auto &layout: def.layouts)
                size += layout.format.size();
    }
    return size;
}

void VSL_NAMESPACE::pipeline_layout::VertexInput::injection(VSL_NAMESPACE::PipelineLayoutAccessor pl)
{
	auto& info = *pl._data->info;

	using VIBDs = std::vector<vk::VertexInputBindingDescription>;
	using VIADs = std::vector<vk::VertexInputAttributeDescription>;

	auto& vertexBindingDescriptions = std::any_cast<VIBDs&>(info.pool["vertexBindingDescriptions"] = VIBDs());
	auto& vertexAttributeDescriptions = std::any_cast<VIADs&>(info.pool["vertexAttributeDescriptions"] = VIADs());

	std::uint32_t nextBinding = 0, nextLocation = 0;
	for (auto& def : this->definitions) {
		vk::VertexInputBindingDescription vertexBindingDescription;
		vertexBindingDescription.binding = def.binding != (std::uint32_t)-1 ? def.binding : nextBinding;
		nextBinding = vertexBindingDescription.binding + 1;

        if (def.size != (std::uint32_t) -1)
            vertexBindingDescription.stride = def.size;
        else {
            vertexBindingDescription.stride = 0;
            for (auto &layout: def.layouts)
                vertexBindingDescription.stride += layout.format.size();
        }
		vertexBindingDescription.inputRate = (vk::VertexInputRate)def.updateTiming;
		vertexBindingDescriptions.push_back(vertexBindingDescription);

        std::uint32_t nextOffset = 0;
		for (auto& layout : def.layouts) {
			vk::VertexInputAttributeDescription vertexAttributeDescription;
			vertexAttributeDescription.location = layout.location != (std::uint32_t)-1 ? layout.location : nextLocation;
			nextLocation = vertexAttributeDescription.location + 1;
			vertexAttributeDescription.binding = vertexBindingDescription.binding;
			vertexAttributeDescription.offset = layout.offset != (std::uint32_t)-1 ? layout.offset : nextOffset;
			vertexAttributeDescription.format = (vk::Format)layout.format.type();
			vertexAttributeDescriptions.push_back(vertexAttributeDescription);

			nextOffset = vertexAttributeDescription.offset + layout.format.size();
		}
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

	info.vertexInput.vertexBindingDescriptionCount = vertexBindingDescriptions.size();
	info.vertexInput.pVertexBindingDescriptions = vertexBindingDescriptions.data();
	info.vertexInput.vertexAttributeDescriptionCount = vertexAttributeDescriptions.size();
	info.vertexInput.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
}
