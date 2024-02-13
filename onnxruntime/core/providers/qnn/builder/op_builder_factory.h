// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "op_builder.h"

namespace onnxruntime {
namespace qnn {

class OpBuilderRegistrations {
 public:
  OpBuilderRegistrations();
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(OpBuilderRegistrations);
  const IOpBuilder* GetOpBuilderByOnnxOpType(const std::string& onnx_op_type) const {
    auto pos = op_builder_map_.find(onnx_op_type);
    if (pos != op_builder_map_.end()) {
      return pos->second;
    }

    return nullptr;
  }

  void AddOpBuilder(const std::string& onnx_op_type, std::unique_ptr<IOpBuilder> builder) {
    if (GetOpBuilderByOnnxOpType(onnx_op_type) != nullptr) {  // already have this Op added
      return;
    }

    auto builder_type = builder->GetOpBuilderType();
    auto pos_in_builder_type_map = builder_type_builder_map_.find(builder_type);
    if (pos_in_builder_type_map != builder_type_builder_map_.end()) {
      // already have this builder type, re-use it for this onnx_op_type
      op_builder_map_.emplace(onnx_op_type, pos_in_builder_type_map->second);
    } else {
      // New Op builder, add to vector and all the maps
      builders_.push_back(std::move(builder));
      op_builder_map_.emplace(onnx_op_type, builders_.back().get());
      builder_type_builder_map_.emplace(builder_type, builders_.back().get());
    }
  }

 private:
  std::vector<std::unique_ptr<IOpBuilder>> builders_;
  // <onnx_op_type, IOpBuilder*>
  std::unordered_map<std::string, const IOpBuilder*> op_builder_map_;
  // <Op_builder_type, IOpBuilder*>
  std::unordered_map<std::string, const IOpBuilder*> builder_type_builder_map_;
};
const IOpBuilder* GetOpBuilder(const std::string& onnx_op_type);

void CreateSimpleOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateSoftmaxOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateCastOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateConvOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreatePoolOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateReshapeOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateGemmOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateArgMaxMinOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateGatherOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateClipOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateSliceOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateSplitOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateResizeOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateTopKOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateTileOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateInstanceNormOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateReduceOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateBatchNormOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateLayerNormOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateLRNOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateTransposeOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreatePadOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

void CreateExpandOpBuilder(const std::string& op_type, OpBuilderRegistrations& op_registrations);

struct HandleConvertResult {
  Status status;                // Indicates an unexpected error. Check if q_node_unit != nullptr to determine
                                // whether a DQ -> Q sequence was successfully merged into a Convert.
  const NodeUnit* q_node_unit;  // Non-null if successfully merged DQ -> Q sequence.
                                // Set to nullptr if this node unit could not be merged into a Convert.
};

/**
 * Tries to merge a DQ -> Q sequence into a QNN Convert operator. The DQ -> Q must be converting from
 * one quantization type (e.g., uint8_t) to another (e.g., uint16_t).
 *
 * \param qnn_model_wrapper The QNN model that is being built.
 * \param maybe_dq_node_unit The node unit that could potentially start the DQ -> Q sequence.
 * \param logger The logger.
 * \param do_op_validation True if should call QNN operator validation APIs.
 * \return An qnn::HandleConvertResult object that indicates success/failure and provides a pointer
 *         to the Q node unit that was successfully merged with the provided DQ node unit.
 */
HandleConvertResult TryHandleConvertSequence(QnnModelWrapper& qnn_model_wrapper,
                                             const NodeUnit& maybe_dq_node_unit,
                                             const std::unordered_map<const Node*, const NodeUnit*>& node_unit_map,
                                             const GraphViewer& graph_viewer,
                                             const logging::Logger& logger,
                                             bool do_op_validation);
}  // namespace qnn
}  // namespace onnxruntime
