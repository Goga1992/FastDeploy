// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fastdeploy/backends/ort/ort_backend.h"
#include <memory>
#include "fastdeploy/backends/ort/ops/multiclass_nms.h"
#include "fastdeploy/backends/ort/utils.h"
#include "fastdeploy/utils/utils.h"
#ifdef ENABLE_PADDLE_FRONTEND
#include "paddle2onnx/converter.h"
#endif

namespace fastdeploy {

std::vector<OrtCustomOp*> OrtBackend::custom_operators_ =
    std::vector<OrtCustomOp*>();

ONNXTensorElementDataType GetOrtDtype(FDDataType fd_dtype) {
  if (fd_dtype == FDDataType::FP32) {
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
  } else if (fd_dtype == FDDataType::FP64) {
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE;
  } else if (fd_dtype == FDDataType::INT32) {
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
  } else if (fd_dtype == FDDataType::INT64) {
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
  }
  FDERROR << "Unrecognized fastdeply data type:" << FDDataTypeStr(fd_dtype)
          << "." << std::endl;
  return ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
}

FDDataType GetFdDtype(ONNXTensorElementDataType ort_dtype) {
  if (ort_dtype == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
    return FDDataType::FP32;
  } else if (ort_dtype == ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE) {
    return FDDataType::FP64;
  } else if (ort_dtype == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32) {
    return FDDataType::INT32;
  } else if (ort_dtype == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64) {
    return FDDataType::INT64;
  }
  FDERROR << "Unrecognized ort data type:" << ort_dtype << "." << std::endl;
  return FDDataType::FP32;
}

void OrtBackend::BuildOption(const OrtBackendOption& option) {
  option_ = option;
  if (option.graph_optimization_level >= 0) {
    session_options_.SetGraphOptimizationLevel(
        GraphOptimizationLevel(option.graph_optimization_level));
  }
  if (option.intra_op_num_threads >= 0) {
    session_options_.SetIntraOpNumThreads(option.intra_op_num_threads);
  }
  if (option.inter_op_num_threads >= 0) {
    session_options_.SetInterOpNumThreads(option.inter_op_num_threads);
  }
  if (option.execution_mode >= 0) {
    session_options_.SetExecutionMode(ExecutionMode(option.execution_mode));
  }
  if (option.use_gpu) {
    auto all_providers = Ort::GetAvailableProviders();
    bool support_cuda = false;
    std::string providers_msg = "";
    for (size_t i = 0; i < all_providers.size(); ++i) {
      providers_msg = providers_msg + all_providers[i] + ", ";
      if (all_providers[i] == "CUDAExecutionProvider") {
        support_cuda = true;
      }
    }
    if (!support_cuda) {
      FDWARNING << "Compiled fastdeploy with onnxruntime doesn't "
                   "support GPU, the available providers are "
                << providers_msg << "will fallback to CPUExecutionProvider."
                << std::endl;
      option_.use_gpu = false;
    } else {
      FDASSERT(option.gpu_id == 0, "Requires gpu_id == 0, but now gpu_id = " +
                                       std::to_string(option.gpu_id) + ".");
      OrtCUDAProviderOptions cuda_options;
      cuda_options.device_id = option.gpu_id;
      session_options_.AppendExecutionProvider_CUDA(cuda_options);
    }
  }
}

bool OrtBackend::InitFromPaddle(const std::string& model_file,
                                const std::string& params_file,
                                const OrtBackendOption& option, bool verbose) {
  if (initialized_) {
    FDERROR << "OrtBackend is already initlized, cannot initialize again."
            << std::endl;
    return false;
  }
#ifdef ENABLE_PADDLE_FRONTEND
  char* model_content_ptr;
  int model_content_size = 0;

  std::vector<paddle2onnx::CustomOp> custom_ops;
  for (auto& item : option.custom_op_info_) {
    paddle2onnx::CustomOp op;
    strcpy(op.op_name, item.first.c_str());
    strcpy(op.export_op_name, item.second.c_str());
    custom_ops.emplace_back(op);
  }
  if (!paddle2onnx::Export(model_file.c_str(), params_file.c_str(),
                           &model_content_ptr, &model_content_size, 11, true,
                           verbose, true, true, true, custom_ops.data(),
                           custom_ops.size())) {
    FDERROR << "Error occured while export PaddlePaddle to ONNX format."
            << std::endl;
    return false;
  }

  std::string onnx_model_proto(model_content_ptr,
                               model_content_ptr + model_content_size);
  delete[] model_content_ptr;
  model_content_ptr = nullptr;
  return InitFromOnnx(onnx_model_proto, option, true);
#else
  FDERROR << "Didn't compile with PaddlePaddle frontend, you can try to "
             "call `InitFromOnnx` instead."
          << std::endl;
#endif
  return false;
}

bool OrtBackend::InitFromOnnx(const std::string& model_file,
                              const OrtBackendOption& option,
                              bool from_memory_buffer) {
  if (initialized_) {
    FDERROR << "OrtBackend is already initlized, cannot initialize again."
            << std::endl;
    return false;
  }

  BuildOption(option);
  InitCustomOperators();
  if (from_memory_buffer) {
    session_ = {env_, model_file.data(), model_file.size(), session_options_};
  } else {
#ifdef _WIN32
    session_ = {env_,
                std::wstring(model_file.begin(), model_file.end()).c_str(),
                session_options_};
#else
    session_ = {env_, model_file.c_str(), session_options_};
#endif
  }
  binding_ = std::make_shared<Ort::IoBinding>(session_);

  Ort::MemoryInfo memory_info("Cpu", OrtDeviceAllocator, 0, OrtMemTypeDefault);
  Ort::Allocator allocator(session_, memory_info);
  size_t n_inputs = session_.GetInputCount();
  for (size_t i = 0; i < n_inputs; ++i) {
    auto input_name = session_.GetInputName(i, allocator);
    auto type_info = session_.GetInputTypeInfo(i);
    std::vector<int64_t> shape =
        type_info.GetTensorTypeAndShapeInfo().GetShape();
    ONNXTensorElementDataType data_type =
        type_info.GetTensorTypeAndShapeInfo().GetElementType();
    inputs_desc_.emplace_back(OrtValueInfo{input_name, shape, data_type});
    allocator.Free(input_name);
  }

  size_t n_outputs = session_.GetOutputCount();
  for (size_t i = 0; i < n_outputs; ++i) {
    auto output_name = session_.GetOutputName(i, allocator);
    auto type_info = session_.GetOutputTypeInfo(i);
    std::vector<int64_t> shape =
        type_info.GetTensorTypeAndShapeInfo().GetShape();
    ONNXTensorElementDataType data_type =
        type_info.GetTensorTypeAndShapeInfo().GetElementType();
    outputs_desc_.emplace_back(OrtValueInfo{output_name, shape, data_type});

    Ort::MemoryInfo out_memory_info("Cpu", OrtDeviceAllocator, 0,
                                    OrtMemTypeDefault);
    binding_->BindOutput(output_name, out_memory_info);

    allocator.Free(output_name);
  }
  initialized_ = true;
  return true;
}

void OrtBackend::CopyToCpu(const Ort::Value& value, FDTensor* tensor) {
  const auto info = value.GetTensorTypeAndShapeInfo();
  const auto data_type = info.GetElementType();
  size_t numel = info.GetElementCount();
  tensor->shape = info.GetShape();

  if (data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
    tensor->data.resize(numel * sizeof(float));
    memcpy(static_cast<void*>(tensor->Data()), value.GetTensorData<void*>(),
           numel * sizeof(float));
    tensor->dtype = FDDataType::FP32;
  } else if (data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32) {
    tensor->data.resize(numel * sizeof(int32_t));
    memcpy(static_cast<void*>(tensor->Data()), value.GetTensorData<void*>(),
           numel * sizeof(int32_t));
    tensor->dtype = FDDataType::INT32;
  } else if (data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64) {
    tensor->data.resize(numel * sizeof(int64_t));
    memcpy(static_cast<void*>(tensor->Data()), value.GetTensorData<void*>(),
           numel * sizeof(int64_t));
    tensor->dtype = FDDataType::INT64;
  } else if (data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE) {
    tensor->data.resize(numel * sizeof(double));
    memcpy(static_cast<void*>(tensor->Data()), value.GetTensorData<void*>(),
           numel * sizeof(double));
    tensor->dtype = FDDataType::FP64;
  } else {
    FDASSERT(false, "Unrecognized data type of " + std::to_string(data_type) +
                        " while calling OrtBackend::CopyToCpu().");
  }
}

bool OrtBackend::Infer(std::vector<FDTensor>& inputs,
                       std::vector<FDTensor>* outputs) {
  if (inputs.size() != inputs_desc_.size()) {
    FDERROR << "[OrtBackend] Size of the inputs(" << inputs.size()
            << ") should keep same with the inputs of this model("
            << inputs_desc_.size() << ")." << std::endl;
    return false;
  }

  // from FDTensor to Ort Inputs
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto ort_value = CreateOrtValue(inputs[i], option_.use_gpu);
    binding_->BindInput(inputs[i].name.c_str(), ort_value);
  }

