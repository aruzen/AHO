#pragma once
#include "../../define.h"
#include "../pv.h"
#include "../pipeline_layout.h"
#include "../../format.h"

namespace VSL_NAMESPACE::pipeline_layout {
	struct VertexInputLayoutDefinition {
		data_format::___Format format;
		std::uint32_t location = (std::uint32_t)-1, offset = (std::uint32_t)-1;
	};

	struct VertexInputShapeDefinition {
		std::vector<VertexInputLayoutDefinition> layouts;

		std::uint32_t binding = (std::uint32_t)-1;
		std::uint32_t size = (std::uint32_t)-1;

		enum class UpdateTiming {
			NextVertex = 0, NextInstance = 1
		} updateTiming = UpdateTiming::NextVertex;

	};

	struct VertexInput {
		VertexInput();
		VertexInput(std::initializer_list<VertexInputShapeDefinition> definitions);
		template<concepts::is_static_convertible_graphic_type... Args>
		VertexInput();
		template<concepts::is_convertible_graphic_type... Args>
		VertexInput(Args... args);

		VertexInput add(data_format::___Format format);
		VertexInput add(std::initializer_list<data_format::___Format> format);
		VertexInput add_shape(std::initializer_list<data_format::___Format> format);
		template<concepts::is_static_convertible_graphic_type T>
		VertexInput add();
		template<concepts::is_convertible_graphic_type T>
		VertexInput add(T t);
		template<concepts::is_static_convertible_graphic_type... Args>
		VertexInput add();
		template<concepts::is_convertible_graphic_type... Args>
		VertexInput add(Args... args);

		VertexInput add(VertexInputShapeDefinition definition);
		VertexInput add(std::initializer_list<VertexInputShapeDefinition> definitions);

		size_t requirements_size();

		std::vector<VertexInputShapeDefinition> definitions;

		void injection(VSL_NAMESPACE::PipelineLayoutAccessor pl);
	};

	template<concepts::is_static_convertible_graphic_type ...Args>
	VSL_NAMESPACE::pipeline_layout::VertexInput::VertexInput() {
		add<Args...>();
		return *this;
	}

	template<concepts::is_convertible_graphic_type ...Args>
	VSL_NAMESPACE::pipeline_layout::VertexInput::VertexInput(Args ...args) {
		add(args...);
	}

	template<concepts::is_static_convertible_graphic_type T>
	VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add()
	{
		add(data_format::convert_graphic_type<T>());
		return *this;
	}

	template<concepts::is_convertible_graphic_type T>
	VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add(T t)
	{
		add(data_format::convert_graphic_type(t));
		return *this;
	}

	template<concepts::is_static_convertible_graphic_type... Arg>
	VertexInput VSL_NAMESPACE::pipeline_layout::VertexInput::add()
	{
		add({ data_format::convert_graphic_type<Arg>()... });
		return *this;
	}

	template<concepts::is_convertible_graphic_type ...Args>
	VertexInput VertexInput::add(Args ...args)
	{
		add({ data_format::convert_graphic_type(args)... });
		return *this;
	}
}
