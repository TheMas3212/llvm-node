//
// Created by Micha Reiser on 28.02.17.
//

#include "ir-builder.h"
#include "llvm-context.h"
#include "basic-block.h"
#include "function.h"
#include "type.h"
#include "phi-node.h"
#include "alloca-inst.h"

NAN_MODULE_INIT(IRBuilderWrapper::Init) {
    v8::Local<v8::FunctionTemplate> functionTemplate = Nan::New<v8::FunctionTemplate>(New);
    functionTemplate->SetClassName(Nan::New("IRBuilder").ToLocalChecked());
    functionTemplate->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(functionTemplate, "setInsertionPoint", IRBuilderWrapper::SetInsertionPoint);
    Nan::SetPrototypeMethod(functionTemplate, "createAlloca", IRBuilderWrapper::CreateAlloca);
    Nan::SetPrototypeMethod(functionTemplate, "createBr", IRBuilderWrapper::CreateBr);
    Nan::SetPrototypeMethod(functionTemplate, "createCall", IRBuilderWrapper::CreateCall);
    Nan::SetPrototypeMethod(functionTemplate, "createCondBr", IRBuilderWrapper::CreateCondBr);
    Nan::SetPrototypeMethod(functionTemplate, "createFAdd", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFAdd>);
    Nan::SetPrototypeMethod(functionTemplate, "createFCmpOLE", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFCmpOLE>);
    Nan::SetPrototypeMethod(functionTemplate, "createFCmpOLT", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFCmpOLT>);
    Nan::SetPrototypeMethod(functionTemplate, "createFCmpOEQ", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFCmpOEQ>);
    Nan::SetPrototypeMethod(functionTemplate, "createFCmpULE", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFCmpULE>);
    Nan::SetPrototypeMethod(functionTemplate, "createFCmpULT", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFCmpULT>);
    Nan::SetPrototypeMethod(functionTemplate, "createFCmpUEQ", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFCmpUEQ>);
    Nan::SetPrototypeMethod(functionTemplate, "createFPToSI", &IRBuilderWrapper::ConvertOperation<&llvm::IRBuilder<>::CreateFPToSI>);
    Nan::SetPrototypeMethod(functionTemplate, "createFDiv", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFDiv>);
    Nan::SetPrototypeMethod(functionTemplate, "createFRem", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFRem>);
    Nan::SetPrototypeMethod(functionTemplate, "createFSub", &IRBuilderWrapper::BinaryOperationFloat<&llvm::IRBuilder<>::CreateFSub>);
    Nan::SetPrototypeMethod(functionTemplate, "createICmpEQ", &IRBuilderWrapper::BinaryOperation<&llvm::IRBuilder<>::CreateICmpEQ>);
    Nan::SetPrototypeMethod(functionTemplate, "createSRem", &IRBuilderWrapper::BinaryOperation<&llvm::IRBuilder<>::CreateSRem>);
    Nan::SetPrototypeMethod(functionTemplate, "createLoad", IRBuilderWrapper::CreateLoad);
    Nan::SetPrototypeMethod(functionTemplate, "createPhi", IRBuilderWrapper::CreatePHI);
    Nan::SetPrototypeMethod(functionTemplate, "createRet", IRBuilderWrapper::CreateRet);
    Nan::SetPrototypeMethod(functionTemplate, "createRetVoid", IRBuilderWrapper::CreateRetVoid);
    Nan::SetPrototypeMethod(functionTemplate, "createSIToFP", &IRBuilderWrapper::ConvertOperation<&llvm::IRBuilder<>::CreateSIToFP>);
    Nan::SetPrototypeMethod(functionTemplate, "createStore", IRBuilderWrapper::CreateStore);
    Nan::SetPrototypeMethod(functionTemplate, "getInsertBlock", IRBuilderWrapper::GetInsertBlock);

    auto constructorFunction = Nan::GetFunction(functionTemplate).ToLocalChecked();
    irBuilderConstructor().Reset(constructorFunction);

    Nan::Set(target, Nan::New("IRBuilder").ToLocalChecked(), constructorFunction);
}

