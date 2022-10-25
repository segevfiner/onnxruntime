// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// import {AttributeWithCacheKey, createAttributeWithCacheKey} from '../../../attribute-with-cache-key';
// import {InferenceHandler} from '../../../backend';
// import {Graph} from '../../../graph';
// import {OperatorAsyncImplementation, OperatorInitialization} from '../../../operators';
// import {Tensor} from '../../../tensor';
// import {PoolConvUtil} from '../../../util';
// import {WebGpuInferenceHandler} from '../inference-handler';

// import {createGroupedConvProgramInfoLoader} from './conv-grouped';
// // import {createDotProductProgramInfoLoader} from './dot-product';
// import {InternalActivationAttributes, parseInternalActivationAttributes} from './fuse-utils';

// // import {createIm2ColProgramInfoLoader} from './im2col';
// // import {createMatmulProgramInfoLoader} from './matmul';


// export const calculateOutputShape =
//     (inputShape: readonly number[], kernelShape: readonly number[], dilations: readonly number[],
//      adjustPads: readonly number[], strides: readonly number[]): number[] => {
//       const batchSize = inputShape[0];
//       const inputSpatialShape = inputShape.slice(2);
//       const spatialRank = inputSpatialShape.length;
//       const outChannels = kernelShape[0];
//       const kernelSpatialShape = kernelShape.slice(2);
//       const dilatedKernelShape = kernelSpatialShape.map((v, i) => v + (v - 1) * (dilations[i] - 1));
//       const inputSpatialShapeWithPad = inputSpatialShape.map((v, i) => v + adjustPads[i] + adjustPads[i +
//       spatialRank]); const outputSpatialShape =
//           inputSpatialShapeWithPad.map((v, i) => Math.floor((v - dilatedKernelShape[i] + strides[i]) / strides[i]));
//       const outputShape = [batchSize, outChannels].concat(...outputSpatialShape);
//       return outputShape;
//     };

// export interface ConvAttributes extends InternalActivationAttributes, AttributeWithCacheKey {
//   readonly autoPad: string;
//   readonly dilations: readonly number[];
//   readonly group: number;
//   readonly kernelShape: readonly number[];
//   readonly pads: readonly number[];
//   readonly strides: readonly number[];
// }

// export const conv: OperatorAsyncImplementation<ConvAttributes> =
//     async(inferenceHandler: InferenceHandler, inputs: Tensor[], attributes: ConvAttributes): Promise<Tensor[]> => {
//   validateInputs(inputs, attributes);  // currently will fail if not conv2D
//   return conv2d(inferenceHandler, inputs, attributes);
// };

// const conv2d: OperatorAsyncImplementation<ConvAttributes> = async(
//     inferenceHandler: WebGpuInferenceHandler, inputs: Tensor[], attributes: ConvAttributes): Promise<Tensor[]> => {
//   const adjustedAttributes = getAdjustedConvAttributes(attributes, inputs);
//   //  const isPointwise = adjustedAttributes.kernelShape[0] === 1 && adjustedAttributes.kernelShape[1] === 1;
//   //  if (adjustedAttributes.group > 1) {
//   return inferenceHandler.run(createGroupedConvProgramInfoLoader(inferenceHandler, inputs, adjustedAttributes),
//   inputs);
//   //  } else if (isPointwise) {
//   //    return conv2DPointwise(inferenceHandler, inputs, adjustedAttributes);
//   //  } else {
//   //    return conv2D(inferenceHandler, inputs, adjustedAttributes);
//   //  }
// };

// const getAdjustedConvAttributes = <T extends ConvAttributes>(attributes: T, inputs: Tensor[]): T => {
//   const kernelShape = attributes.kernelShape.slice();
//   // if kernelShape is not specified in the attributes of this op, infer it from the weight tensor dims
//   if (attributes.kernelShape.length === 0) {
//     for (let i = 2; i < inputs[1].dims.length; ++i) {
//       kernelShape.push(inputs[1].dims[i]);
//     }
//   }
//   const pads = attributes.pads.slice();
//   PoolConvUtil.adjustPadsBasedOnAutoPad(
//       inputs[0].dims, attributes.strides, attributes.dilations, kernelShape, pads, attributes.autoPad);