  for (size_t i = 0; i < outputs_desc_.size(); ++i) {
    Ort::MemoryInfo memory_info("Cpu", OrtDeviceAllocator, 0,
                                OrtMemTypeDefault);
    binding_->BindOutput(outputs_desc_[i].name.c_str(), memory_info);
  }

  // Inference with inputs
  try {
    session_.Run({}, *(binding_.get()));
  } catch (const std::exception& e) {
    FDERROR << "Failed to Infer: " << e.what() << std::endl;
    return false;
  }

  // Copy result after inference
  std::vector<Ort::Value> ort_outputs = binding_->GetOutputValues();
  outputs->resize(ort_outputs.size());
  for (size_t i = 0; i < ort_outputs.size(); ++i) {
    (*outputs)[i].name = outputs_desc_[i].name;
    CopyToCpu(ort_outputs[i], &((*outputs)[i]));
  }
  return true;
}

TensorInfo OrtBackend::GetInputInfo(int index) {
  FDASSERT(index < NumInputs(), "The index:" + std::to_string(index) +
                                    " should less than the number of inputs:" +
                                    std::to_string(NumInputs()) + ".");
  TensorInfo info;
  info.name = inputs_desc_[index].name;
  info.shape.assign(inputs_desc_[index].shape.begin(),
                    inputs_desc_[index].shape.end());
  info.dtype = GetFdDtype(inputs_desc_[index].dtype);
  return info;
}

TensorInfo OrtBackend::GetOutputInfo(int index) {
  FDASSERT(index < NumOutputs(),
           "The index:" + std::to_string(index) +
               " should less than the number of outputs:" +
               std::to_string(NumOutputs()) + ".");
  TensorInfo info;
  info.name = outputs_desc_[index].name;
  info.shape.assign(outputs_desc_[index].shape.begin(),
                    outputs_desc_[index].shape.end());
  info.dtype = GetFdDtype(outputs_desc_[index].dtype);
  return info;
}

void OrtBackend::InitCustomOperators() {
#ifndef NON_64_PLATFORM
  if (custom_operators_.size() == 0) {
    MultiClassNmsOp* custom_op = new MultiClassNmsOp{};
    custom_operators_.push_back(custom_op);
  }
  for (size_t i = 0; i < custom_operators_.size(); ++i) {
    custom_op_domain_.Add(custom_operators_[i]);
  }
  session_options_.Add(custom_op_domain_);
#endif
}

}  // namespace fastdeploy