NAN_METHOD(IRBuilderWrapper::New) {
    if (!info.IsConstructCall()) {
        return Nan::ThrowTypeError("IRBuilder constructor needs to be called with new");
    }

    if (info.Length() < 1 || !(LLVMContextWrapper::isInstance(info[0]) || BasicBlockWrapper::isInstance(info[0]))) {
        return Nan::ThrowTypeError("IRBuilder constructor needs either be called with a context or a basic block");
    }

    IRBuilderWrapper* wrapper = nullptr;
    if (LLVMContextWrapper::isInstance(info[0])) {
        auto& llvmContext = LLVMContextWrapper::FromValue(info[0])->getContext();
        wrapper = new IRBuilderWrapper { llvm::IRBuilder<> { llvmContext } };
    } else {
        auto* basicBlock = BasicBlockWrapper::FromValue(info[0])->getBasicBlock();
        wrapper = new IRBuilderWrapper { llvm::IRBuilder<> { basicBlock, basicBlock->begin() } };
    }

    wrapper->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
}

typedef llvm::Value* (llvm::IRBuilder<>::*BinaryOpFloatFn)(llvm::Value*, llvm::Value*, const llvm::Twine&, llvm::MDNode *FPMathTag);
template<BinaryOpFloatFn method>
NAN_METHOD(IRBuilderWrapper::BinaryOperationFloat) {
    if (info.Length() < 2 || !ValueWrapper::isInstance(info[0]) || !ValueWrapper::isInstance(info[1])
                        || (info.Length() == 3 && !info[2]->IsString())
                        || info.Length() > 3) {
    return Nan::ThrowTypeError("Binary operation needs to be called with: lhs: Value, rhs: Value, name: string?");
    }

    auto* lhs = ValueWrapper::FromValue(info[0])->getValue();
    auto* rhs = ValueWrapper::FromValue(info[1])->getValue();
    std::string name {};

    if (info.Length() == 3) {
    name = ToString(info[2]);
    }

    auto* wrapper = IRBuilderWrapper::FromValue(info.Holder());
    llvm::Value* value = (wrapper->irBuilder.*method)(lhs, rhs, name, nullptr);

    info.GetReturnValue().Set(ValueWrapper::of(value));
}

typedef llvm::Value* (llvm::IRBuilder<>::*BinaryOpFn)(llvm::Value*, llvm::Value*, const llvm::Twine&);
template<BinaryOpFn method>
NAN_METHOD(IRBuilderWrapper::BinaryOperation) {
    if (info.Length() < 2 || !ValueWrapper::isInstance(info[0]) || !ValueWrapper::isInstance(info[1])
        || (info.Length() == 3 && !info[2]->IsString())
        || info.Length() > 3) {
        return Nan::ThrowTypeError("Binary operation needs to be called with: lhs: Value, rhs: Value, name: string?");
    }

    auto* lhs = ValueWrapper::FromValue(info[0])->getValue();
    auto* rhs = ValueWrapper::FromValue(info[1])->getValue();
    std::string name {};

    if (info.Length() == 3) {
        name = ToString(info[2]);
    }

    auto* wrapper = IRBuilderWrapper::FromValue(info.Holder());
    llvm::Value* value = (wrapper->irBuilder.*method)(lhs, rhs, name);

    info.GetReturnValue().Set(ValueWrapper::of(value));
}

typedef llvm::Value* (llvm::IRBuilder<>::*ConvertOperationFn)(llvm::Value*, llvm::Type*, const llvm::Twine&);
template<ConvertOperationFn method>
NAN_METHOD(IRBuilderWrapper::ConvertOperation) {
    if (info.Length() < 2 || !ValueWrapper::isInstance(info[0]) || !TypeWrapper::isInstance(info[1])
        || (info.Length() == 3 && !info[2]->IsString())
        || info.Length() > 3) {
        return Nan::ThrowTypeError("Convert operation needs to be called with: value: Value, type: Type, name: string?");
    }

    auto* value = ValueWrapper::FromValue(info[0])->getValue();
    auto* type = TypeWrapper::FromValue(info[1])->getType();
    std::string name {};

    if (info.Length() == 3) {
        name = ToString(info[2]);
    }

    auto* wrapper = IRBuilderWrapper::FromValue(info.Holder());
    llvm::Value* returnValue = (wrapper->irBuilder.*method)(value, type, name);

    info.GetReturnValue().Set(ValueWrapper::of(returnValue));
}

