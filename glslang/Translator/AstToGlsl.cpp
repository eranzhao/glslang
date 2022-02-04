
#include "AstToGlsl.h"

// Glslang includes
#include "../MachineIndependent/localintermediate.h"
#include "../MachineIndependent/SymbolTable.h"
#include "../Include/Common.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <list>
#include <map>
#include <stack>
#include <string>
#include <vector>

namespace glslang {

bool getBasicTypeName(TBasicType type, TString &name) {
  switch (type) {
    case EbtFloat:    name = "float32_t";   return true;
    case EbtDouble:   name = "float64_t";   return true;
    case EbtFloat16:  name = "float16_t";   return true;
    case EbtInt8:     name = "int8_t";      return true;
    case EbtUint8:    name = "uint8_t";     return true;
    case EbtInt16:    name = "int16_t";     return true;
    case EbtUint16:   name = "uint16_t";    return true;
    case EbtInt:      name = "int32_t";     return true;
    case EbtUint:     name = "uint32_t";    return true;
    case EbtInt64:    name = "int64_t";     return true;
    case EbtUint64:   name = "uint64_t";    return true;
    case EbtBool:     name = "bool";        return true;
    default:                                return false;
  }
  return false;
}

bool getVectorTypeName(TBasicType type, int num, TString &name) {
  std::string numStr = std::to_string(num);
  switch (type) {
    case EbtFloat:    name = "f32vec" + numStr;  return true;
    case EbtDouble:   name = "f64vec" + numStr;  return true;
    case EbtFloat16:  name = "f16vec" + numStr;  return true;
    case EbtInt8:     name = "i8vec" + numStr;   return true;
    case EbtUint8:    name = "u8vec" + numStr;   return true;
    case EbtInt16:    name = "i16vec" + numStr;  return true;
    case EbtUint16:   name = "u16vec" + numStr;  return true;
    case EbtInt:      name = "i32vec" + numStr;  return true;
    case EbtUint:     name = "u32vec" + numStr;  return true;
    case EbtInt64:    name = "i64vec" + numStr;  return true;
    case EbtUint64:   name = "u64vec" + numStr;  return true;
    case EbtBool:     name = "bvec" + numStr;    return true;
    default:                                     return false;
  }
  return false;
}

bool getMatrixTypeName(TBasicType type, int col, int row, TString &name) {
  std::string numStr = std::to_string(col) + "x" + std::to_string(row);
  switch (type) {
    case EbtFloat:    name = "f32mat" + numStr;   return true;
    case EbtDouble:   name = "f64mat" + numStr;   return true;
    case EbtFloat16:  name = "f16mat" + numStr;   return true;
    default:                                      return false;
  }
  return false;
}

// Get return type name in glsl.
bool getReturnTypeName(const TType &type, TString &name) {
  
  // Cannot return array in HLSL, disable it.
  // if(type.isArray())
  //   return false;
  
  // Must be sized array.
  if (type.isArray() && type.isUnsizedArray()) {
    return false;
  }
  
  TBasicType basicType = type.getBasicType();

  // Cannot return a sampler in Vulkan GLSL.
  if (basicType == EbtSampler || basicType == EbtBlock || basicType == EbtAccStruct ||
      basicType == EbtRayQuery || basicType == EbtString) {
    return false;
  }
  
#ifndef GLSLANG_WEB
  if (basicType == EbtSpirvType) {
    return false;
  }
#endif
  
  if (basicType == EbtVoid) {
    name = "void";
    return true;
  }
  
  if (basicType == EbtStruct) {
    name = type.getTypeName();
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  if ( basicType == EbtFloat  || basicType == EbtDouble || basicType == EbtFloat16 ||
       basicType == EbtInt8   || basicType == EbtUint8  || basicType == EbtInt16   ||
       basicType == EbtUint16 || basicType == EbtInt    || basicType == EbtUint    ||
       basicType == EbtInt64  || basicType == EbtUint64 || basicType == EbtBool) {
    
    
    bool validBasicType = false;
    
    if (type.isVector()) {
      validBasicType = getVectorTypeName(basicType, type.getVectorSize(), name);
    }
    else if (type.isMatrix()) {
      validBasicType = getMatrixTypeName(basicType, type.getMatrixCols(), type.getMatrixRows(), name);
    }
    else {
      validBasicType = getBasicTypeName(basicType, name);
    }
    
    // Apply precision qualifier.
    switch (type.getQualifier().precision) {
      case EpqLow:    name = "lowp " + name;    break;
      case EpqMedium: name = "mediump " + name; break;
      case EpqHigh:   name = "highp " + name;   break;
      default:                                  break;
    }
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  return false;
}

// Check storage qualifier is support or not.
bool checkStorageQualifier(TStorageQualifier s) {
  switch (s) {
    case EvqPayload:
    case EvqPayloadIn:
    case EvqHitAttr:
    case EvqCallableData:
    case EvqCallableDataIn:
#ifndef GLSLANG_WEB
    case EvqSpirvStorageClass:
#endif
      return false;
    default:
      return true;
  }
  return true;
}

bool getLinkerObject(const TType &type, TString basename, TString &name) {
  
  TBasicType basicType = type.getBasicType();
  
  // Void cannot be type of function parameters in Vulkan GLSL.
  if (basicType == EbtVoid) {
    return false;
  }
  
  // Cannot return a sampler in Vulkan GLSL.
  if (basicType == EbtAccStruct || basicType == EbtRayQuery || basicType == EbtString) {
    return false;
  }
  
#ifndef GLSLANG_WEB
  if (basicType == EbtSpirvType) {
    return false;
  }
#endif
  
  // Translator always translate simple code.
  if (!checkStorageQualifier(type.getQualifier().storage)) {
    printf("ERROR: Translator do not support qualifier: Payload, PayloadIn, HitAttr, CallableData, CallableDataIn, SpirvStorageClass. Use platform implementation instead.\n");
    return false;
  }
  
  // Always translate simple code.
  if (type.getQualifier().semanticName != nullptr) {
    printf("ERROR: Translator do not support semantics, use platform implementation instead.\n");
    return false;
  }
  
  if (basicType == EbtSampler) {
   
    
    return true;
  }
  
  if (basicType == EbtStruct || basicType == EbtBlock) {
    name = type.getTypeName();
    
    // Apply qualifiers.
    switch (type.getQualifier().storage) {
      case EvqConst:              name = "const " + name;               break;
      case EvqVaryingIn:          name = "in " + name;                  break;
      case EvqVaryingOut:         name = "out " + name;                 break;
      case EvqUniform:            name = "uniform " + name;             break;
      case EvqBuffer:             name = "buffer " + name;              break;
      case EvqShared:             name = "shared " + name;              break;
      default:                                                          break;
    }
    
    // Apply layouts.
    
    name += " " + basename;
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  if ( basicType == EbtFloat  || basicType == EbtDouble || basicType == EbtFloat16 ||
       basicType == EbtInt8   || basicType == EbtUint8  || basicType == EbtInt16   ||
       basicType == EbtUint16 || basicType == EbtInt    || basicType == EbtUint    ||
       basicType == EbtInt64  || basicType == EbtUint64 || basicType == EbtBool) {
    
    bool validBasicType = false;
    
    if (type.isVector()) {
      validBasicType = getVectorTypeName(basicType, type.getVectorSize(), name);
    }
    else if (type.isMatrix()) {
      validBasicType = getMatrixTypeName(basicType, type.getMatrixCols(), type.getMatrixRows(), name);
    }
    else {
      validBasicType = getBasicTypeName(basicType, name);
    }
    
    // Apply precision qualifier.
    switch (type.getQualifier().precision) {
      case EpqLow:    name = "lowp " + name;    break;
      case EpqMedium: name = "mediump " + name; break;
      case EpqHigh:   name = "highp " + name;   break;
      default:                                  break;
    }
    
    switch (type.getQualifier().storage) {
      case EvqConst:         name = "const " + name;   break;
      case EvqVaryingIn:     name = "in " + name;      break;
      case EvqVaryingOut:    name = "out " + name;     break;
      case EvqUniform:       name = "uniform " + name; break;
      default:                                         break;
    }
    
    // Apply layouts.
    
    
    name += " " + basename;
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  return false;
}

bool getParameterTypeName(const TType &type, TString basename, TString &name) {
  
  TBasicType basicType = type.getBasicType();

  // Always translate simple code.
  if (type.getQualifier().semanticName != nullptr) {
    printf("ERROR: Translator do not support semantics, use platform implementation instead.\n");
    return false;
  }
  
  // Void cannot be type of function parameters in Vulkan GLSL.
  if (basicType == EbtVoid) {
    return false;
  }
  
  // Cannot return a sampler in Vulkan GLSL.
  if (basicType == EbtSampler || basicType == EbtBlock || basicType == EbtAccStruct ||
      basicType == EbtRayQuery || basicType == EbtString) {
    return false;
  }
  
#ifndef GLSLANG_WEB
  if (basicType == EbtSpirvType) {
    return false;
  }
#endif
  
  if (basicType == EbtStruct) {
    name = type.getTypeName();
    
    // Apply in, out, inout.
    switch (type.getQualifier().storage) {
      case EvqIn:            name = "in " + name;      break;
      case EvqOut:           name = "out " + name;     break;
      case EvqInOut:         name = "inout " + name;   break;
      case EvqConstReadOnly: name = "const " + name;   break;
      default:                                         break;
    }
    
    name += " " + basename;
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  if ( basicType == EbtFloat  || basicType == EbtDouble || basicType == EbtFloat16 ||
       basicType == EbtInt8   || basicType == EbtUint8  || basicType == EbtInt16   ||
       basicType == EbtUint16 || basicType == EbtInt    || basicType == EbtUint    ||
       basicType == EbtInt64  || basicType == EbtUint64 || basicType == EbtBool) {
    
    
    bool validBasicType = false;
    
    if (type.isVector()) {
      validBasicType = getVectorTypeName(basicType, type.getVectorSize(), name);
    }
    else if (type.isMatrix()) {
      validBasicType = getMatrixTypeName(basicType, type.getMatrixCols(), type.getMatrixRows(), name);
    }
    else {
      validBasicType = getBasicTypeName(basicType, name);
    }
    
    // Apply precision qualifier.
    switch (type.getQualifier().precision) {
      case EpqLow:    name = "lowp " + name;    break;
      case EpqMedium: name = "mediump " + name; break;
      case EpqHigh:   name = "highp " + name;   break;
      default:                                  break;
    }
    
    // Apply in, out, inout.
    switch (type.getQualifier().storage) {
      case EvqIn:            name = "in " + name;      break;
      case EvqOut:           name = "out " + name;     break;
      case EvqInOut:         name = "inout " + name;   break;
      case EvqConstReadOnly: name = "const " + name;   break;
      default:                                         break;
    }
    
    name += " " + basename;
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  return false;
}

bool getTempTypeName(const TType &type, TString basename, TString &name) {
  
  TBasicType basicType = type.getBasicType();

  // Void cannot be type of function parameters in Vulkan GLSL.
  if (basicType == EbtVoid) {
    return false;
  }
  
  // Cannot return a sampler in Vulkan GLSL.
  if (basicType == EbtSampler || basicType == EbtBlock || basicType == EbtAccStruct ||
      basicType == EbtRayQuery || basicType == EbtString) {
    return false;
  }
  
#ifndef GLSLANG_WEB
  if (basicType == EbtSpirvType) {
    return false;
  }
#endif
  
  if (basicType == EbtStruct) {
    name = type.getTypeName();
    name += " " + basename;
    return true;
  }
  
  if ( basicType == EbtFloat  || basicType == EbtDouble || basicType == EbtFloat16 ||
       basicType == EbtInt8   || basicType == EbtUint8  || basicType == EbtInt16   ||
       basicType == EbtUint16 || basicType == EbtInt    || basicType == EbtUint    ||
       basicType == EbtInt64  || basicType == EbtUint64 || basicType == EbtBool) {
    
    
    bool validBasicType = false;
    
    if (type.isVector()) {
      validBasicType = getVectorTypeName(basicType, type.getVectorSize(), name);
    }
    else if (type.isMatrix()) {
      validBasicType = getMatrixTypeName(basicType, type.getMatrixCols(), type.getMatrixRows(), name);
    }
    else {
      validBasicType = getBasicTypeName(basicType, name);
    }
    
    // Apply precision qualifier.
    switch (type.getQualifier().precision) {
      case EpqLow:    name = "lowp " + name;    break;
      case EpqMedium: name = "mediump " + name; break;
      case EpqHigh:   name = "highp " + name;   break;
      default:                                  break;
    }
    
    // Apply in, out, inout.
    switch (type.getQualifier().storage) {
      case EvqIn:            name = "in " + name;      break;
      case EvqOut:           name = "out " + name;     break;
      case EvqInOut:         name = "inout " + name;   break;
      case EvqConstReadOnly: name = "const " + name;   break;
      default:                                         break;
    }
    
    name += " " + basename;
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  return false;
}

bool getConstantTypeName(const TType &type, TString &name) {
  
  // Cannot return array in HLSL, disable it.
  // if(type.isArray())
  //   return false;
  
  // Must be sized array.
  if (type.isArray() && type.isUnsizedArray()) {
    return false;
  }
  
  TBasicType basicType = type.getBasicType();

  // Cannot return a sampler in Vulkan GLSL.
  if (basicType == EbtSampler || basicType == EbtBlock || basicType == EbtAccStruct ||
      basicType == EbtRayQuery || basicType == EbtString) {
    return false;
  }
  
#ifndef GLSLANG_WEB
  if (basicType == EbtSpirvType) {
    return false;
  }
#endif
  
  if (basicType == EbtVoid) {
    return false;
  }
  
  if (basicType == EbtStruct) {
    
    name = type.getTypeName();
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  if ( basicType == EbtFloat  || basicType == EbtDouble || basicType == EbtFloat16 ||
       basicType == EbtInt8   || basicType == EbtUint8  || basicType == EbtInt16   ||
       basicType == EbtUint16 || basicType == EbtInt    || basicType == EbtUint    ||
       basicType == EbtInt64  || basicType == EbtUint64 || basicType == EbtBool) {
    
    bool validBasicType = false;
    
    if (type.isVector()) {
      validBasicType = getVectorTypeName(basicType, type.getVectorSize(), name);
    }
    else if (type.isMatrix()) {
      validBasicType = getMatrixTypeName(basicType, type.getMatrixCols(), type.getMatrixRows(), name);
    }
    else {
      validBasicType = getBasicTypeName(basicType, name);
    }
    
    if (type.isArray()) {
      for (int i = 0; i < type.getArraySizes()->getNumDims(); i++) {
        name += "[" + std::to_string(type.getArraySizes()->getDimSize(i)) + "]";
      }
    }
    
    return true;
  }
  
  return false;
}


TString getFunctionName(TString completeString) {
  TString::size_type found = completeString.find('(');
  if(found == completeString.npos)
    return completeString;
  return completeString.substr(0, found);
}

TString printConstant(const TType& type, const TConstUnionArray& unionArray, int& unionIndex) {
  
  TType basic;
  basic.shallowCopy(type);
  basic.clearArraySizes();
  
  TString returnType;
  TString basicType;
  
  if (!getConstantTypeName(basic, basicType)) {
    basicType = "<error-type>";
  }
  
  // type name
  if (!type.isScalar()) {
    if (!getConstantTypeName(type, returnType)) {
      returnType = "<error-type>";
    }
    returnType += "(";
  }
  
  if (type.isArray()) {
    int numDims = type.getArraySizes()->getNumDims();
    for (int i = 0; i < numDims; i++) {
      int arrSize = type.getArraySizes()->getDimSize(i);
      
      if (numDims > 1) {
        TString arrType = basicType;
        if (arrSize > 1) {
          arrType += "[" + std::to_string(arrSize) + "]";
        }
        returnType += arrType + "(";
      }
      
      for (int j = 0; j < arrSize; j++) {
        returnType += printConstant(basic, unionArray, unionIndex);
        if (j < (arrSize - 1)) {
          returnType += ", ";
        }
      }
      
      if (numDims > 1) {
        returnType += ")";
      }
      
      if (i < (numDims - 1)) {
        returnType += ", ";
      }
    }
  }
  else if (type.isStruct()) {
    const TTypeList* typeList = type.getStruct();
    if (typeList != nullptr) {
      for (int i = 0; i < typeList->size(); i++) {
        const TTypeLoc& memType = (*typeList)[i];
        returnType += printConstant(*memType.type, unionArray, unionIndex);
        if (i < (typeList->size() -1)) {
          returnType += ", ";
        }
      }
    }
  }
  else {
    // scalar, vector, matrix...
    int constCount = 0;
    if (basic.isMatrix()) {
      constCount = basic.getMatrixCols() * basic.getMatrixRows();
    }
    else if (basic.isVector()) {
      constCount = basic.getVectorSize();
    }
    else {
      constCount = 1;
    }
    
    for (int i = 0; i < constCount; i++) {
      
      switch (unionArray[unionIndex + i].getType()) {
      case EbtBool:
          returnType += unionArray[unionIndex + i].getBConst() ? "true" : "false";
          break;
      case EbtFloat:
      case EbtDouble:
      case EbtFloat16:
          returnType += std::to_string(unionArray[unionIndex + i].getDConst());
          break;
      case EbtInt8:
          returnType += std::to_string(unionArray[unionIndex + i].getI8Const());
          break;
      case EbtUint8:
          returnType += std::to_string(unionArray[unionIndex + i].getU8Const());
          break;
      case EbtInt16:
          returnType += std::to_string(unionArray[unionIndex + i].getI16Const());
          break;
      case EbtUint16:
          returnType += std::to_string(unionArray[unionIndex + i].getU16Const());
          break;
      case EbtInt:
          returnType += std::to_string(unionArray[unionIndex + i].getIConst());
          break;
      case EbtUint:
          returnType += std::to_string(unionArray[unionIndex + i].getUConst());
          break;
      case EbtInt64:
          returnType += std::to_string(unionArray[unionIndex + i].getI64Const());
          break;
      case EbtUint64:
          returnType += std::to_string(unionArray[unionIndex + i].getU64Const());
          break;
      default:
          printf("ERROR: type of constant is not allowed.\n");
          returnType += "<error-const>";
          break;
      }
      if (i < (constCount -1)) {
        returnType += ", ";
      }
    }
    unionIndex += constCount;
  }
  
  if (!type.isScalar()) {
    returnType += ")";
  }
  
  return returnType;
}

class TTranslatorToGLSL : public TIntermTraverser {
public:
  TTranslatorToGLSL();
  virtual ~TTranslatorToGLSL();
  
  void finish();

  bool visitAggregate(glslang::TVisit, glslang::TIntermAggregate*);
  bool visitBinary(glslang::TVisit, glslang::TIntermBinary*);
  void visitConstantUnion(glslang::TIntermConstantUnion*);
  bool visitSelection(glslang::TVisit, glslang::TIntermSelection*);
  bool visitSwitch(glslang::TVisit, glslang::TIntermSwitch*);
  void visitSymbol(glslang::TIntermSymbol*);
  bool visitUnary(glslang::TVisit, glslang::TIntermUnary*);
  bool visitLoop(glslang::TVisit, glslang::TIntermLoop*);
  bool visitBranch(glslang::TVisit, glslang::TIntermBranch*);
  
protected:
  TTranslatorToGLSL(TTranslatorToGLSL&);
  TTranslatorToGLSL& operator=(TTranslatorToGLSL&);
  
  TString printIndent();
  std::stringstream& GetOutStringStream();
  
private:
  // std::fstream out_;
  std::stringstream out_head_;
  std::stringstream out_body_;
  
  std::set<long long> tempVals;
  
  int indent_;
  
  bool use_out_head_;
  
  bool in_func_param_;
  bool in_func_temp_dcl_;
  bool in_linker_objects_;
};

TTranslatorToGLSL::TTranslatorToGLSL()
: TIntermTraverser(true, true, true, false), indent_(0), use_out_head_(false), in_func_param_(false), in_func_temp_dcl_(false), in_linker_objects_(false) {
}

TTranslatorToGLSL::~TTranslatorToGLSL() {
}

void TTranslatorToGLSL::finish() {
  std::cout << out_head_.str() << std::endl << std::endl;
  std::cout << out_body_.str() << std::endl;
}

TString TTranslatorToGLSL::printIndent() {
  int blanks = indent_ > 0 ? indent_ * 2 : 0;
  return TString(blanks, ' ');
}

std::stringstream& TTranslatorToGLSL::GetOutStringStream() {
  return use_out_head_ ? out_head_ : out_body_;
}

bool TTranslatorToGLSL::visitAggregate(glslang::TVisit visit, glslang::TIntermAggregate* node) {
  
  if (in_func_temp_dcl_)
    return true;
  
  if (node->getOp() == EOpNull) {
      return true;
  }
  
  bool travel = true;
  auto& o = GetOutStringStream();
  
  if (visit == EvPreVisit) {
    switch (node->getOp()) {
      case EOpFunction: {
        in_func_param_ = true;
        TString returnTypeName;
        if (!getReturnTypeName(node->getType(), returnTypeName)) {
          printf("ERROR: %s:%d translate return type %s failed\n", node->getLoc().getFilenameStr(),
                 node->getLoc().line, node->getType().getCompleteString().c_str());
          returnTypeName = "<error-type>";
        }
        o << printIndent() << returnTypeName << " " << getFunctionName(node->getName()) << "(";
        break;
      }
      case EOpLinkerObjects: {
        in_linker_objects_ = true;
        
        auto aggr = node->getAsAggregate();
        auto& seq = aggr->getSequence();
        
        use_out_head_ = true;
        for (std::size_t i = 0; i < seq.size(); i++) {
          auto& s = seq[i];
          s->traverse(this);
          GetOutStringStream() << ";\n";
        }
        use_out_head_ = false;
        
        travel = false;
        in_linker_objects_ = false;
        break;
      }
      default:
        // EOpParameters
        // EOpSequence
        break;
    }
  }
  else if (visit == EvPostVisit) {
    switch (node->getOp()) {
      case EOpFunction: {
        indent_--;
        o << printIndent() << "}\n\n";
        break;
      }
      default:
        // EOpSequence
        // EOpParameters
        break;
    }
  }
  else if (visit == EvInVisit) {
    switch (node->getOp()) {
      case EOpFunction: {
        in_func_param_ = false;
        o << ") {\n";
        indent_++;
        // Collect all temp variables and declared here.
        in_func_temp_dcl_ = true;
        node->traverse(this);
        in_func_temp_dcl_ = false;
        break;
      }
      case EOpParameters: {
        o << ", ";
        break;
      }
      default:
        // EOpSequence
        break;
    }
  }
  
  return travel;
}

bool TTranslatorToGLSL::visitBinary(glslang::TVisit visit, glslang::TIntermBinary* node) {
  
  if (in_func_temp_dcl_)
    return true;
  
  bool travel = true;
  auto& o = GetOutStringStream();
  
  if(visit == EvPreVisit) {
    switch (node->getOp()) {
      case EOpAssign:
      case EOpAddAssign:
      case EOpSubAssign:
      case EOpMulAssign:
      case EOpVectorTimesMatrixAssign:
      case EOpVectorTimesScalarAssign:
      case EOpMatrixTimesScalarAssign:
      case EOpMatrixTimesMatrixAssign:
      case EOpDivAssign:
      case EOpModAssign:
      case EOpAndAssign:
      case EOpInclusiveOrAssign:
      case EOpExclusiveOrAssign:
      case EOpLeftShiftAssign:
      case EOpRightShiftAssign:
        o << printIndent();
        break;
      case EOpAdd:
      case EOpSub:
      case EOpMul:
      case EOpDiv:
      case EOpMod:
      case EOpRightShift:
      case EOpLeftShift:
      case EOpAnd:
      case EOpInclusiveOr:
      case EOpExclusiveOr:
      case EOpEqual:
      case EOpNotEqual:
      case EOpLessThan:
      case EOpGreaterThan:
      case EOpLessThanEqual:
      case EOpGreaterThanEqual:
      case EOpVectorEqual:
      case EOpVectorNotEqual:
      case EOpVectorTimesScalar:
      case EOpVectorTimesMatrix:
      case EOpMatrixTimesVector:
      case EOpMatrixTimesScalar:
      case EOpMatrixTimesMatrix:
      case EOpLogicalOr:
      case EOpLogicalXor:
      case EOpLogicalAnd:
        o << "(" ;
        break;
      default:
        // EOpIndexDirect
        // EOpIndexIndirect
        // EOpIndexDirectStruct
        break;
    }
  } else if (visit == EvPostVisit) {
    switch (node->getOp()) {
      case EOpAssign:
      case EOpAddAssign:
      case EOpSubAssign:
      case EOpMulAssign:
      case EOpVectorTimesMatrixAssign:
      case EOpVectorTimesScalarAssign:
      case EOpMatrixTimesScalarAssign:
      case EOpMatrixTimesMatrixAssign:
      case EOpDivAssign:
      case EOpModAssign:
      case EOpAndAssign:
      case EOpInclusiveOrAssign:
      case EOpExclusiveOrAssign:
      case EOpLeftShiftAssign:
      case EOpRightShiftAssign:
        o << ";\n";
        break;
      case EOpIndexDirect:
      case EOpIndexIndirect:
        o << "]";
        break;
      case EOpAdd:
      case EOpSub:
      case EOpMul:
      case EOpDiv:
      case EOpMod:
      case EOpRightShift:
      case EOpLeftShift:
      case EOpAnd:
      case EOpInclusiveOr:
      case EOpExclusiveOr:
      case EOpEqual:
      case EOpNotEqual:
      case EOpLessThan:
      case EOpGreaterThan:
      case EOpLessThanEqual:
      case EOpGreaterThanEqual:
      case EOpVectorEqual:
      case EOpVectorNotEqual:
      case EOpVectorTimesScalar:
      case EOpVectorTimesMatrix:
      case EOpMatrixTimesVector:
      case EOpMatrixTimesScalar:
      case EOpMatrixTimesMatrix:
      case EOpLogicalOr:
      case EOpLogicalXor:
      case EOpLogicalAnd:
        o << ")" ;
        break;
      default:
        // EOpIndexDirectStruct
        break;
    }
  } else if (visit == EvInVisit) {
    switch (node->getOp()) {
      case EOpAssign:                   o << " = ";      break;
      case EOpAddAssign:                o << " += ";  break;
      case EOpSubAssign:                o << " -= ";  break;
      case EOpMulAssign:                o << " *= ";  break;
      case EOpVectorTimesMatrixAssign:  o << " *= " ; break;
      case EOpVectorTimesScalarAssign:  o << " *= " ; break;
      case EOpMatrixTimesScalarAssign:  o << " *= " ; break;
      case EOpMatrixTimesMatrixAssign:  o << " *= " ; break;
      case EOpDivAssign:                o << " /= ";  break;
      case EOpModAssign:                o << " %= ";  break;
      case EOpAndAssign:                o << " &= ";  break;
      case EOpInclusiveOrAssign:        o << " |= ";  break;
      case EOpExclusiveOrAssign:        o << " ^= ";  break;
      case EOpLeftShiftAssign:          o << " <<= "; break;
      case EOpRightShiftAssign:         o << " >>= "; break;
      case EOpIndexDirect:              o << "[";     break;
      case EOpIndexIndirect:            o << "[";     break;
      case EOpIndexDirectStruct: {
        o << ".";
        auto right = node->getRight()->getAsConstantUnion();
        auto left = node->getLeft();
        auto left_struct = left->getType().getStruct();
        int struct_index = right->getConstArray()[0].getIConst();
        o << (*left_struct)[struct_index].type->getFieldName();
        travel = false;
        break;
      }
      case EOpAdd:                      o << " + ";   break;
      case EOpSub:                      o << " - ";   break;
      case EOpMul:                      o << " * ";   break;
      case EOpDiv:                      o << " / ";   break;
      case EOpMod:                      o << " % ";   break;
      case EOpRightShift:               o << " >> ";  break;
      case EOpLeftShift:                o << " << ";  break;
      case EOpAnd:                      o << " & ";   break;
      case EOpInclusiveOr:              o << " | ";   break;
      case EOpExclusiveOr:              o << " ^ ";   break;
      case EOpEqual:                    o << " == ";  break;
      case EOpNotEqual:                 o << " != ";  break;
      case EOpLessThan:                 o << " < ";   break;
      case EOpGreaterThan:              o << " > ";   break;
      case EOpLessThanEqual:            o << " <= ";  break;
      case EOpGreaterThanEqual:         o << " >= ";  break;
      case EOpVectorEqual:              o << " == ";  break;
      case EOpVectorNotEqual:           o << " != ";  break;
      case EOpVectorTimesScalar:        o << " * ";   break;
      case EOpVectorTimesMatrix:        o << " * ";   break;
      case EOpMatrixTimesVector:        o << " * ";   break;
      case EOpMatrixTimesScalar:        o << " * ";   break;
      case EOpMatrixTimesMatrix:        o << " * ";   break;
      case EOpLogicalOr:                o << " || ";  break;
      case EOpLogicalXor:               o << " ^^ ";  break;
      case EOpLogicalAnd:               o << " && ";  break;
      default:
        break;
    }
  }
  return travel;
}

void TTranslatorToGLSL::visitConstantUnion(glslang::TIntermConstantUnion* node) {
  
  if (in_func_temp_dcl_)
    return;
  
  const TType& type = node->getType();
  const TBasicType basicType = node->getBasicType();
  auto& o = GetOutStringStream();
  
  if (basicType == EbtVoid || basicType == EbtAtomicUint || basicType == EbtSampler || basicType == EbtBlock ||
      basicType == EbtAccStruct || basicType == EbtReference || basicType == EbtRayQuery || basicType == EbtString
#ifndef GLSLANG_WEB
      || basicType == EbtSpirvType
#endif
      ) {
    printf("ERROR: %s:%d translate constant %s failed\n", node->getLoc().getFilenameStr(),
           node->getLoc().line, node->getType().getCompleteString().c_str());
    return;
  }

  int unionIndex = 0;
  o << printConstant(type, node->getConstArray(), unionIndex);
}

bool TTranslatorToGLSL::visitSelection(glslang::TVisit visit, glslang::TIntermSelection* node) {
  
  if (in_func_temp_dcl_)
    return true;
  
  auto& o = GetOutStringStream();
  
  if (visit == EvPreVisit) {
  
    // Conditions.
    ++depth;
    o << printIndent() << "if (";
    node->getCondition()->traverse(this);
    o << ") {\n";
    
    // True Case.
    if (node->getTrueBlock()) {
      indent_++;
      node->getTrueBlock()->traverse(this);
      indent_--;
    }
    o << printIndent() << "}\n";
    
    // False Case.
    if (node->getFalseBlock()) {
      o << printIndent() << "else {\n";
      indent_++;
      node->getFalseBlock()->traverse(this);
      indent_--;
      o << printIndent() << "}\n";
    }
    
    --depth;
  }
  
  // Has already processed, do not travel children.
  return false;
}

bool TTranslatorToGLSL::visitSwitch(glslang::TVisit visit, glslang::TIntermSwitch* node) {
  
  if (in_func_temp_dcl_)
    return true;
  
  auto& o = GetOutStringStream();
  
  if (visit == EvPreVisit) {
  
    // Conditions.
    ++depth;
    o << printIndent() << "switch (";
    node->getCondition()->traverse(this);
    o << ") {\n";
    
    indent_++;
    node->getBody()->traverse(this);
    indent_--;
    
    o << printIndent() << "}\n";
    
    --depth;
  }
  
  // Has already processed, do not travel children.
  return false;
}

void TTranslatorToGLSL::visitSymbol(glslang::TIntermSymbol* node) {
  
  auto& o = GetOutStringStream();
  
  if (in_func_temp_dcl_) {
    if ((node->getQualifier().storage == EvqTemporary) && (tempVals.find(node->getId()) == tempVals.end())) {
      TString tempDcl;
      TString baseName = node->getName();
      baseName += "_" + std::to_string(node->getId());
      if (!getTempTypeName(node->getType(), baseName, tempDcl)) {
        printf("ERROR: %s:%d translate return type %s failed\n", node->getLoc().getFilenameStr(),
               node->getLoc().line, node->getType().getCompleteString().c_str());
        tempDcl = "<error-temp-dcl>";
      }
      o << printIndent() << tempDcl << ";\n";
      tempVals.emplace(node->getId());
    }
    return;
  }
  
  if (in_linker_objects_) {
    TString typeName;
    if (!getLinkerObject(node->getType(), node->getName(), typeName)) {
      printf("ERROR: %s:%d translate linker object %s failed\n", node->getLoc().getFilenameStr(),
             node->getLoc().line, node->getType().getCompleteString().c_str());
      typeName = "<error-linker-object>";
    }
    o << typeName;
    return;
  }
  
  if (in_func_param_) {
    TString typeName;
    if (!getParameterTypeName(node->getType(), node->getName(), typeName)) {
      printf("ERROR: %s:%d translate function parameter type %s failed\n", node->getLoc().getFilenameStr(),
             node->getLoc().line, node->getType().getCompleteString().c_str());
      typeName = "<error-func-param>";
    }
    o << typeName;
    return;
  }
  
  if(node->getType().getQualifier().storage == EvqTemporary) {
    o << node->getName() << "_" << node->getId();
  }
  else {
    o << node->getName();
  }
}

bool TTranslatorToGLSL::visitUnary(glslang::TVisit visit, glslang::TIntermUnary* node) {
  
  if (in_func_temp_dcl_)
    return true;
  
  auto& o = GetOutStringStream();
  
  return true;
}

bool TTranslatorToGLSL::visitLoop(glslang::TVisit visit, glslang::TIntermLoop* node) {
  
  if (in_func_temp_dcl_)
    return true;
  
  auto& o = GetOutStringStream();
  
  return true;
}

bool TTranslatorToGLSL::visitBranch(glslang::TVisit visit, glslang::TIntermBranch* node) {
  
  if (in_func_temp_dcl_)
    return true;
  
  auto& o = GetOutStringStream();
  
  if(visit == EvPreVisit) {
    switch (node->getFlowOp()) {
      case EOpKill:                   o << printIndent() << "discard";          break;
      case EOpTerminateInvocation:    o << printIndent() << "terminateInvocation"; break;
      case EOpIgnoreIntersectionKHR:  o << printIndent() << "ignoreIntersectionEXT"; break;
      case EOpTerminateRayKHR:        o << printIndent() << "terminateRayEXT";  break;
      case EOpBreak:                  o << printIndent() << "break";            break;
      case EOpContinue:               o << printIndent() << "continue";         break;
      case EOpReturn:                 o << printIndent() << "return ";          break;
      case EOpCase:                   o << "\n" << printIndent() << "case ";    break;
      case EOpDemote:                 o << printIndent() << "demote";           break;
      case EOpDefault:                o << "\n" << printIndent() << "default";  break;
      default:                                                                     break;
    }
  }
  else if(visit == EvPostVisit) {
    switch (node->getFlowOp()) {
      case EOpKill:                   o << ";\n";                               break;
      case EOpTerminateInvocation:    o << ";\n";                               break;
      case EOpIgnoreIntersectionKHR:  o << ";\n";                               break;
      case EOpTerminateRayKHR:        o << ";\n";                               break;
      case EOpBreak:                  o << ";\n";                               break;
      case EOpContinue:               o << ";\n";                               break;
      case EOpReturn:                 o << ";\n";                               break;
      case EOpCase:                   o << ":\n";                               break;
      case EOpDemote:                 o << ";\n";                               break;
      case EOpDefault:                o << ":\n";                               break;
      default:                                                                     break;
    }
  }
  
  return true;
}

void AstToGlsl(const glslang::TIntermediate& intermediate, const char* baseName) {
  TTranslatorToGLSL translator;
  intermediate.getTreeRoot()->traverse(&translator);
  translator.finish();
}

} // namespace glslang
