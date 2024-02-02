// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/coreml/builders/impl/base_op_builder.h"
#include "core/providers/coreml/builders/impl/builder_utils.h"
#include "core/providers/coreml/builders/model_builder.h"
#include "core/providers/coreml/builders/op_builder_factory.h"
#include "core/providers/shared/utils/utils.h"

namespace onnxruntime {
namespace coreml {

class ClipOpBuilder : public BaseOpBuilder {
  void AddInitializersToSkip(ModelBuilder& model_builder, const Node& node) const override;

  Status AddToModelBuilderImpl(ModelBuilder& model_builder, const Node& node,
                               const logging::Logger& logger) const override;

  bool IsOpSupportedImpl(const Node& node, const OpBuilderInputParams& input_params,
                         const logging::Logger& logger) const override;

  bool SupportsMLProgram() const override { return true; }
};

void ClipOpBuilder::AddInitializersToSkip(ModelBuilder& model_builder, const Node& node) const {
  if (model_builder.CreateMLProgram()) {
    // we add the initializers as 'const' operations via ModelBuilder::RegisterInitializers
    return;
  }

  // Both min and max values will be injected into the layer, no need to add to the model
  if (node.SinceVersion() >= 11) {
    if (node.InputDefs().size() > 1)
      model_builder.AddInitializerToSkip(node.InputDefs()[1]->Name());

    if (node.InputDefs().size() > 2)
      model_builder.AddInitializerToSkip(node.InputDefs()[2]->Name());
  }
}

Status ClipOpBuilder::AddToModelBuilderImpl(ModelBuilder& model_builder,
                                            const Node& node,
                                            const logging::Logger& logger) const {
  const auto& node_name = node.Name();
  const auto& input_name = node.InputDefs()[0]->Name();
  const auto& output = *node.OutputDefs()[0];
  const auto& output_name = output.Name();
  float min, max;
  ORT_RETURN_IF_NOT(GetClipMinMax(model_builder.GetGraphViewer(), node, min, max, logger), "GetClipMinMax failed");

  bool has_min = min != std::numeric_limits<float>::lowest();
  bool has_max = max != std::numeric_limits<float>::max();

#if defined(COREML_ENABLE_MLPROGRAM)
  if (model_builder.CreateMLProgram()) {
    using namespace CoreML::Specification::MILSpec;

    std::unique_ptr<Operation> op;
    if (!has_min && !has_max) {
      // Clip without min/max is an identity node.
      op = model_builder.CreateOperation(node, "identity");
      Operation& identity_op = *op;
      AddOperationInput(identity_op, "x", input_name);
    } else {
      if (has_min && has_max && min == 0.f && max == 6.f) {
        op = model_builder.CreateOperation(node, "relu6");
        Operation& relu6_op = *op;
        AddOperationInput(relu6_op, "x", input_name);
      } else if (has_min && min == 0.f && !has_max) {
        op = model_builder.CreateOperation(node, "relu");
        Operation& relu_op = *op;
        AddOperationInput(relu_op, "x", input_name);
      } else {
        op = model_builder.CreateOperation(node, "clip");
        Operation& clip_op = *op;
        AddOperationInput(clip_op, "x", input_name);
        model_builder.AddValueAsConstantOperationInput(clip_op, "alpha", min);
        model_builder.AddValueAsConstantOperationInput(clip_op, "beta", max);
      }
    }

    AddOperationOutput(*op, output);
    model_builder.AddOperation(std::move(op));
  } else
#endif  // defined(COREML_ENABLE_MLPROGRAM)
  {
    // TODO: CoreML has a Clip layer for NeuralNetwork. Added in CoreML 4. We could potentially use that if available
    // to simplify.
    // https://apple.github.io/coremltools/mlmodel/Format/NeuralNetwork.html#cliplayerparams

    if (!has_min && !has_max) {
      // Clip without min/max is an identity node
      // In CoreML we don't have identity, use ActivationLinear instead
      std::unique_ptr<COREML_SPEC::NeuralNetworkLayer> layer = model_builder.CreateNNLayer(node);
      layer->mutable_activation()->mutable_linear()->set_alpha(1.0f);
      *layer->mutable_input()->Add() = input_name;
      *layer->mutable_output()->Add() = output_name;

      model_builder.AddLayer(std::move(layer));
    } else {
      // The implementation of clip(min, max) is done by
      // 1. Clipping at min -> max(input, min) is handled by
      //    min_output = threshold(input, min)
      // 2. Clipping at max -> min(min_output, max) is handled by
      //    output = -1 * (threshold(-min_output, -max))

      // Now we have at least one or min or max is not default value
      // Clipping at max will need take the output of clipping at min, or the node input, if min value is default
      // If max value is default, the output of clipping at min will be the output of the node
      std::string min_output_name = output_name;
      if (has_max) {
        min_output_name = has_min
                              ? model_builder.GetUniqueName(node_name + "min_output")
                              : input_name;
      }

      // Handle clipping at min first
      if (has_min) {
        std::unique_ptr<COREML_SPEC::NeuralNetworkLayer> min_layer = model_builder.CreateNNLayer(node, "_Clip_min");
        if (min == 0.0f) {  // If min is 0. then this min will be handled by relu
          min_layer->mutable_activation()->mutable_relu();
        } else {  // otherwise, min will be handled by unary->threshold
          min_layer->mutable_unary()->set_alpha(min);
          min_layer->mutable_unary()->set_type(COREML_SPEC::UnaryFunctionLayerParams::THRESHOLD);
        }

        *min_layer->mutable_input()->Add() = input_name;
        *min_layer->mutable_output()->Add() = min_output_name;
        model_builder.AddLayer(std::move(min_layer));
      }

      // Clipping at max is handled by -1 * (threshold (-min_output, -max))
      if (has_max) {
        const auto threshold_output_name = model_builder.GetUniqueName(MakeString(node_name, "threshold_output"));
        {  // Add threshold layer, which is actually max( -1 * min_output, -max)
          auto threshold_layer = model_builder.CreateNNLayer(node, "_Clip_max_threshold");
          threshold_layer->mutable_unary()->set_alpha(-max);
          threshold_layer->mutable_unary()->set_scale(-1.0f);
          threshold_layer->mutable_unary()->set_type(COREML_SPEC::UnaryFunctionLayerParams::THRESHOLD);
          *threshold_layer->mutable_input()->Add() = min_output_name;
          *threshold_layer->mutable_output()->Add() = threshold_output_name;
          model_builder.AddLayer(std::move(threshold_layer));
        }
        {  // Add linear activation layer -1 * threshold_output
          auto linear_layer = model_builder.CreateNNLayer(node, "_Clip_max_linear");
          linear_layer->mutable_activation()->mutable_linear()->set_alpha(-1.0f);
          *linear_layer->mutable_input()->Add() = threshold_output_name;
          *linear_layer->mutable_output()->Add() = output_name;
          model_builder.AddLayer(std::move(linear_layer));
        }
      }
    }
  }

  return Status::OK();
}

bool ClipOpBuilder::IsOpSupportedImpl(const Node& node, const OpBuilderInputParams& input_params,
                                      const logging::Logger& logger) const {
  float min, max;
  return GetClipMinMax(input_params.graph_viewer, node, min, max, logger);
}

void CreateClipOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations) {
  op_registrations.builders.push_back(std::make_unique<ClipOpBuilder>());
  op_registrations.op_builder_map.emplace(op_type, op_registrations.builders.back().get());
}

}  // namespace coreml
}  // namespace onnxruntime