NAN_METHOD(IRBuilderWrapper::CreateAlloca) {
    if (info.Length() < 1 || !TypeWrapper::isInstance(info[0])
            || (info.Length() > 1 && !ValueWrapper::isInstance(info[1]) && !info[1]->IsUndefined())
            || (info.Length() > 2 && !info[2]->IsString())
            || info.Length() > 3) {
        return Nan::ThrowTypeError("createAlloca needs to be called with: type: Type, arraySize: Value, name?: string");
    }

    auto* type = TypeWrapper::FromValue(info[0])->getType();
    llvm::Value* value = nullptr;
    std::string name {};

    if (info.Length() > 1 && !info[1]->IsUndefined()) {
        value = ValueWrapper::FromValue(info[1])->getValue();
    }

    if (info.Length() > 2) {
        name = ToString(Nan::To<v8::String>(info[2]).ToLocalChecked());
    }

    auto& irBuilder = IRBuilderWrapper::FromValue(info.Holder())->irBuilder;
    auto* alloc = irBuilder.CreateAlloca(type, value, name);

    info.GetReturnValue().Set(AllocaInstWrapper::of(alloc));
}

NAN_METHOD(IRBuilderWrapper::CreateLoad) {
    if (info.Length() < 1 || !ValueWrapper::isInstance(info[0])
            || (info.Length() > 1 && !info[1]->IsString())
            || info.Length() > 2) {
        return Nan::ThrowTypeError("createLoad needs to be called with: value: Value, name?: string");
    }

    auto* value = ValueWrapper::FromValue(info[0])->getValue();
    std::string name {};

    if (info.Length() > 1) {
        name = ToString(Nan::To<v8::String>(info[1]).ToLocalChecked());
    }

    auto& irBuilder = IRBuilderWrapper::FromValue(info.Holder())->irBuilder;
    auto* inst = irBuilder.CreateLoad(value, name);
    info.GetReturnValue().Set(ValueWrapper::of(inst));
}

NAN_METHOD(IRBuilderWrapper::CreateStore) {
    if (info.Length() < 2 || !ValueWrapper::isInstance(info[0]) || !ValueWrapper::isInstance(info[1])
            || (info.Length() > 2 && !info[2]->IsBoolean())
            || info.Length() > 3) {
        return Nan::ThrowTypeError("createStore needs to be called with: value: Value, ptr: Value, isVolatile?: boolean");
    }

    auto* value = ValueWrapper::FromValue(info[0])->getValue();
    auto* ptr = ValueWrapper::FromValue(info[1])->getValue();
    bool isVolatile = false;

    if (info.Length() > 2) {
        isVolatile = Nan::To<bool>(info[2]).FromJust();
    }

    auto* inst = IRBuilderWrapper::FromValue(info.Holder())->irBuilder.CreateStore(value, ptr, isVolatile);
    info.GetReturnValue().Set(ValueWrapper::of(inst));
}

NAN_METHOD(IRBuilderWrapper::CreateCall) {
    if (info.Length() < 2 || !FunctionWrapper::isInstance(info[0]) || !info[1]->IsArray()
            || (info.Length() > 3 && !info[2]->IsString())
            || info.Length() > 3){
        return Nan::ThrowTypeError("createCall needs to be called with: callee: Function, args: Value[], name: string?");
    }

    auto* callee = FunctionWrapper::FromValue(info[0])->getFunction();
    auto* argsArray = v8::Array::Cast(*info[1]);
    std::vector<llvm::Value*> args { argsArray->Length() };
    std::string name {};

    for (uint32_t i = 0; i < argsArray->Length(); ++i) {
        if (!ValueWrapper::isInstance(argsArray->Get(i))) {
            return Nan::ThrowTypeError("Expected Value");
        }
        args[i] = ValueWrapper::FromValue(argsArray->Get(i))->getValue();
    }

    if (info.Length() == 3) {
        name = ToString(Nan::To<v8::String>(info[2]).ToLocalChecked());
    }

    auto* callInstr = IRBuilderWrapper::FromValue(info.Holder())->irBuilder.CreateCall(callee, args, name);
    info.GetReturnValue().Set(ValueWrapper::of(callInstr));
}