//   // always return a new object so does not modify the original attributes
//   const newAttributes: T = Object.assign({}, attributes);
//   Object.assign(newAttributes, {kernelShape, pads, cacheKey: attributes.cacheKey});
//   return newAttributes;
// };

// export const parseConvAttributes: OperatorInitialization<ConvAttributes> = (node: Graph.Node): ConvAttributes => {
//   const attributes = node.attributes;
//   const activationAttributes = parseInternalActivationAttributes(attributes);
//   // TODO : Make this generic enough to compute default attributes for multi-dimensional conv
//   const autoPad = attributes.getString('auto_pad', 'NOTSET');
//   const dilations = attributes.getInts('dilations', [1, 1]);
//   const group = attributes.getInt('group', 1);
//   const kernelShape = attributes.getInts('kernel_shape', []);
//   const pads = attributes.getInts('pads', [0, 0, 0, 0]);
//   const strides = attributes.getInts('strides', [1, 1]);

//   return createAttributeWithCacheKey({autoPad, dilations, group, kernelShape, pads, strides,
//   ...activationAttributes});
// };

// const validateInputs = (inputs: Tensor[], attributes: ConvAttributes): void => {
//   // Refer to the below link for all input checks
//   // https://github.com/onnx/onnx/blob/master/docs/Operators.md#Conv
//   if (!inputs || (inputs.length !== 2 && inputs.length !== 3)) {
//     throw new Error('Conv requires 2 or 3 inputs');
//   }

//   // TODO : Need to add support for multi-dimensional conv
//   if (inputs[0].dims.length !== 4 || inputs[1].dims.length !== 4) {
//     throw new Error('currently only support 2-dimensional conv');
//   }

//   // FILTER_IN_CHANNEL should be equal to DATA_CHANNEL
//   const dataChannel = inputs[0].dims[1];
//   const filterInChannel = inputs[1].dims[1] * attributes.group;
//   if (dataChannel !== filterInChannel) {
//     throw new Error('FILTER_IN_CHANNEL should be equal to DATA_CHANNEL');
//   }

//   // if bias is provided it should be 1D and the number of elements should be equal to the number of feature maps
//   if (inputs.length === 3 && (inputs[2].dims.length !== 1 || inputs[1].dims[0] !== inputs[2].dims[0])) {
//     throw new Error('invalid bias');
//   }

//   const spatialRank = inputs[0].dims.length - 2;
//   // wrong dilations dimension
//   if (attributes.dilations.length !== spatialRank) {
//     throw new Error(`dilations should be ${spatialRank}D`);
//   }

//   // Wrong strides dimension
//   if (attributes.strides.length !== spatialRank) {
//     throw new Error(`strides should be ${spatialRank}D`);
//   }

//   // Wrong pads dimension
//   if (attributes.pads.length !== spatialRank * 2) {
//     throw new Error(`pads should be ${spatialRank * 2}D`);
//   }

//   // if kernelShape is specified, it's data length must be 2 less than dims length of the weights tensor
//   // (the first 2 dims are batch_size and channels)
//   if (attributes.kernelShape.length !== 0 && attributes.kernelShape.length !== inputs[1].dims.length - 2) {
//     throw new Error('invalid kernel shape');
//   }

//   // TODO : Need to add support for float64
//   if (inputs[0].type !== 'float32' || inputs[1].type !== 'float32') {
//     throw new Error('Conv input(X,W) should be float tensor');
//   }

//   if (inputs.length === 3 && inputs[2].type !== 'float32') {
//     throw new Error('Conv input(bias) should be float tensor');
//   }
// };
