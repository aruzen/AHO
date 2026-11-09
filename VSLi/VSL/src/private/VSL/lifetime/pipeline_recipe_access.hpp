#pragma once
#include <VSL/lifetime/pipeline_recipe.hpp>

namespace VSL_NAMESPACE::lifetime::detail {
    struct PipelineRecipeAccess {
        static const impl::PipelineRecipe& Read(const PipelineLayoutAccessor& value);
        static const impl::PipelineRecipe& Read(const PipelineLayoutAccessor&& value) = delete;
        static const impl::PipelineRecipe& Read(const PipelineLayoutEditAccessor& value) { return Read(value._value); }
        static const impl::PipelineRecipe& Read(const PipelineLayoutEditAccessor&& value) = delete;
        static const impl::PipelineRecipe& Read(const FrozenPipelineLayout& value) { return Read(value._value); }
        static const impl::PipelineRecipe& Read(const FrozenPipelineLayout&& value) = delete;
        static const impl::PipelineRecipe& Read(const PipelineRecipeSnapshot& value);
        static const impl::PipelineRecipe& Read(const PipelineRecipeSnapshot&& value) = delete;
        static impl::PipelineRecipe& Edit(const PipelineLayoutEditAccessor& value);
        static impl::PipelineRecipe& Edit(const PipelineLayoutEditAccessor&& value) = delete;
    };
} // namespace VSL_NAMESPACE::lifetime::detail