NAN_METHOD(IRBuilderWrapper::CreatePHI) {
    if (info.Length() < 2 || !TypeWrapper::isInstance(info[0]) || !info[1]->IsUint32()
            || (info.Length() == 3 && !info[2]->IsString())
            || info.Length() > 3) {
        return Nan::ThrowTypeError("createPhi needs to be called with: type: Type, numReservedValues: uint32, name: string?");
    }

    auto* type = TypeWrapper::FromValue(info[0])->getType();
    auto numReservedValues = Nan::To<uint32_t>(info[1]).ToChecked();
    std::string name {};

    if (info.Length() == 3) {
        name = ToString(Nan::To<v8::String>(info[2]).ToLocalChecked());
    }

    auto* phi = IRBuilderWrapper::FromValue(info.Holder())->irBuilder.CreatePHI(type, numReservedValues, name);
    info.GetReturnValue().Set(PhiNodeWrapper::of(phi));
}

NAN_METHOD(IRBuilderWrapper::CreateRet) {
    if (info.Length() != 1 || !ValueWrapper::isInstance(info[0])) {
        return Nan::ThrowTypeError("createRet needs to be called with: value: Value");
    }

    auto* value = ValueWrapper::FromValue(info[0])->getValue();
    auto& builder = IRBuilderWrapper::FromValue(info.Holder())->irBuilder;

    auto* returnInstruction = builder.CreateRet(value);
    info.GetReturnValue().Set(ValueWrapper::of(returnInstruction));
}

NAN_METHOD(IRBuilderWrapper::CreateRetVoid) {
    auto& builder = IRBuilderWrapper::FromValue(info.Holder())->irBuilder;
    auto* returnInstruction = builder.CreateRetVoid();
    info.GetReturnValue().Set(ValueWrapper::of(returnInstruction));
}

NAN_METHOD(IRBuilderWrapper::GetInsertBlock) {
    auto* builder = IRBuilderWrapper::FromValue(info.Holder());
    auto* block = builder->irBuilder.GetInsertBlock();
    info.GetReturnValue().Set(BasicBlockWrapper::of(block));
}

NAN_METHOD(IRBuilderWrapper::SetInsertionPoint) {
    if (info.Length() != 1 || !BasicBlockWrapper::isInstance(info[0])) {
        return Nan::ThrowTypeError("setInsertionPoint needs to be called with: basicBlock: BasicBlock");
    }

    auto* basicBlock = BasicBlockWrapper::FromValue(info[0])->getBasicBlock();
    auto* builder = IRBuilderWrapper::FromValue(info.Holder());

    builder->irBuilder.SetInsertPoint(basicBlock);
}

NAN_METHOD(IRBuilderWrapper::CreateBr) {
    if (info.Length() != 1 || !BasicBlockWrapper::isInstance(info[0])) {
        return Nan::ThrowTypeError("createBr needs to be called with: basicBlock: BasicBlock");
    }

    auto* basicBlock = BasicBlockWrapper::FromValue(info[0])->getBasicBlock();
    auto* builder = IRBuilderWrapper::FromValue(info.Holder());
    auto* branchInst = builder->irBuilder.CreateBr(basicBlock);

    info.GetReturnValue().Set(ValueWrapper::of(branchInst));
}

NAN_METHOD(IRBuilderWrapper::CreateCondBr) {
    if (info.Length() != 3 || !ValueWrapper::isInstance(info[0]) || !BasicBlockWrapper::isInstance(info[1]) || !BasicBlockWrapper::isInstance(info[2])) {
        return Nan::ThrowTypeError("createCondBr needs to be called with: condition: Value, then: BasicBlock, else: BasicBlock");
    }

    auto* value = ValueWrapper::FromValue(info[0])->getValue();
    auto* thenBlock = BasicBlockWrapper::FromValue(info[1])->getBasicBlock();
    auto* elseBlock = BasicBlockWrapper::FromValue(info[2])->getBasicBlock();

    auto* branchInst = IRBuilderWrapper::FromValue(info.Holder())->irBuilder.CreateCondBr(value, thenBlock, elseBlock);
    info.GetReturnValue().Set(ValueWrapper::of(branchInst));
}