//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2020 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#include <stdafx.h>
#include <array>
#include <unordered_set>
#include <vd2/system/binary.h>
#include <vd2/system/constexpr.h>
#include <vd2/system/hash.h>
#include <at/atvm/compiler.h>
#include <at/atvm/vm.h>

ATVMCompileError::ATVMCompileError(const ATVMDataValue& ref, const char *err)
	: MyError(err)
	, mSrcOffset(ref.mSrcOffset)
{
}

ATVMCompileError ATVMCompileError::Format(const ATVMDataValue& ref, const char *format, ...) {
	ATVMCompileError e;
	e.mSrcOffset = ref.mSrcOffset;

	va_list val;

	va_start(val, format);
	e.vsetf(format, val);
	va_end(val);

	return e;
}

///////////////////////////////////////////////////////////////////////////

ATVMCompiler::ATVMCompiler(ATVMDomain& domain)
	: mpDomain(&domain)
{
	// function pointer type 0 is always void()
	(void)GetFunctionPointerTypeIndex(vdvector_view(&kATVMTypeVoid, 1));

	// add break stack sentinel
	mBreakTargetLabels.push_back(0);
}

void ATVMCompiler::SetBindEventHandler(vdfunction<bool(ATVMCompiler&, const char *, const ATVMScriptFragment&)> bindEventFn) {
	mpBindEvent = std::move(bindEventFn);
}

void ATVMCompiler::SetOptionHandler(vdfunction<bool(ATVMCompiler&, const char *, const ATVMDataValue&)> setOptionFn) {
	mpSetOption = std::move(setOptionFn);
}

void ATVMCompiler::SetUnsafeCallsAllowed(bool allowed) {
	mbUnsafeCallsAllowed = allowed;
}

bool ATVMCompiler::IsValidVariableName(const char *name) {
	char c = *name;

	if (c != '_' && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
		return false;

	while((c = *++name)) {
		if (c != '_' && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9'))
			return false;
	}

	return true;
}

bool ATVMCompiler::IsSpecialVariableReferenced(const char *name) const {
	auto it = mSpecialVariableLookup.find_as(name);

	if (it == mSpecialVariableLookup.end())
		return false;
	
	uint32 idx = it->second.mIndex;

	return idx < mSpecialVariablesReferenced.size() && mSpecialVariablesReferenced[idx];
}

void ATVMCompiler::DefineClass(const ATVMObjectClass& objClass, DefineInstanceFn *defineFn) {
	auto r = mClassLookup.insert_as(objClass.mpClassName);

	if (r.second) {
		r.first->second.mTypeInfo = ATVMTypeInfo { ATVMTypeClass::ObjectClass, 0, &objClass };
	} else
		VDASSERT(r.first->second.mTypeInfo.mpObjectClass == &objClass);

	if (defineFn)
		r.first->second.mpDefineFn = std::move(*defineFn);
}

bool ATVMCompiler::DefineObjectVariable(const char *name, ATVMObject *obj, const ATVMObjectClass& objClass) {
	if (!IsValidVariableName(name))
		return ReportErrorF("Invalid variable name '%s'", name);

	auto r = mVariableLookup.insert_as(name);

	if (!r.second)
		return ReportErrorF("Variable '%s' has already been defined", name);

	if (mClassLookup.find_as(name) != mClassLookup.end())
		return ReportErrorF("'%s' cannot be declared as a variable because it is a class name", name);

	mpDomain->mGlobalVariables.push_back((sint32)mpDomain->mGlobalObjects.size());
	mpDomain->mGlobalVariableNames.push_back(mpDomain->StoreString(name));

	mpDomain->mGlobalObjects.push_back(obj);
	r.first->second = TypeInfo { TypeClass::ObjectLValue, mVariableCount++, &objClass };

	DefineClass(objClass);
	return true;
}

bool ATVMCompiler::DefineSpecialObjectVariable(const char *name, ATVMObject *obj, const ATVMObjectClass& objClass) {
	auto r = mSpecialVariableLookup.insert_as(name);
	VDASSERT(r.second);

	mpDomain->mSpecialVariables.push_back((sint32)mpDomain->mGlobalObjects.size());
	mpDomain->mGlobalObjects.push_back(obj);
	r.first->second = TypeInfo { TypeClass::ObjectLValue, (uint8)(mpDomain->mSpecialVariables.size() - 1), &objClass };

	DefineClass(objClass);
	return true;
}

bool ATVMCompiler::DefineIntegerVariable(const char *name, bool isConst, sint32 initValue) {
	if (!IsValidVariableName(name))
		return ReportErrorF("Invalid variable name '%s'", name);

	if (mClassLookup.find_as(name) != mClassLookup.end())
		return ReportErrorF("'%s' cannot be declared as a variable because it is a class name", name);

	auto r = mVariableLookup.insert_as(name);

	if (!r.second)
		return ReportErrorF("Variable '%s' has already been defined", name);

	if (isConst) {
		r.first->second = TypeInfo { TypeClass::IntConst, (uint32)initValue };
	} else {
		mpDomain->mGlobalVariables.push_back(initValue);
		mpDomain->mGlobalVariableNames.push_back(mpDomain->StoreString(name));
		r.first->second = TypeInfo { TypeClass::IntLValueVariable, mVariableCount++ };
	}

	return true;
}

void ATVMCompiler::DefineSpecialVariable(const char *name) {
	auto r = mSpecialVariableLookup.insert_as(name);
	VDASSERT(r.second);

	mpDomain->mSpecialVariables.push_back(0);
	r.first->second = TypeInfo { TypeClass::IntLValueVariable, (uint8)(mpDomain->mSpecialVariables.size() - 1) };
}

void ATVMCompiler::DefineThreadVariable(const char *name) {
	auto r = mThreadVariableLookup.insert_as(name);
	VDASSERT(r.second);

	++mpDomain->mNumThreadVariables;
	mpDomain->mSpecialVariables.push_back(0);
	r.first->second = TypeInfo { TypeClass::IntLValueVariable, (uint8)(mThreadVariableLookup.size() - 1) };
}

const ATVMTypeInfo *ATVMCompiler::GetVariable(const char *name) const {
	auto it = mVariableLookup.find_as(name);

	if (it == mVariableLookup.end())
		return nullptr;

	return &it->second;
}

const ATVMFunction *ATVMCompiler::DeferCompile(const ATVMTypeInfo& returnType, const ATVMScriptFragment& scriptFragment, ATVMFunctionFlags asyncAllowedMask, ATVMConditionalMask conditionalMask) {
	ATVMFunction *func = mpDomain->mAllocator.Allocate<ATVMFunction>();
	VDStringA name;
	name.sprintf("<anonymous function %u>", (unsigned)mpDomain->mFunctions.size() + 1);
	char *name2 = (char *)mpDomain->mAllocator.Allocate(name.size() + 1);
	memcpy(name2, name.c_str(), name.size() + 1);
	func->mpName = name2;
	mpDomain->mFunctions.push_back(func);
	
	FunctionInfo *fi = AllocTemp<FunctionInfo>();
	mFunctionInfoTable.push_back(fi);

	fi->mAsyncAllowedMask = asyncAllowedMask;
	fi->mDefinitionPos = scriptFragment.mSrcOffset;
	fi->mFunctionIndex = (sint32)mpDomain->mFunctions.size() - 1;

	mDeferredCompiles.emplace_back(
		[returnType, &scriptFragment, asyncAllowedMask, conditionalMask, func, fi, this] {
			mpReturnType = &returnType;

			const auto prevSrc = mpSrc;
			const auto prevSrcEnd = mpSrcEnd;
			const auto prevSrcLine = mSrcLine;

			InitSource(scriptFragment.mpSrc, scriptFragment.mpSrc + scriptFragment.mSrcOffset, scriptFragment.mSrcLength, scriptFragment.mSrcLine);

			mpCurrentFunctionInfo = fi;
			bool success = ParseFunction(func, returnType, asyncAllowedMask, conditionalMask);
			mpCurrentFunctionInfo = nullptr;

			if (success && Token() != kTokEnd)
				success = ReportError("Expected end of script");

			mpSrc = prevSrc;
			mpSrcEnd = prevSrcEnd;
			mSrcLine = prevSrcLine;

			return success;
		}
	);

	return func;
}

bool ATVMCompiler::CompileDeferred() {
	for(size_t i = 0; i < mDeferredCompiles.size(); ++i) {
		const auto& step =  mDeferredCompiles[i];

		if (!step())
			return false;
	}

	// check for undefined functions
	for(const ATVMFunction *func : mpDomain->mFunctions) {
		if (!func->mpByteCode) {
			if (!func->mpName)
				return ReportError("Internal compiler error: anonymous function was not defined");
			else
				return ReportErrorF("Function '%s' declared but not defined", func->mpName);
		}
	}

	// propagate and validate dependencies
	for(;;) {
		bool changed = false;

		for(const auto [callerIndex, calleeIndex] : mFunctionDependencies) {
			FunctionInfo *callerInfo = mFunctionInfoTable[callerIndex];
			FunctionInfo *calleeInfo = mFunctionInfoTable[calleeIndex];

			// propagate flags upward
			ATVMFunctionFlags missingFlags = ~callerInfo->mFlags & calleeInfo->mFlags & ATVMFunctionFlags::AsyncAll;
			if (missingFlags != ATVMFunctionFlags{}) {
				changed = true;

				if ((~callerInfo->mAsyncAllowedMask & missingFlags) != ATVMFunctionFlags{}) {
					return ReportErrorF("Function %s() can suspend in a mode not allowed by calling function %s()"
						, mpDomain->mFunctions[calleeInfo->mFunctionIndex]->mpName
						, mpDomain->mFunctions[callerInfo->mFunctionIndex]->mpName
					);
				}

				callerInfo->mFlags |= missingFlags;
			}

			// propagate restrictions downward
			ATVMFunctionFlags missingRestrictions = ~callerInfo->mAsyncAllowedMask & calleeInfo->mAsyncAllowedMask;
			if (missingRestrictions != ATVMFunctionFlags{}) {
				changed = true;

				if ((callerInfo->mFlags & missingFlags) != ATVMFunctionFlags{}) {
					return ReportErrorF("Function %s() can suspend in a mode not allowed by calling function %s()"
						, mpDomain->mFunctions[calleeInfo->mFunctionIndex]->mpName
						, mpDomain->mFunctions[callerInfo->mFunctionIndex]->mpName
					);
				}

				callerInfo->mAsyncAllowedMask &= ~missingRestrictions;
			}
		}

		if (!changed)
			break;
	}

	return true;
}

bool ATVMCompiler::CompileFile(const char *src, size_t len) {
	mpSrc0 = src;
	InitSource(src, src, len, 1);

	return ParseFile();
}

const char *ATVMCompiler::GetError() const {
	return mError.empty() ? nullptr : mError.c_str();
}

std::pair<uint32, uint32> ATVMCompiler::GetErrorLinePos() const {
	uint32 lineNo = 1;
	const char *lineStart = mpSrc0;
	const char *errorPos = mpSrc0 + mErrorPos;

	for(const char *s = mpSrc0; s != errorPos; ++s) {
		if (*s == '\n') {
			++lineNo;
			lineStart = s+1;
		}
	}

	return { lineNo, (uint32)(errorPos - lineStart) + 1 };
}

void ATVMCompiler::InitSource(const void *src0, const void *src, size_t len, uint32 line) {
	mpSrc = (const char *)src;
	mpSrcStart = mpSrc;
	mpSrcEnd = mpSrc + len;
	mSrcLine = line;

	mMethodPtrs.clear();
	mByteCodeBuffer.clear();
	mByteCodeSourceRanges.clear();
	mPushedToken = 0;

	mError.clear();
}

bool ATVMCompiler::ParseFile() {
	for(;;) {
		uint32 tok = Token();
		if (tok == kTokError)
			return false;

		if (tok == kTokEnd)
			break;

		if (tok == kTokFunction) {
			if (!ParseFunction())
				return false;
		} else if (tok == kTokInt || tok == kTokVoid || tok == kTokIdentifier || tok == kTokConst) {
			if (!ParseVariableDefinition(tok))
				return false;
		} else if (tok == kTokEvent) {
			if (!ParseEventBinding())
				return false;
		} else if (tok == kTokOption) {
			if (!ParseOption())
				return false;
		} else
			return ReportError("Function or variable definition expected");
	}

	return true;
}

bool ATVMCompiler::ParseVariableDefinition(uint32 typeTok) {
	bool isConst = false;

	if (typeTok == kTokConst) {
		isConst = true;

		typeTok = Token();

		if (typeTok == kTokIdentifier)
			return ReportError("'const' can only be used on integer variables");

		if (typeTok != kTokVoid && typeTok != kTokInt)
			return ReportError("Expected type after 'const'");
	}

	if (typeTok == kTokVoid)
		return ReportError("Variables cannot be of type void");

	ClassInfo *classInfo = nullptr;

	if (typeTok == kTokIdentifier) {
		auto itClass = mClassLookup.find_as(mTokIdent);

		if (itClass == mClassLookup.end())
			return ReportErrorF("Unknown type '%.*s'", (int)mTokIdent.size(), mTokIdent.data());

		classInfo = &itClass->second;

		if (!classInfo->mpDefineFn)
			return ReportErrorF("Cannot instantiate instances of class type '%.*s'", (int)mTokIdent.size(), mTokIdent.data());
	}

	for(;;) {
		uint32 tok = Token();

		if (tok != kTokIdentifier)
			return ReportError("Expected variable name");

		VDStringA varName(mTokIdent);
		if (!IsValidVariableName(varName.c_str()))
			return ReportErrorF("Invalid variable name: '%s'", varName.c_str());

		if (mClassLookup.find_as(varName) != mClassLookup.end())
			return ReportErrorF("'%s' cannot be declared as a variable because it is a class name", varName.c_str());

		auto r = mVariableLookup.find_as(varName);

		if (r != mVariableLookup.end())
			return ReportErrorF("Variable '%s' has already been defined", varName.c_str());

		if (classInfo) {
			// check for initializer
			ATVMDataValue initializer {};

			tok = Token();
			if (tok == ':') {
				if (!ParseDataValue(initializer))
					return false;

				tok = Token();
			}

			try {
				if (!classInfo->mpDefineFn(*this, varName.c_str(), initializer.mType != ATVMDataType::Invalid ? &initializer : nullptr))
					return false;
			} catch(const ATVMCompileError& e) {
				if (e.mSrcOffset != ~UINT32_C(0))
					return ReportError(e.mSrcOffset, e.c_str());
				else
					return ReportError(e.c_str());
			} catch(const MyError& e) {
				return ReportError(e.c_str());
			}
		} else {
			sint32 initValue = 0;

			tok = Token();
			if (tok == '=') {
				ATVMTypeInfo initializerValue {};
				if (!ParseConstantExpression(initializerValue))
					return false;

				if (initializerValue.mClass != ATVMTypeClass::IntConst)
					return ReportError("Initializer for integer variable is not integer type");

				initValue = (sint32)initializerValue.mIndex;

				tok = Token();
			}

			if (!DefineIntegerVariable(varName.c_str(), isConst, initValue))
				return false;
		}

		if (tok == ';')
			break;

		if (tok != ',')
			return ReportError("Expected ';' or ',' after variable name");
	}

	return true;
}

bool ATVMCompiler::ParseEventBinding() {
	if (Token() != kTokStringLiteral)
		return ReportError("Event name expected");

	VDStringA eventName(mTokIdent);

	if (Token() != ':')
		return ReportError("Expected ':' after event name");

	ATVMDataValue value;
	if (!ParseDataValue(value))
		return false;

	if (!value.IsScript())
		return ReportError("Expected inline script");

	if (Token() != ';')
		return ReportError("Expected ';' at end of event binding");

	return mpBindEvent(*this, eventName.c_str(), *value.mpScript);
}

bool ATVMCompiler::ParseOption() {
	if (Token() != kTokStringLiteral)
		return ReportError("Option name expected");

	VDStringA optionName(mTokIdent);

	if (Token() != ':')
		return ReportError("Expected ':' after option name");

	ATVMDataValue value;
	if (!ParseDataValue(value))
		return false;

	if (Token() != ';')
		return ReportError("Expected ';' at end of option directive");

	if (optionName == "debug") {
		// reject if any functions have been compiled or queued
		if (!mDeferredCompiles.empty() || !mpDomain->mFunctions.empty())
			return ReportError("Option 'debug' must be set before any functions are declared");

		if (!value.IsInteger())
			return ReportError("Option 'debug' value must be an integer");

		mbCompileDebugCode = (value.mIntValue != 0);
		return true;
	}

	return mpSetOption(*this, optionName.c_str(), value);
}

bool ATVMCompiler::ParseFunction() {
	uint32 tok = Token();
	ATVMTypeInfo returnType;

	if (tok == kTokVoid)
		returnType = ATVMTypeInfo { ATVMTypeClass::Void };
	else if (tok == kTokInt)
		returnType = ATVMTypeInfo { ATVMTypeClass::Int };
	else
		return ReportError("Return type expected (int or void)");

	mpReturnType = &returnType;

	tok = Token();
	if (tok != kTokIdentifier)
		return ReportError("Function name expected");

	const VDStringA& name(mTokIdent);

	if (mpDomain->mFunctions.size() >= 256)
		return ReportError("Named function count limit exceeded (256 max)");

	if (mVariableLookup.find_as(name.c_str()) != mVariableLookup.end())
		return ReportError("Variable with same name has already been declared");

	auto r = mFunctionLookup.insert_as(name.c_str());
	bool alreadyDefined = false;

	ATVMFunction *func = nullptr;
	FunctionInfo *fi = r.first->second;
	if (!r.second) {
		func = const_cast<ATVMFunction *>(mpDomain->mFunctions[fi->mFunctionIndex]);

		if (func->mpByteCode)
			alreadyDefined = true;

		if (func->mReturnType != returnType)
			return ReportErrorF("Function '%s' previously declared with different return type", name.c_str());
	} else {
		fi = AllocTemp<FunctionInfo>();
		mFunctionInfoTable.push_back(fi);

		fi->mFunctionIndex = mpDomain->mFunctions.size();
		r.first->second = fi;

		// we might compile inline functions, so reserve this slot first
		func = mpDomain->mAllocator.Allocate<ATVMFunction>();
		char *persistentName = (char *)mpDomain->mAllocator.Allocate(name.size() + 1);
		memcpy(persistentName, name.c_str(), mTokIdent.size() + 1);
		func->mpName = persistentName;
		func->mReturnType = returnType;
		mpDomain->mFunctions.push_back(func);
	}

	tok = Token();
	if (tok != '(')
		return ReportError("Expected '('");

	tok = Token();
	if (tok != ')')
		return ReportError("Expected ')'");

	tok = Token();
	if (tok == ';') {
		// declaration
		return true;
	} else {
		// definition
		if (alreadyDefined)
			return ReportErrorF("Function '%s' has already been declared", name.c_str());

		if (tok != '{')
			return ReportError("Expected '{'");

		fi->mDefinitionPos = (sint32)(mpSrc - mpSrc0);

		mpCurrentFunctionInfo = fi;
		bool success = ParseFunction(func, returnType, (ATVMFunctionFlags)~UINT32_C(0), ATVMConditionalMask::None);
		mpCurrentFunctionInfo = nullptr;

		if (!success)
			return false;

		tok = Token();
		if (tok != '}')
			return ReportError("Expected '}' at end of function");

		return success;
	}
}

bool ATVMCompiler::ParseFunction(ATVMFunction *func, const ATVMTypeInfo& returnType, ATVMFunctionFlags asyncAllowedMask, ATVMConditionalMask conditionalMask) {
	bool hasReturn = false;

	mMethodPtrs.clear();
	mByteCodeBuffer.clear();
	mByteCodeSourceRanges.clear();
	mLocalLookup.clear();
	mLabelCounter = 0;
	mKnownLabelOffsets.clear();
	
	mpCurrentFunction = func;
	mAsyncAllowedMask = asyncAllowedMask;
	mConditionalMask = conditionalMask;

	if (mbCompileDebugCode)
		mConditionalMask |= ATVMConditionalMask::DebugEnabled;
	else
		mConditionalMask |= ATVMConditionalMask::NonDebugEnabled;

	func->mLocalSlotsRequired = 0;

	if (!ParseBlock(hasReturn))
		return false;

	EmitSourceReference();

	if (!hasReturn) {
		if (returnType.mClass == ATVMTypeClass::Void)
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::ReturnVoid);
		else
			return ReportErrorF("No return at end of function '%s'", func->mpName);
	}

	if (!mPendingBranchTargets.empty())
		return ReportError("Internal compiler error: Unresolved branch targets");

	if (mBreakTargetLabels.size() != 1)
		return ReportError("Internal compiler error: Break stack invalid after function");

	uint32 bclen = (uint32)mByteCodeBuffer.size();

	uint8 *byteCode = (uint8 *)mpDomain->mAllocator.Allocate(bclen);
	memcpy(byteCode, mByteCodeBuffer.data(), bclen);

	void (**methodTable)() = nullptr;
	
	if (!mMethodPtrs.empty()) {
		methodTable = mpDomain->mAllocator.AllocateArray<void (*)()>(mMethodPtrs.size());
		memcpy(methodTable, mMethodPtrs.data(), mMethodPtrs.size() * sizeof(methodTable[0]));
	}

	func->mReturnType = returnType;
	func->mpByteCode = byteCode;
	func->mByteCodeLen = bclen;
	func->mpMethodTable = methodTable;
	func->mStackSlotsRequired = 0;

	struct StackState {
		sint32 mParent;
		TypeClass mTopType;
		uint16 mDepth;
	};

	struct StackStatePred {
		bool operator()(const StackState& x, const StackState& y) const {
			return x.mParent == y.mParent
				&& x.mTopType == y.mTopType;
		}

		size_t operator()(const StackState& x) const {
			return x.mParent ^ ((uint32)x.mTopType << 16);
		}
	};

	vdfastvector<sint32> ipToStackId(bclen, -1);

	vdfastvector<StackState> stackStateList;
	vdhashmap<StackState, uint32, StackStatePred, StackStatePred> stackStateLookup;

	stackStateList.push_back(StackState{});
	stackStateLookup[StackState{}] = 0;

	struct TraversalTarget {
		sint32 mIP;
		sint32 mStackState;
	};

	vdfastvector<TraversalTarget> traversalStack;

	traversalStack.push_back({0, 0});

	while(!traversalStack.empty()) {
		auto [ip, stackId] = traversalStack.back();
		traversalStack.pop_back();

		for(;;) {
			if (ip < 0 || (uint32)ip >= bclen)
				return ReportError("Internal compiler error: Bytecode validation failed (invalid branch target)");

			if (ipToStackId[ip] < 0)
				ipToStackId[ip] = stackId;
			else {
				if (ipToStackId[ip] != stackId)
					return ReportError("Internal compiler error: Bytecode validation failed (stack mismatch)");

				break;
			}

			ATVMOpcode opcode = (ATVMOpcode)byteCode[ip];
			uint32 popCount = 0;
			TypeClass pushType = TypeClass::Void;

			switch(opcode) {
				case ATVMOpcode::Nop:
				case ATVMOpcode::ReturnVoid:
				case ATVMOpcode::IntNeg:
				case ATVMOpcode::IntNot:
				case ATVMOpcode::Jmp:
				case ATVMOpcode::Ljmp:
				case ATVMOpcode::LoopChk:
				case ATVMOpcode::Not:
					// no stack change
					break;

				case ATVMOpcode::Dup:
					pushType = stackStateList[stackId].mTopType;
					break;

				case ATVMOpcode::IVLoad:
				case ATVMOpcode::ILLoad:
				case ATVMOpcode::ISLoad:
				case ATVMOpcode::ITLoad:
				case ATVMOpcode::IntConst:
				case ATVMOpcode::IntConst8:
					pushType = TypeClass::Int;
					break;

				case ATVMOpcode::MethodCallVoid:
					popCount = byteCode[ip+1] + 1;
					break;

				case ATVMOpcode::MethodCallInt:
					popCount = byteCode[ip+1];
					break;

				case ATVMOpcode::StaticMethodCallVoid:
				case ATVMOpcode::FunctionCallVoid:
					popCount = byteCode[ip+1];
					break;

				case ATVMOpcode::StaticMethodCallInt:
				case ATVMOpcode::FunctionCallInt:
					popCount = byteCode[ip+1];
					pushType = TypeClass::Int;
					break;

				case ATVMOpcode::IntAdd:
				case ATVMOpcode::IntSub:
				case ATVMOpcode::IntMul:
				case ATVMOpcode::IntDiv:
				case ATVMOpcode::IntMod:
				case ATVMOpcode::IntAnd:
				case ATVMOpcode::IntOr:
				case ATVMOpcode::IntXor:
				case ATVMOpcode::IntAsr:
				case ATVMOpcode::IntAsl:
				case ATVMOpcode::IVStore:
				case ATVMOpcode::ILStore:
				case ATVMOpcode::And:
				case ATVMOpcode::Or:
				case ATVMOpcode::IntLt:
				case ATVMOpcode::IntLe:
				case ATVMOpcode::IntGt:
				case ATVMOpcode::IntGe:
				case ATVMOpcode::IntEq:
				case ATVMOpcode::IntNe:
				case ATVMOpcode::Jz:
				case ATVMOpcode::Jnz:
				case ATVMOpcode::Ljz:
				case ATVMOpcode::Ljnz:
				case ATVMOpcode::Pop:
				case ATVMOpcode::ReturnInt:
					popCount = 1;
					break;

				default:
					return ReportError("Bytecode validation failed (unhandled opcode)");
			}

			while(popCount--) {
				if (stackId == 0)
					return ReportError("Bytecode validation failed (stack underflow)");

				stackId = stackStateList[stackId].mParent;
			}

			if (pushType != TypeClass::Void) {
				StackState newStackState { stackId, pushType, (uint16)(stackStateList[stackId].mDepth + 1) };

				auto r = stackStateLookup.insert(newStackState);
				if (r.second) {
					r.first->second = (sint32)stackStateList.size();
					stackStateList.push_back(newStackState);

					if (func->mStackSlotsRequired < newStackState.mDepth)
						func->mStackSlotsRequired = newStackState.mDepth;

				}

				stackId = r.first->second;
			}

			if (opcode == ATVMOpcode::Jnz || opcode == ATVMOpcode::Jz) {
				sint32 branchTarget = ip + (sint8)byteCode[ip + 1] + 2;

				traversalStack.push_back({branchTarget, stackId});
			} else if (opcode == ATVMOpcode::Ljnz || opcode == ATVMOpcode::Ljz) {
				sint32 branchTarget = ip + VDReadUnalignedLES32(&byteCode[ip + 1]) + 5;

				traversalStack.push_back({branchTarget, stackId});
			} else if (opcode == ATVMOpcode::Jmp) {
				ip += (sint8)byteCode[ip + 1] + 2;
				continue;
			} else if (opcode == ATVMOpcode::Ljmp) {
				ip += VDReadUnalignedLES32(&byteCode[ip + 1]) + 5;
				continue;
			} else if (opcode == ATVMOpcode::ReturnInt || opcode == ATVMOpcode::ReturnVoid) {
				break;
			} else if (opcode == ATVMOpcode::ILStore || opcode == ATVMOpcode::ILLoad) {
				if (byteCode[ip + 1] >= func->mLocalSlotsRequired)
					return ReportError("Bytecode validation failed (invalid local index)");
			}

			ip += ATVMGetOpcodeLength(opcode);
		}
	}

	func->mStackSlotsRequired += func->mLocalSlotsRequired;

	// encode debug info -- first, remove useless markers at the end
	while(!mByteCodeSourceRanges.empty()
		&& mByteCodeSourceRanges.back().mCodeOffset >= func->mByteCodeLen)
	{
		mByteCodeSourceRanges.pop_back();
	}

	// remove any source markers that have the same offset -- keep the last entry
	// remove any source markers that have the same line -- keep the first entry
	if (!mByteCodeSourceRanges.empty()) {
		auto begin = mByteCodeSourceRanges.begin();
		auto end = mByteCodeSourceRanges.end();
		auto dst = begin + 1;
		auto src = begin + 1;

		for(; src != end; ++src) {
			// if code offset is the same as the last, skip further checks and
			// accept the new entry
			if (dst[-1].mCodeOffset != src->mCodeOffset) {
				// if the file/line has not changed, skip this entry
				if (dst[-1].mSourceRef == src->mSourceRef)
					continue;
			}

			if (dst != src)
				*dst = *src;

			++dst;
		}

		// clone source range debug info into the function
		size_t numRanges = (size_t)(dst - begin);
		ATVMDebugSourceRange *ranges = mpDomain->mAllocator.AllocateArray<ATVMDebugSourceRange>(numRanges);
		std::copy(begin, dst, ranges);

		func->mDebugSourceRanges = vdspan(ranges, numRanges);
	}

	// clean up
	mpCurrentFunction = nullptr;

	return true;
}

bool ATVMCompiler::ParseBlock(bool& hasReturn) {
	if (++mNestingLevel >= 256)
		return ReportError("Nesting limit exceeded");

	for(;;) {
		uint32 tok = Token();
		if (tok == kTokError)
			return false;

		if (tok == '}') {
			Push(tok);
			break;
		}

		if (tok == kTokEnd)
			break;

		if (!ParseStatement(tok, hasReturn))
			return false;
	}

	--mNestingLevel;
	return true;
}

bool ATVMCompiler::ParseStatement(uint32 tok, bool& hasReturn) {
	EmitSourceReference();

	if (tok == '[') {
		tok = Token();

		bool inv = false;
		if (tok == '!') {
			tok = Token();
			inv = true;
		}

		if (tok != kTokIdentifier)
			return ReportError("Expected attribute name");

		ATVMConditionalMask condition;
		bool checkAllowed = false;

		if (mTokIdent == "debug_read") {
			condition = inv ? ATVMConditionalMask::NonDebugReadEnabled : ATVMConditionalMask::DebugReadEnabled;
			checkAllowed = true;
		} else if (mTokIdent == "debug")
			condition = inv ? ATVMConditionalMask::NonDebugEnabled : ATVMConditionalMask::DebugEnabled;
		else
			return ReportErrorF("Unrecognized attribute '%.*s'", (int)mTokIdent.size(), mTokIdent.size());

		if (Token() != ']')
			return ReportError("Expected ']' after attribute name");

		if (checkAllowed) {
			if (!((uint32)mConditionalMask & (uint32)ATVMConditionalMask::Allowed))
				return ReportError("Conditional attributes not supported in this function");
		}

		if (!((uint32)mConditionalMask & (uint32)condition)) {
			uint32 braceLevel = 0;

			for(;;) {
				tok = Token();

				if (tok == kTokError)
					return false;

				if (tok == kTokEnd)
					return ReportError("Encountered end of file while looking for end of statement");

				if (tok == '{') {
					++braceLevel;
				} else if (tok == '}') {
					--braceLevel;

					if (!braceLevel) {
						tok = Token();

						if (tok != kTokElse) {
							Push(tok);
							break;
						}
					}
				} else if (braceLevel == 0 && tok == ';')
					break;
			}

			return true;
		}

		tok = Token();
	}

	if (tok == kTokIf) {
		if (!ParseIfStatement(hasReturn))
			return false;

		return true;
	} else if (tok == kTokReturn) {
		if (!ParseReturnStatement())
			return false;

		hasReturn = true;
	} else if (tok == kTokLoop) {
		if (!ParseLoopStatement(hasReturn))
			return false;

		return true;
	} else if (tok == kTokDo) {
		if (!ParseDoWhileStatement())
			return false;
	} else if (tok == kTokWhile) {
		if (!ParseWhileStatement())
			return false;

		return true;
	} else if (tok == kTokBreak) {
		if (!ParseBreakStatement())
			return false;

		// Break ends the execution path. It won't have issued a proper return if
		// used at the top level of the function, but we won't have gotten here
		// if that's the case as it's ill-formed outside of a loop.
		hasReturn = true;
	} else if (tok == kTokContinue) {
		if (!ParseContinueStatement())
			return false;

		// Break ends the execution path. It won't have issued a proper return if
		// used at the top level of the function, but we won't have gotten here
		// if that's the case as it's ill-formed outside of a loop.
		hasReturn = true;
	} else if (tok == kTokInt) {
		if (!ParseDeclStatement(tok))
			return false;

		return true;
	} else if (tok == '{') {
		uint32 localLevel = mpCurrentFunction->mLocalSlotsRequired;

		if (!ParseBlock(hasReturn))
			return false;

		tok = Token();
		if (tok != '}')
			return ReportError("Expected '}' at end of block");

		// remove all local variables added within the scope, but do not reuse their
		// stack slots -- we currently avoid scoreboarding so as to not risk values
		// from dead locals appearing as uninitialized data in others, since we don't
		// have a proper dead store elimination pass to optimize
		if (localLevel != mpCurrentFunction->mLocalSlotsRequired) {
			for(auto it = mLocalLookup.begin(); it != mLocalLookup.end(); ) {
				if (it->second.mIndex >= localLevel)
					it = mLocalLookup.erase(it);
				else
					++it;
			}
		}

		return true;
	} else {
		if (!ParseExpressionStatement(tok))
			return false;
	}

	tok = Token();
	if (tok != ';')
		return ReportError("Expected ';' at end of statement");

	return true;
}

bool ATVMCompiler::ParseIfStatement(bool& hasReturn) {
	if (Token() != '(')
		return ReportError("Expected '(' before if condition");

	if (!ParseIntExpression())
		return false;

	if (Token() != ')')
		return ReportError("Expected ')' after if condition");

	mByteCodeBuffer.push_back((uint8)ATVMOpcode::Ljz);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);

	uint32 branchAnchor = (uint32)mByteCodeBuffer.size();

	bool trueBranchHasReturn = false;
	if (!ParseStatement(Token(), trueBranchHasReturn))
		return false;

	uint32 tok = Token();
	bool hasElse = false;
	if (tok == kTokElse)
		hasElse = true;
	else
		Push(tok);

	if (hasElse) {
		mByteCodeBuffer.push_back((uint8)ATVMOpcode::Ljmp);
		mByteCodeBuffer.push_back(0);
		mByteCodeBuffer.push_back(0);
		mByteCodeBuffer.push_back(0);
		mByteCodeBuffer.push_back(0);
	}

	uint32 branchDistance = (uint32)mByteCodeBuffer.size() - branchAnchor;
	VDWriteUnalignedLEU32(&mByteCodeBuffer[branchAnchor - 4], branchDistance);

	uint32 elseBranchAnchor = (uint32)mByteCodeBuffer.size();

	if (hasElse) {
		bool falseBranchHasReturn = false;
		if (!ParseStatement(Token(), falseBranchHasReturn))
			return false;

		uint32 elseBranchDistance = (uint32)mByteCodeBuffer.size() - elseBranchAnchor;

		VDWriteUnalignedLEU32(&mByteCodeBuffer[elseBranchAnchor - 4], elseBranchDistance);

		if (trueBranchHasReturn && falseBranchHasReturn)
			hasReturn = true;
	}

	return true;
}

bool ATVMCompiler::ParseReturnStatement() {
	uint32 tok = Token();
	if (tok == ';') {
		if (mpReturnType->mClass != ATVMTypeClass::Void)
			return ReportError("Return value required");

		Push(tok);
		mByteCodeBuffer.push_back((uint8)ATVMOpcode::ReturnVoid);
		return true;
	}

	Push(tok);

	TypeInfo returnType;
	ExprGenFn gen;
	if (!ParseExpression(returnType, gen, false))
		return false;

	ConvertToRvalue(returnType, gen);

	if (returnType.mClass != mpReturnType->mClass || (returnType.mClass == ATVMTypeClass::Object && returnType.mpObjectClass != mpReturnType->mpObjectClass))
		return ReportError("Return type mismatch");

	gen();

	if (returnType.mClass == TypeClass::Void)
		mByteCodeBuffer.push_back((uint8)ATVMOpcode::ReturnVoid);
	else if (returnType.mClass == TypeClass::Int)
		mByteCodeBuffer.push_back((uint8)ATVMOpcode::ReturnInt);
	else
		return ReportError("Cannot return expression type");

	return true;
}

bool ATVMCompiler::ParseLoopStatement(bool& hasReturn) {
	const uint32 branchAnchor = (uint32)mByteCodeBuffer.size();

	BeginLoop();

	bool loopBodyHasReturn = false;
	if (!ParseStatement(Token(), loopBodyHasReturn))
		return false;

	mByteCodeBuffer.push_back((uint8)ATVMOpcode::LoopChk);
	JumpToOffset(branchAnchor);

	const bool hadBreak = EndLoop();

	// if there is no break in the loop body and the return type is void,
	// we can mark the path as terminated
	if (!hadBreak && mpReturnType->mClass == ATVMTypeClass::Void)
		hasReturn = true;

	return true;
}

bool ATVMCompiler::ParseDoWhileStatement() {
	BeginLoop();

	const uint32 branchAnchor = (uint32)mByteCodeBuffer.size();

	bool loopBodyHasReturn = false;
	if (!ParseStatement(Token(), loopBodyHasReturn))
		return false;

	if (Token() != kTokWhile)
		return ReportError("Expected 'while' after 'do' and do-while loop body");

	if (Token() != '(')
		return ReportError("Expected '(' after 'while'");

	if (!ParseIntExpression())
		return false;

	if (Token() != ')')
		return ReportError("Expected ')' after while condition");

	JumpToOffset(ATVMOpcode::Ljnz, branchAnchor);

	EndLoop();
	return true;
}

bool ATVMCompiler::ParseWhileStatement() {
	const auto breakLabel = BeginLoop();

	if (Token() != '(')
		return ReportError("Expected '(' after 'while'");

	const uint32 branchAnchor = (uint32)mByteCodeBuffer.size();

	if (!ParseIntExpression())
		return false;

	if (Token() != ')')
		return ReportError("Expected ')' after while condition");

	mByteCodeBuffer.push_back((uint8)ATVMOpcode::Ljz);
	EmitPendingBranchTarget(breakLabel);

	bool loopBodyHasReturn = false;
	if (!ParseStatement(Token(), loopBodyHasReturn))
		return false;

	JumpToOffset(branchAnchor);

	EndLoop();
	return true;
}

bool ATVMCompiler::ParseBreakStatement() {
	mByteCodeBuffer.push_back((uint8)ATVMOpcode::Ljmp);
	return EmitBreakTarget();
}

bool ATVMCompiler::ParseContinueStatement() {
	if (mContinueTargetLabels.empty())
		return ReportError("No loop for continue statement");

	mByteCodeBuffer.push_back((uint8)ATVMOpcode::Ljmp);
	return EmitPendingBranchTarget(mContinueTargetLabels.back());
}

bool ATVMCompiler::ParseDeclStatement(uint32 typeToken) {
	VDASSERT(typeToken == kTokInt);

	for(;;) {
		uint32 tok = Token();

		if (tok != kTokIdentifier)
			return ReportError("Expected variable name");

		auto ins = mLocalLookup.insert_as(mTokIdent);
		if (!ins.second)
			return ReportErrorF("Local variable '%.*s' already declared", (int)mTokIdent.size(), mTokIdent.data());

		uint32 varIndex = mpCurrentFunction->mLocalSlotsRequired;
		if (varIndex >= 0x100)
			return ReportError("Local variable limit exceeded");

		ins.first->second = ATVMTypeInfo { ATVMTypeClass::IntLValueLocal, varIndex };
		++mpCurrentFunction->mLocalSlotsRequired;

		tok = Token();

		// check for initializer
		if (tok == '=') {
			if (!ParseIntExpression())
				return false;

			mByteCodeBuffer.push_back((uint8)ATVMOpcode::ILStore);
			mByteCodeBuffer.push_back((uint8)varIndex);

			tok = Token();
		}

		if (tok == ';')
			break;

		if (tok != ',')
			return ReportError("Expected ';' or ',' after variable name");
	}

	return true;
}

bool ATVMCompiler::ParseExpressionStatement(uint32 tok) {
	Push(tok);

	TypeInfo returnType;
	ExprGenFn gen;
	if (!ParseExpression(returnType, gen, false))
		return false;

	if (gen)
		gen();

	// discard rvalues, and don't bother doing lvalue conversion as we don't need it on the stack
	if (returnType.mClass == TypeClass::Int || returnType.mClass == TypeClass::Object)
		mByteCodeBuffer.push_back((uint8)ATVMOpcode::Pop);

	return true;
}

// dataValue := dataObject | dataArray | expr
// dataValues = dataValue | dataValue ',' dataValues
// dataArray := '[' dataValues? ']'
// dataObject := '{' dataMembers? '}'
// dataMembers := dataMember | dataMember ',' dataMembers
// dataMember := identifier ':' dataValue

bool ATVMCompiler::ParseDataValue(ATVMDataValue& value) {
	uint32 tok = Token();

	value.mSrcOffset = (uint32)(mpSrc - mpSrc0);

	if (tok == '[') {
		ATVMDataValue *array = nullptr;
		vdfastvector<ATVMDataValue> arrayElements;

		for(;;) {
			tok = Token();
			if (tok == ']')
				break;

			Push(tok);
			arrayElements.push_back();

			if (!ParseDataValue(arrayElements.back()))
				return false;

			tok = Token();
			if (tok == ']')
				break;

			if (tok != ',')
				return ReportError("Expected ',' or ']' after data array element");
		}

		array = mpDomain->mAllocator.AllocateArray<ATVMDataValue>(arrayElements.size());

		std::copy(arrayElements.begin(), arrayElements.end(), array);

		value.mType = ATVMDataType::Array;
		value.mLength = (uint32)arrayElements.size();
		value.mpArrayElements = array;
	} else if (tok == '{') {
		ATVMDataMember *members = nullptr;

		struct ATVMDataMemberHash {
			size_t operator()(const ATVMDataMember& m) const {
				return m.mNameHash;
			}
		};

		struct ATVMDataMemberPred {
			bool operator()(const ATVMDataMember& a, const ATVMDataMember& b) const {
				return a.mNameHash == b.mNameHash && !strcmp(a.mpName, b.mpName);
			}
		};

		std::unordered_set<ATVMDataMember, ATVMDataMemberHash, ATVMDataMemberPred> memberElements;

		for(;;) {
			tok = Token();

			if (tok == '}')
				break;

			if (tok != kTokIdentifier)
				return ReportError("Expected data member name");

			tok = Token();
			if (tok != ':')
				return ReportError("Expected ':' after data member name");

			ATVMDataMember mb {};
			size_t nameLen = mTokIdent.size();
			mb.mNameHash = VDHashString32(mTokIdent.data(), nameLen);
			char *name = (char *)mpDomain->mAllocator.Allocate(nameLen + 1); 
			mb.mpName = name;
			memcpy(name, mTokIdent.data(), nameLen);
			name[nameLen] = 0;

			if (!ParseDataValue(mb.mValue))
				return false;

			if (!memberElements.insert(mb).second)
				return ReportErrorF("Member '%s' has already been defined in this object", name);

			tok = Token();
			if (tok == '}')
				break;

			if (tok != ',')
				return ReportErrorF("Expected ',' or '}' after data object member '%s'", name);
		}

		members = mpDomain->mAllocator.AllocateArray<ATVMDataMember>(memberElements.size());
		std::copy(memberElements.begin(), memberElements.end(), members);

		value.mType = ATVMDataType::DataObject;
		value.mLength = (uint32)memberElements.size();
		value.mpObjectMembers = members;
	} else if (tok == kTokFunction) {
		if (!ParseInlineScript(value))
			return false;
	} else {
		Push(tok);

		ATVMTypeInfo valueType;
		if (!ParseConstantExpression(valueType))
			return false;

		if (valueType.mClass == TypeClass::IntConst) {
			value.mType = ATVMDataType::Int;
			value.mLength = 0;
			value.mIntValue = (sint32)valueType.mIndex;
		} else if (valueType.mClass == TypeClass::StringConst) {
			value.mType = ATVMDataType::String;
			value.mLength = 0;
			value.mpStrValue = static_cast<const ATVMStringObject *>(mpDomain->mGlobalObjects[valueType.mIndex])->c_str();
		} else if (valueType.mClass == TypeClass::ObjectLValue) {
			value.mType = ATVMDataType::RuntimeObject;
			value.mLength = mpDomain->mGlobalVariables[valueType.mIndex];
			value.mpRuntimeObjectClass = valueType.mpObjectClass;
		} else
			return ReportError("Cannot use this type in a data object");
	}

	return true;
}

bool ATVMCompiler::ParseConstantExpression(TypeInfo& returnType) {
	ExprGenFn gen;
	return ParseExpression(returnType, gen, true);
}

bool ATVMCompiler::ParseIntExpression() {
	TypeInfo returnType;
	ExprGenFn gen;
	if (!ParseExpression(returnType, gen, false))
		return false;

	ConvertToRvalue(returnType, gen);

	if (gen)
		gen();

	if (returnType.mClass != TypeClass::Int)
		return ReportError("Integer expression required");

	return true;
}

enum class ATVMCompiler::ExprOp : uint8 {
	None,
	Comma,
	Assign,
	AddEq,
	SubEq,
	MulEq,
	DivEq,
	ModEq,
	AndEq,
	OrEq,
	XorEq,
	ShlEq,
	ShrEq,
	LogOr,
	LogAnd,
	Eq,
	Ne,
	Lt,
	Le,
	Gt,
	Ge,
	BitAnd,
	BitOr,
	BitXor,
	Shl,
	Shr,
	Add,
	Sub,
	Mul,
	Div,
	Mod,
	Plus,
	Minus,
	BitNot,
	LogNot,
	OpenPar,
	ClosePar
};

bool ATVMCompiler::ParseExpression(TypeInfo& returnType, ExprGenFn& gen, bool requireConst) {
	using Op = ExprOp;

	enum class OpArgType : uint8 {
		IntLR,	// lvalue <op> rvalue
		IntLRA,	// lvalue <op>= rvalue
		IntRR,	// rvalue <op> rvalue
		IntR,	// <op> rvalue
		Other
	};

	struct OpInfo {
		uint8 mPrecOn;
		uint8 mPrecOff;
		OpArgType mArgType;
	};

	static constexpr auto kOpInfoTable =
		[] {
			VDCxArray<OpInfo, (size_t)Op::ClosePar + 1> opInfo {};

			// A new operator forces a reduce if its prec-on is greater than
			// the TOS operator's prec-off, and shifts if less or equal.
			//
			// Thus, prec-on == prec-off + 1 means left associative, and prec-on ==
			// prec-off means right associative. Unary operators must be right
			// associative.
			//
			opInfo[(size_t)Op::Comma	] = {  2,  2, OpArgType::Other	};
			opInfo[(size_t)Op::OpenPar	] = {  3,  3, OpArgType::Other	};
			opInfo[(size_t)Op::ClosePar	] = {  2,  2, OpArgType::Other	};
			opInfo[(size_t)Op::Assign	] = {  4,  4, OpArgType::IntLR	};
			opInfo[(size_t)Op::AddEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::SubEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::MulEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::DivEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::ModEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::AndEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::OrEq		] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::XorEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::ShlEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::ShrEq	] = {  4,  4, OpArgType::IntLRA	};
			opInfo[(size_t)Op::LogOr	] = {  7,  6, OpArgType::IntRR	};
			opInfo[(size_t)Op::LogAnd	] = {  9,  8, OpArgType::IntRR	};
			opInfo[(size_t)Op::Eq		] = { 11, 10, OpArgType::IntRR	};
			opInfo[(size_t)Op::Ne		] = { 11, 10, OpArgType::IntRR	};
			opInfo[(size_t)Op::Lt		] = { 13, 12, OpArgType::IntRR	};
			opInfo[(size_t)Op::Le		] = { 13, 12, OpArgType::IntRR	};
			opInfo[(size_t)Op::Gt		] = { 13, 12, OpArgType::IntRR	};
			opInfo[(size_t)Op::Ge		] = { 13, 12, OpArgType::IntRR	};
			opInfo[(size_t)Op::BitOr	] = { 15, 14, OpArgType::IntRR	};
			opInfo[(size_t)Op::BitXor	] = { 17, 16, OpArgType::IntRR	};
			opInfo[(size_t)Op::BitAnd	] = { 19, 18, OpArgType::IntRR	};
			opInfo[(size_t)Op::Shl		] = { 21, 20, OpArgType::IntRR	};
			opInfo[(size_t)Op::Shr		] = { 21, 20, OpArgType::IntRR	};
			opInfo[(size_t)Op::Add		] = { 23, 22, OpArgType::IntRR	};
			opInfo[(size_t)Op::Sub		] = { 23, 22, OpArgType::IntRR	};
			opInfo[(size_t)Op::Mul		] = { 25, 24, OpArgType::IntRR	};
			opInfo[(size_t)Op::Div		] = { 25, 24, OpArgType::IntRR	};
			opInfo[(size_t)Op::Mod		] = { 25, 24, OpArgType::IntRR	};
			opInfo[(size_t)Op::Plus		] = { 26, 26, OpArgType::IntR	};
			opInfo[(size_t)Op::Minus	] = { 26, 26, OpArgType::IntR	};
			opInfo[(size_t)Op::BitNot	] = { 26, 26, OpArgType::IntR	};
			opInfo[(size_t)Op::LogNot	] = { 26, 26, OpArgType::IntR	};

			return opInfo;
		}();

	vdfastvector<Op> opStack;
	vdfastvector<ATVMTypeInfo> typeStack;
	vdvector<vdfunction<void()>> genStack;

	bool needValue = true;

	for(;;) {
		Op op = Op::None;
		int tok = Token();

		if (needValue) {
			if (tok == '+')
				op = Op::Plus;
			else if (tok == '-')
				op = Op::Minus;
			else if (tok == '~')
				op = Op::BitNot;
			else if (tok == '!')
				op = Op::LogNot;
			else if (tok == '(')
				op = Op::OpenPar;
			else {
				typeStack.emplace_back();
				genStack.emplace_back();

				Push(tok);
				if (!ParsePostfixExpression(typeStack.back(), genStack.back()))
					return false;

				needValue = false;
				continue;
			}

			// handle unary operator
			opStack.push_back(op);
		} else {
			// parse operator(s)
			switch(tok) {
//				case ',': op = Op::Comma; break;
				case '=': op = Op::Assign; break;
				case kTokLogicalOr: op = Op::LogOr; break;
				case kTokLogicalAnd: op = Op::LogAnd; break;
				case kTokEq: op = Op::Eq; break;
				case kTokNe: op = Op::Ne; break;
				case '<': op = Op::Lt; break;
				case '>': op = Op::Gt; break;
				case kTokLe: op = Op::Le; break;
				case kTokGe: op = Op::Ge; break;
				case '|': op = Op::BitOr; break;
				case '^': op = Op::BitXor; break;
				case '&': op = Op::BitAnd; break;
				case kTokLeftShift: op = Op::Shl; break;
				case kTokRightShift: op = Op::Shr; break;
				case '+': op = Op::Add; break;
				case '-': op = Op::Sub; break;
				case '*': op = Op::Mul; break;
				case '/': op = Op::Div; break;
				case '%': op = Op::Mod; break;
				case ')': op = Op::ClosePar; break;
				case kTokAddEq: op = Op::AddEq; break;
				case kTokSubEq: op = Op::SubEq; break;
				case kTokMulEq: op = Op::MulEq; break;
				case kTokDivEq: op = Op::DivEq; break;
				case kTokModEq: op = Op::ModEq; break;
				case kTokAndEq: op = Op::AndEq; break;
				case kTokOrEq: op = Op::OrEq; break;
				case kTokXorEq: op = Op::XorEq; break;
				case kTokShlEq: op = Op::ShlEq; break;
				case kTokShrEq: op = Op::ShrEq; break;
				default:
					Push(tok);
					break;
			}

			// get op info
			const OpInfo& opInfo = kOpInfoTable[(size_t)op];

			// reduce
			bool closeParensOk = false;

			while(!opStack.empty()) {
				Op reduceOp = opStack.back();
				const OpInfo& reduceOpInfo = kOpInfoTable[(size_t)reduceOp];

				if (opInfo.mPrecOn > reduceOpInfo.mPrecOff)
					break;

				opStack.pop_back();
				
				// open parens can only be reduced by a close parens, otherwise
				// we have a dangling parens
				if (reduceOp == Op::OpenPar) {
					if (op != Op::ClosePar)
						return ReportError("unmatched '(' in expression");

					closeParensOk = true;
					break;
				}

				switch(reduceOpInfo.mArgType) {
					case OpArgType::IntLRA:
					case OpArgType::IntLR:
						switch(typeStack.end()[-2].mClass) {
							case ATVMTypeClass::IntLValueLocal:
							case ATVMTypeClass::IntLValueVariable:
								break;

							default:
								return ReportError("assignment operator requires modifiable variable on left side");
						}

						ConvertToRvalue(typeStack.back(), genStack.back());

						// right side must be int
						if (typeStack.back().mClass != ATVMTypeClass::Int)
							return ReportError("binary operator requires integer type on right side");

						if (reduceOpInfo.mArgType == OpArgType::IntLRA) {
							// fused assign-op
							Op arithOp {};

							switch(reduceOp) {
								case Op::AddEq:	arithOp = Op::Add; break;
								case Op::SubEq:	arithOp = Op::Sub; break;
								case Op::MulEq:	arithOp = Op::Mul; break;
								case Op::DivEq:	arithOp = Op::Div; break;
								case Op::ModEq:	arithOp = Op::Mod; break;
								case Op::AndEq:	arithOp = Op::BitAnd; break;
								case Op::OrEq:	arithOp = Op::BitOr; break;
								case Op::XorEq:	arithOp = Op::BitXor; break;
								case Op::ShlEq:	arithOp = Op::Shl; break;
								case Op::ShrEq:	arithOp = Op::Shr; break;
								default:
									VDFAIL("Invalid assignment op");
									break;
							}

							// generate read of destination
							ATVMTypeInfo arithDestType = typeStack.end()[-2];
							ExprGenFn arithDestGen;

							ConvertToRvalue(arithDestType, arithDestGen);

							// compile binary arith op
							if (!ReduceBinary(arithOp, arithDestType, typeStack.back(), arithDestGen, genStack.back()))
								return false;

							// replace rhs with arith op result
							typeStack.back() = std::move(arithDestType);
							genStack.back() = std::move(arithDestGen);

							reduceOp = Op::Assign;
						}

						// plain assignment
						if (!ReduceBinaryAssignment(reduceOp, typeStack.end()[-2], typeStack.back(), genStack.end()[-2], genStack.back()))
							return false;

						typeStack.pop_back();
						genStack.pop_back();
						break;

					case OpArgType::IntRR: {
						ATVMTypeInfo& typex = typeStack.end()[-2];
						const ATVMTypeInfo& typey = typeStack.back();

						// if both arguments are constant, evaluate as const
						if (typex.mClass == ATVMTypeClass::IntConst && typey.mClass == ATVMTypeClass::IntConst) {
							if (!ReduceBinaryIntConst(reduceOp, typex, typey))
								return false;
						} else {
							ConvertToRvalue(typex, genStack.end()[-2]);
							ConvertToRvalue(typeStack.back(), genStack.back());

							if (!ReduceBinary(reduceOp, typex, typey, genStack.end()[-2], genStack.back()))
								return false;
						}

						typeStack.pop_back();
						genStack.pop_back();
						break;
					}

					case OpArgType::IntR: {
						ATVMTypeInfo& typex = typeStack.back();

						if (typex.mClass == ATVMTypeClass::IntConst) {
							ReduceUnaryIntConst(reduceOp, typex);
						} else {
							ConvertToRvalue(typex, genStack.back());

							if (typex.mClass != ATVMTypeClass::Int)
								return ReportError("Unary operator requires integer type on right side");

							if (!ReduceUnary(reduceOp, typex, genStack.back()))
								return false;
						}
						break;
					}

					case OpArgType::Other:
						break;
				}
			}

			VDASSERT(typeStack.back().mClass != ATVMTypeClass::Int || genStack.back());

			if (op == Op::ClosePar) {
				if (!closeParensOk) {
					Push(tok);
					break;
				} else
					continue;
			}

			if (op == Op::None)
				break;

			opStack.push_back(op);
			needValue = true;
		}
	}

	if (genStack.size() != 1 || typeStack.size() != 1)
		return ReportError("Internal compile error: incorrect stack level during expression evaluation");

	gen = std::move(genStack.back());
	if (gen) {
		if (requireConst)
			return ReportError("Constant expression required");
	}

	returnType = typeStack.back();
	return true;
}

bool ATVMCompiler::ReduceUnary(ExprOp op, ATVMTypeInfo& x, ExprGenFn& genx) {
	ATVMOpcode opcode {};

	switch(op) {
		case ExprOp::Plus:
			// no codegen needed, just a type check
			return true;

		case ExprOp::Minus:
			opcode = ATVMOpcode::IntNeg;
			break;

		case ExprOp::BitNot:
			opcode = ATVMOpcode::IntNot;
			break;

		case ExprOp::LogNot:
			opcode = ATVMOpcode::Not;
			break;

		default:
			return ReportError("Internal compiler error: invalid unary op");
	}

	genx = [genx, opcode, this] {
		genx();

		mByteCodeBuffer.push_back((uint8)opcode);
	};

	return true;
}

bool ATVMCompiler::ReduceUnaryIntConst(ExprOp op, ATVMTypeInfo& x) {
	sint32 xv = (sint32)x.mIndex;

	switch(op) {
		case ExprOp::Plus:
			break;

		case ExprOp::Minus:
			xv = (sint32)((uint32)0 - (uint32)xv);
			break;

		case ExprOp::BitNot:
			xv = ~xv;
			break;

		case ExprOp::LogNot:
			xv = !xv;
			break;

		default:
			return ReportError("Internal compiler error: invalid unary op");
	}

	x.mIndex = (uint32)xv;
	return true;
}

bool ATVMCompiler::ReduceBinary(ExprOp op, ATVMTypeInfo& x, const ATVMTypeInfo& y, ExprGenFn& genx, const ExprGenFn& geny) {
	if (op == ExprOp::LogOr) {
		genx = [genx, geny, this] {
			// generate x
			genx();

			// duplicate x
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::Dup);

			// skip if non-zero
			const auto takeFirst = AllocLabel();

			mByteCodeBuffer.push_back((uint8)ATVMOpcode::Ljnz);
			EmitPendingBranchTarget(takeFirst);

			// remove x
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::Pop);

			// generate y
			geny();
	
			// patch branch
			PatchLabel(takeFirst);
		};

		return true;
	}
	
	if (op == ExprOp::LogAnd) {
		genx = [genx, geny, this] {
			// generate x
			genx();

			// duplicate x
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::Dup);

			// skip if zero
			const auto takeFirst = AllocLabel();

			mByteCodeBuffer.push_back((uint8)ATVMOpcode::Ljz);
			EmitPendingBranchTarget(takeFirst);

			// remove x
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::Pop);

			// generate y
			geny();
	
			// patch branch
			PatchLabel(takeFirst);
		};

		return true;
	}

	ATVMOpcode opcode {};

	switch(op) {
		case ExprOp::Eq:		opcode = ATVMOpcode::IntEq; break;
		case ExprOp::Ne:		opcode = ATVMOpcode::IntNe; break;
		case ExprOp::Lt:		opcode = ATVMOpcode::IntLt; break;
		case ExprOp::Le:		opcode = ATVMOpcode::IntLe; break;
		case ExprOp::Gt:		opcode = ATVMOpcode::IntGt; break;
		case ExprOp::Ge:		opcode = ATVMOpcode::IntGe; break;
		case ExprOp::BitAnd:	opcode = ATVMOpcode::IntAnd; break;
		case ExprOp::BitOr:		opcode = ATVMOpcode::IntOr; break;
		case ExprOp::BitXor:	opcode = ATVMOpcode::IntXor; break;
		case ExprOp::Shl:		opcode = ATVMOpcode::IntAsl; break;
		case ExprOp::Shr:		opcode = ATVMOpcode::IntAsr; break;
		case ExprOp::Add:		opcode = ATVMOpcode::IntAdd; break;
		case ExprOp::Sub:		opcode = ATVMOpcode::IntSub; break;
		case ExprOp::Mul:		opcode = ATVMOpcode::IntMul; break;
		case ExprOp::Div:		opcode = ATVMOpcode::IntDiv; break;
		case ExprOp::Mod:		opcode = ATVMOpcode::IntMod; break;

		default:
			return ReportError("Internal compiler error: invalid binary op");
	}

	auto fn = [genx = std::move(genx), geny, opcode, this] {
		genx();
		geny();

		mByteCodeBuffer.push_back((uint8)opcode);
	};

	genx = std::move(fn);

	return true;
}

bool ATVMCompiler::ReduceBinaryIntConst(ExprOp op, ATVMTypeInfo& x, const ATVMTypeInfo& y) {
	sint32 xv = (sint32)x.mIndex;
	sint32 yv = (sint32)y.mIndex;

	switch(op) {
		case ExprOp::LogAnd:	xv = xv && yv; break;
		case ExprOp::LogOr:		xv = xv || yv; break;
		case ExprOp::Eq:		xv = (xv == yv) ? 1 : 0; break;
		case ExprOp::Ne:		xv = (xv != yv) ? 1 : 0; break;
		case ExprOp::Lt:		xv = (xv <  yv) ? 1 : 0; break;
		case ExprOp::Le:		xv = (xv <= yv) ? 1 : 0; break;
		case ExprOp::Gt:		xv = (xv >  yv) ? 1 : 0; break;
		case ExprOp::Ge:		xv = (xv >= yv) ? 1 : 0; break;
		case ExprOp::BitAnd:	xv = xv & yv; break;
		case ExprOp::BitOr:		xv = xv | yv; break;
		case ExprOp::BitXor:	xv = xv ^ yv; break;
		case ExprOp::Shl:		xv = xv << (yv & 31); break;
		case ExprOp::Shr:		xv = xv >> (yv & 31); break;
		case ExprOp::Add:		xv = (sint32)((uint32)xv + (uint32)yv); break;
		case ExprOp::Sub:		xv = (sint32)((uint32)xv - (uint32)yv); break;

		case ExprOp::Mul:
			xv = (sint32)((sint64)xv * yv);
			break;

		case ExprOp::Div:
			if (yv == 0)
				return ReportError("Division by zero");

			if (yv == -1)
				return (sint32)((uint32)0 - (uint32)xv);

			xv /= yv;
			break;

		case ExprOp::Mod:
			if (yv == 0)
				return ReportError("Division by zero");
			break;

		default:
			return ReportError("Internal compiler error: invalid binary op");
	}

	x = ATVMTypeInfo { ATVMTypeClass::IntConst, (uint32)xv };
	return true;
}

bool ATVMCompiler::ReduceBinaryAssignment(ExprOp op, ATVMTypeInfo& x, const ATVMTypeInfo& y, ExprGenFn& genx, const ExprGenFn& geny) {
	if (op != ExprOp::Assign)
		return ReportError("Internal compiler error: invalid binary assignment op");

	ATVMOpcode storeOpcode {};

	if (x.mClass == TypeClass::IntLValueVariable)
		storeOpcode = ATVMOpcode::IVStore;
	else
		storeOpcode = ATVMOpcode::ILStore;

	VDASSERT(!genx);
	VDASSERT(geny);

	genx = [geny, storeOpcode, storeIndex = (uint8)x.mIndex, this] {
		geny();

		mByteCodeBuffer.push_back((uint8)storeOpcode);
		mByteCodeBuffer.push_back(storeIndex);
	};
	
	x = TypeInfo { TypeClass::Void };
	return true;
}

bool ATVMCompiler::ParsePostfixExpression(TypeInfo& returnType, ExprGenFn& gen) {
	ATVMTypeInfo objectType;
	if (!ParseValue(objectType, gen))
		return false;

	uint32 tok = Token();

	if (tok == kTokIncrement || tok == kTokDecrement)
		return ReportError("Postincrement/decrement operators not supported");

	if (tok != '.') {
		Push(tok);

		returnType = objectType;
		return true;
	}

	ConvertToRvalue(objectType, gen);

	if (objectType.mClass != TypeClass::Object && objectType.mClass != TypeClass::ObjectClass)
		return ReportError("'.' operator can only be used on object");

	tok = Token();
	if (tok != kTokIdentifier)
		return ReportError("Expected method name after '.' operator");

	const ATVMExternalMethod *foundMethod = nullptr;
	for(const ATVMExternalMethod& method : objectType.mpObjectClass->mMethods) {
		if (mTokIdent == method.mpName) {
			foundMethod = &method;
			break;
		}
	}

	if (!foundMethod)
		return ReportErrorF("Class '%s' does not have method called '%.*s'", objectType.mpObjectClass->mpClassName, (int)mTokIdent.size(), mTokIdent.data());

	const bool isStaticCall = (objectType.mClass == TypeClass::ObjectClass);
	if ((foundMethod->mFlags & ATVMFunctionFlags::Static) != ATVMFunctionFlags::None) {
		if (!isStaticCall)
			return ReportErrorF("Static method '%.*s' must be called on a class instance", (int)mTokIdent.size(), mTokIdent.data());
	} else {
		if (isStaticCall)
			return ReportErrorF("Instance method '%.*s' must be called on an object instance", (int)mTokIdent.size(), mTokIdent.data());
	}

	if (!mbUnsafeCallsAllowed && (foundMethod->mFlags & ATVMFunctionFlags::Unsafe) != ATVMFunctionFlags::None)
		return ReportErrorF("Method '%s' is potentially unsafe, but unsafe calls have not been allowed", foundMethod->mpName);

	ATVMFunctionFlags funcAsyncMask = foundMethod->mFlags & ATVMFunctionFlags::AsyncAll;

	if (funcAsyncMask != ATVMFunctionFlags::None) {
		if ((~mAsyncAllowedMask & funcAsyncMask) != ATVMFunctionFlags::None) {
			if (mAsyncAllowedMask != ATVMFunctionFlags::None)
				return ReportErrorF("Cannot call '%.*s' as it can suspend in a mode not supported by the current context", (int)mTokIdent.size(), mTokIdent.data());
			else
				return ReportErrorF("Cannot call '%.*s' as it can suspend, which is not supported by the current context", (int)mTokIdent.size(), mTokIdent.data());
		}

		mpCurrentFunctionInfo->mFlags |= funcAsyncMask;
	}

	tok = Token();
	if (tok != '(')
		return ReportError("Expected '(' after method name");

	uint32 argCount = 0;

	vdvector<ExprGenFn> argGens;

	tok = Token();
	if (tok != ')') {
		Push(tok);

		for(;;) {
			TypeInfo argType;
			ExprGenFn gen;
			if (!ParseExpression(argType, gen, false))
				return false;

			ConvertToRvalue(argType, gen);
			++argCount;

			if (argCount <= foundMethod->mNumArgs && *foundMethod->mpTypes[argCount] != argType)
				return ReportErrorF("Argument type mismatch on argument %u", argCount);

			argGens.emplace_back(std::move(gen));

			tok = Token();

			if (tok == ')')
				break;

			if (tok != ',')
				return ReportError("Expected ',' or ')' after method argument");
		}
	}

	if (argCount != foundMethod->mNumArgs)
		return ReportErrorF("Method %s.%s() expects %u arguments, %u provided", objectType.mpObjectClass->mpClassName, foundMethod->mpName, foundMethod->mNumArgs, argCount);

	if (mMethodPtrs.size() >= 256)
		return ReportError("External method call limit exceeded");

	const bool isIntCall = foundMethod->mpTypes[0]->mClass == ATVMTypeClass::Int;
	const auto methodPtr = isIntCall ? (void(*)())foundMethod->mpIntMethod : (void(*)())foundMethod->mpVoidMethod;
	ATVMOpcode opcode = 
		isIntCall
			? isStaticCall ? ATVMOpcode::StaticMethodCallInt : ATVMOpcode::MethodCallInt
			: isStaticCall ? ATVMOpcode::StaticMethodCallVoid : ATVMOpcode::MethodCallVoid;

	gen = [
		gen = std::move(gen),
		argGens = std::move(argGens),
		argCount,
		isIntCall,
		methodPtr,
		opcode,
		this
	] {
		// may be null for static methods
		if (gen)
			gen();

		for(const ExprGenFn& fn : argGens)
			fn();

		mMethodPtrs.push_back(methodPtr);

		mByteCodeBuffer.push_back((uint8)opcode);
		mByteCodeBuffer.push_back((uint8)argCount);
		mByteCodeBuffer.push_back((uint8)(mMethodPtrs.size() - 1));
	};

	returnType = *foundMethod->mpTypes[0];
	return true;
}

bool ATVMCompiler::ParseValue(TypeInfo& returnType, ExprGenFn& gen) {
	uint32 tok = Token();
	if (tok == kTokIdentifier) {
		if (auto it = mLocalLookup.find_as(mTokIdent); it != mLocalLookup.end()) {
			// local variable reference
			returnType = it->second;
		} else if (auto it = mVariableLookup.find_as(mTokIdent); it != mVariableLookup.end()) {
			// variable reference
			returnType = it->second;
		} else if (auto it = mClassLookup.find_as(mTokIdent); it != mClassLookup.end()) {
			// class reference
			returnType = it->second.mTypeInfo;
		} else if (auto it = mFunctionLookup.find_as(mTokIdent); it != mFunctionLookup.end()) {
			// function reference -- check if it is a call
			const FunctionInfo& fi = *it->second;
			const ATVMFunction& func = *mpDomain->mFunctions[fi.mFunctionIndex];

			tok = Token();
			if (tok != '(') {
				// no function call, we are returning a function pointer
				Push(tok);

				gen = [idx = fi.mFunctionIndex, this] {
					LoadConst(idx);
				};

				returnType = ATVMTypeInfo { ATVMTypeClass::FunctionPointer, GetFunctionPointerTypeIndex(vdvector_view(&func.mReturnType, 1)) };
				return true;
			}

			// we have a function call -- check for arguments (currently we don't support any)
			tok = Token();
			if (tok != ')')
				return ReportError("Expected ')' after function name");

			ATVMFunctionFlags funcAsyncMask = fi.mFlags & ATVMFunctionFlags::AsyncAll;

			if (funcAsyncMask != ATVMFunctionFlags::None) {
				if ((~mAsyncAllowedMask & funcAsyncMask) != ATVMFunctionFlags::None) {
					if (mAsyncAllowedMask != ATVMFunctionFlags::None)
						return ReportErrorF("Cannot call '%.*s' as it can suspend in a mode not supported by the current context", (int)mTokIdent.size(), mTokIdent.data());
					else
						return ReportErrorF("Cannot call '%.*s' as it can suspend, which is not supported by the current context", (int)mTokIdent.size(), mTokIdent.data());
				}

				mpCurrentFunctionInfo->mFlags |= funcAsyncMask;
			}

			mFunctionDependencies.emplace(mpCurrentFunctionInfo->mFunctionIndex, fi.mFunctionIndex);

			ATVMOpcode opcode;
			if (func.mReturnType.mClass == ATVMTypeClass::Int)
				opcode = ATVMOpcode::FunctionCallInt;
			else
				opcode = ATVMOpcode::FunctionCallVoid;

			gen = [opcode, idx = fi.mFunctionIndex, this] {
				mByteCodeBuffer.push_back((uint8)opcode);
				mByteCodeBuffer.push_back((uint8)0);
				mByteCodeBuffer.push_back((uint8)idx);
			};

			returnType = func.mReturnType;
		} else
			return ReportErrorF("Unknown variable or function '%.*s'", (int)mTokIdent.size(), mTokIdent.data());

	} else if (tok == kTokSpecialIdent) {
		auto it = mSpecialVariableLookup.find(mTokIdent);
		ATVMOpcode opcode {};

		if (it == mSpecialVariableLookup.end()) {
			it = mThreadVariableLookup.find(mTokIdent);
			if (it == mThreadVariableLookup.end())
				return ReportErrorF("Unknown special variable '$%.*s'", (int)mTokIdent.size(), mTokIdent.data());

			opcode = ATVMOpcode::ITLoad;
		} else {
			if (mSpecialVariablesReferenced.size() <= it->second.mIndex)
				mSpecialVariablesReferenced.resize(it->second.mIndex + 1, false);

			mSpecialVariablesReferenced[it->second.mIndex] = true;

			opcode = ATVMOpcode::ISLoad;
		}
		
		gen = [idx = (uint8)it->second.mIndex, opcode, this] {
			mByteCodeBuffer.push_back((uint8)opcode);
			mByteCodeBuffer.push_back(idx);
		};

		if (it->second.mClass == ATVMTypeClass::ObjectLValue)
			returnType = TypeInfo { TypeClass::Object, 0, it->second.mpObjectClass };
		else
			returnType = TypeInfo { TypeClass::Int };
	} else if (tok == kTokInteger || tok == kTokStringLiteral || tok == kTokTrue || tok == kTokFalse || tok == kTokFunction) {
		Push(tok);
		return ParseConstantValue(returnType);
	} else
		return ReportError("Expected expression value");

	return true;
}

bool ATVMCompiler::ParseConstantValue(TypeInfo& returnType) {
	uint32 tok = Token();

	if (tok == kTokInteger)
		returnType = TypeInfo { TypeClass::IntConst, (uint32)mTokValue };
	else if (tok == kTokTrue)
		returnType = TypeInfo { TypeClass::IntConst, 1 };
	else if (tok == kTokFalse)
		returnType = TypeInfo { TypeClass::IntConst, 0 };
	else if (tok == kTokStringLiteral) {
		const uint32 id = (uint32)mpDomain->mGlobalObjects.size();

		for(const char c : mTokIdent) {
			if (c < 0x20 || c >= 0x7f)
				return ReportError("String literals can only contain printable ASCII characters");
		}

		size_t len = mTokIdent.size();
		ATVMStringObject *sobj = (ATVMStringObject *)mpDomain->mAllocator.Allocate(sizeof(ATVMStringObject) + len + 1);
		char *s = (char *)(sobj + 1);

		memcpy(s, mTokIdent.data(), len);
		s[len] = 0;

		mpDomain->mGlobalObjects.push_back(sobj);

		returnType = TypeInfo { TypeClass::StringConst, id };
	} else if (tok == kTokIdentifier) {
		auto it = mVariableLookup.find_as(mTokIdent);
		
		if (it == mVariableLookup.end())
			return ReportErrorF("Unknown variable '%.*s'", (int)mTokIdent.size(), mTokIdent.data());

		// variable reference
		returnType = it->second;

		if (returnType.mClass != ATVMTypeClass::ObjectLValue)
			return ReportError("Expected constant value");
	} else if (tok == kTokFunction) {
		ATVMDataValue value;
		if (!ParseInlineScript(value))
			return false;

		const ATVMFunction *func = DeferCompile(kATVMTypeVoid, *value.mpScript, ATVMFunctionFlags::AsyncAll);
		mpDomain->mFunctions.push_back(func);

		LoadConst((sint32)(mpDomain->mFunctions.size() - 1));

		returnType = ATVMTypeInfo { ATVMTypeClass::FunctionPointer, 0 };
	} else
		return ReportError("Expected constant value");

	return true;
}

bool ATVMCompiler::ParseInlineScript(ATVMDataValue& value) {
	if (Token() != '{')
		return ReportError("Expected '{' after 'function' for inline script expression");

	ATVMScriptFragment *fragment = mpDomain->mAllocator.Allocate<ATVMScriptFragment>();

	fragment->mpSrc = mpSrc;

	uint32 nestingLevel = 1;
	const char *p;

	for(;;) {
		p = mpSrc;
		uint32 tok = Token();

		if (tok == kTokEnd)
			return ReportError("End of file encountered while parsing inline function");

		if (tok == '{')
			++nestingLevel;
		else if (tok == '}') {
			if (!--nestingLevel)
				break;
		}
	}

	fragment->mSrcLength = (size_t)(p - fragment->mpSrc);

	value.mType = ATVMDataType::Script;
	value.mLength = 0;
	value.mpScript = fragment;
	return true;
}

void ATVMCompiler::ConvertToRvalue(TypeInfo& returnType, ExprGenFn& gen) {
	if (returnType.mClass == TypeClass::IntConst) {
		gen = [val = (sint32)returnType.mIndex, this] {
			LoadConst(val);
		};

		returnType.mClass = TypeClass::Int;
		returnType.mIndex = 0;
	} else if (returnType.mClass == TypeClass::IntLValueVariable) {
		gen = [idx = (uint8)returnType.mIndex, this] {
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::IVLoad);
			mByteCodeBuffer.push_back(idx);
		};

		returnType.mClass = TypeClass::Int;
		returnType.mIndex = 0;
	} else if (returnType.mClass == TypeClass::IntLValueLocal) {
		gen = [idx = (uint8)returnType.mIndex, this] {
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::ILLoad);
			mByteCodeBuffer.push_back(idx);
		};

		returnType.mClass = TypeClass::Int;
		returnType.mIndex = 0;
	} else if (returnType.mClass == TypeClass::ObjectLValue) {
		gen = [idx = (uint8)returnType.mIndex, this] {
			mByteCodeBuffer.push_back((uint8)ATVMOpcode::IVLoad);
			mByteCodeBuffer.push_back(idx);
		};

		returnType.mClass = TypeClass::Object;
		returnType.mIndex = 0;
	} else if (returnType.mClass == TypeClass::StringConst) {
		gen = [idx = (sint32)returnType.mIndex, this] {
			LoadConst(idx);
		};

		returnType.mClass = TypeClass::String;
		returnType.mIndex = 0;
	}
}

uint32 ATVMCompiler::GetFunctionPointerTypeIndex(vdvector_view<const ATVMTypeInfo> types) {
	auto it = mFunctionPointerTypeLookup.find(types);

	if (it != mFunctionPointerTypeLookup.end())
		return it->second;

	const uint32 index = (uint32)mFunctionPointerTypeLookup.size();

	ATVMTypeInfo *internedTypes = mpDomain->mAllocator.AllocateArray<ATVMTypeInfo>(types.size());
	std::copy(types.begin(), types.end(), internedTypes);

	mFunctionPointerTypeLookup.insert(vdvector_view(internedTypes, types.size())).first->second = index;
	return index;
}

void ATVMCompiler::LoadConst(sint32 v) {
	if (v >= -128 && v <= 127) {
		mByteCodeBuffer.push_back((uint8)ATVMOpcode::IntConst8);
		mByteCodeBuffer.push_back((uint8)v);
	} else {
		mByteCodeBuffer.push_back((uint8)ATVMOpcode::IntConst);
		mByteCodeBuffer.push_back(0);
		mByteCodeBuffer.push_back(0);
		mByteCodeBuffer.push_back(0);
		mByteCodeBuffer.push_back(0);

		VDWriteUnalignedLES32(&*(mByteCodeBuffer.end() - 4), v);
	}
}

void ATVMCompiler::JumpToOffset(uint32 targetOffset) {
	JumpToOffset(ATVMOpcode::Ljmp, targetOffset);
}

void ATVMCompiler::JumpToOffset(ATVMOpcode opcode, uint32 targetOffset) {
	mByteCodeBuffer.push_back((uint8)opcode);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);

	sint32 branchDistance = targetOffset - (sint32)mByteCodeBuffer.size();
	VDWriteUnalignedLEU32(&*(mByteCodeBuffer.end() - 4), branchDistance);
}

uint32 ATVMCompiler::BeginLoop() {
	const uint32 breakLabel = AllocLabel();
	mBreakTargetLabels.push_back(breakLabel);

	const uint32 continueLabel = AllocLabel();
	mContinueTargetLabels.push_back(continueLabel);

	PatchLabel(continueLabel);

	return breakLabel;
}

bool ATVMCompiler::EndLoop() {
	uint32 label = mBreakTargetLabels.back();
	mBreakTargetLabels.pop_back();

	mContinueTargetLabels.pop_back();

	return PatchLabel(label);
}

bool ATVMCompiler::EmitBreakTarget() {
	if (!mBreakTargetLabels.back())
		return ReportError("No loop for break statement");

	return EmitPendingBranchTarget(mBreakTargetLabels.back());
}

uint32 ATVMCompiler::AllocLabel() {
	mKnownLabelOffsets.push_back(-1);
	return mLabelCounter++;
}

bool ATVMCompiler::EmitPendingBranchTarget(uint32 label) {
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);
	mByteCodeBuffer.push_back(0);

	if (label >= mKnownLabelOffsets.size())
		return ReportError("Internal compiler error: invalid label used as target");

	const uint32 curOffset = (uint32)mByteCodeBuffer.size();
	const sint32 labelOffset = mKnownLabelOffsets[label];
	if (labelOffset < 0) {
		mPendingBranchTargets.push_back(
			PendingBranchTarget {
				label, 
				curOffset
			}
		);
	} else {

		VDWriteUnalignedLES32(&*(mByteCodeBuffer.end() - 4), labelOffset - (sint32)curOffset);
	}

	return true;
}

bool ATVMCompiler::PatchLabel(uint32 label) {
	if (label >= mKnownLabelOffsets.size())
		return ReportError("Internal compiler error: invalid label to patch");

	const sint32 curOffset = (sint32)mByteCodeBuffer.size();
	mKnownLabelOffsets[label] = curOffset;

	auto it = mPendingBranchTargets.begin();
	bool patched = false;

	while(it != mPendingBranchTargets.end()) {
		if (it->mLabel == label) {
			VDWriteUnalignedLES32(&mByteCodeBuffer[it->mPatchOffset - 4], curOffset - (sint32)it->mPatchOffset);
			patched = true;

			*it = mPendingBranchTargets.back();
			mPendingBranchTargets.pop_back();
		} else {
			++it;
		}
	}

	return patched;
}

void ATVMCompiler::EmitSourceReference() {
	ATVMDebugSourceRange range;
	range.mCodeOffset = mByteCodeBuffer.size();
	range.mSourceRef.mLineId = mSrcLine;
	mByteCodeSourceRanges.emplace_back(range);
}

uint32 ATVMCompiler::Token() {
	if (mPushedToken) {
		uint32 tok = mPushedToken;
		mPushedToken = 0;

		return tok;
	}

	char c;
	for(;;) {
		if (mpSrc == mpSrcEnd)
			return kTokEnd;

		c = *mpSrc++;

		if (c == '/' && mpSrc != mpSrcEnd && *mpSrc == '/') {
			while(mpSrc != mpSrcEnd) {
				c = *mpSrc;

				if (c == '\r' || c == '\n')
					break;
				
				++mpSrc;
			}

			continue;
		}

		if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
			break;

		if (c == '\n')
			++mSrcLine;
	}

	if (mpSrc != mpSrcEnd) {
		char d = *mpSrc;

		switch(c) {
			case '<':
				if (d == '<') {
					++mpSrc;

					if (*mpSrc == '=') {
						++mpSrc;
						return kTokShlEq;
					}

					return kTokLeftShift;
				} else if (d == '=') {
					++mpSrc;
					return kTokLe;
				}
				break;

			case '>':
				if (d == '>') {
					++mpSrc;

					if (*mpSrc == '=') {
						++mpSrc;
						return kTokShrEq;
					}

					return kTokRightShift;
				} else if (d == '=') {
					++mpSrc;
					return kTokGe;
				}
				break;

			case '!':
				if (d == '=') {
					++mpSrc;
					return kTokNe;
				}
				break;

			case '=':
				if (d == '=') {
					++mpSrc;
					return kTokEq;
				}
				break;

			case '|':
				if (d == '|') {
					++mpSrc;
					return kTokLogicalOr;
				} else if (d == '=') {
					++mpSrc;
					return kTokOrEq;
				}
				break;

			case '^':
				if (d == '=') {
					++mpSrc;
					return kTokXorEq;
				}
				break;

			case '&':
				if (d == '&') {
					++mpSrc;
					return kTokLogicalAnd;
				} else if (d == '=') {
					++mpSrc;
					return kTokAndEq;
				}
				break;

			case '+':
				if (d == '+') {
					++mpSrc;
					return kTokIncrement;
				} else if (d == '=') {
					++mpSrc;
					return kTokAddEq;
				}
				break;

			case '-':
				if (d == '-') {
					++mpSrc;
					return kTokDecrement;
				} else if (d == '=') {
					++mpSrc;
					return kTokSubEq;
				}
				break;

			case '*':
				if (d == '=') {
					++mpSrc;
					return kTokMulEq;
				}
				break;

			case '/':
				if (d == '=') {
					++mpSrc;
					return kTokDivEq;
				}
				break;

			case '%':
				if (d == '=') {
					++mpSrc;
					return kTokModEq;
				}
				break;
		}
	}

	if (c == '"' || c == '\'') {
		const char *strStart = mpSrc;
		char term = c;

		for(;;) {
			if (mpSrc == mpSrcEnd && (*mpSrc == '\r' ||*mpSrc == '\n')) {
				ReportError("Unterminated string literal");
				return kTokError;
			}

			c = *mpSrc++;
			if (c == term)
				break;
		}

		mTokIdent = VDStringSpanA(strStart, mpSrc - 1);
		return kTokStringLiteral;
	}

	if (c == '0' && mpSrc != mpSrcEnd && (*mpSrc == 'x' || *mpSrc == 'X')) {
		++mpSrc;

		uint32 value = 0;
		const char *idStart = mpSrc;

		while(mpSrc != mpSrcEnd) {
			c = *mpSrc;

			if (c >= '0' && c <= '9')
				value = (value << 4) + (uint32)(c - '0');
			else if (c >= 'A' && c <= 'F')
				value = (value << 4) + (uint32)(c - 'A') + 10;
			else if (c >= 'a' && c <= 'f')
				value = (value << 4) + (uint32)(c - 'a') + 10;
			else
				break;

			++mpSrc;
		}

		if (mpSrc == idStart) {
			ReportError("Expected hex constant after '0x'");
			return kTokError;
		}

		mTokValue = (sint32)value;
		return kTokInteger;
	}

	if (c == '$') {
		uint32 value = 0;
		bool hexvalid = true;

		const char *idStart = mpSrc;

		while(mpSrc != mpSrcEnd) {
			c = *mpSrc;

			if (c >= '0' && c <= '9') {
				value = (value << 4) + (uint32)(c - '0');
			} else if (c >= 'A' && c <= 'F') {
				value = (value << 4) + (uint32)(c - 'A') + 10;
			} else if (c >= 'a' && c <= 'f') {
				value = (value << 4) + (uint32)(c - 'a') + 10;
			} else {
				if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && c != '_')
					break;

				hexvalid = false;
			}

			++mpSrc;
		}

		if (mpSrc == idStart) {
			ReportError("Expected hex constant or special variable name after '$'");
			return kTokError;
		}

		if (hexvalid) {
			mTokValue = (sint32)value;
			return kTokInteger;
		}

		mTokIdent = VDStringSpanA(idStart, mpSrc);
		return kTokSpecialIdent;
	}

	if (c >= '0' && c <= '9') {
		uint32 value = (uint32)(c - '0');

		while(mpSrc != mpSrcEnd) {
			c = *mpSrc;

			if (c < '0' || c > '9')
				break;

			value = (value * 10) + (uint32)(c - '0');

			++mpSrc;
		}

		mTokValue = (sint32)value;
		return kTokInteger;
	}

	if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
		const char *idStart = mpSrc - 1;

		while(mpSrc != mpSrcEnd) {
			c = *mpSrc;

			if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
				break;

			++mpSrc;
		}

		const char *idEnd = mpSrc;

		VDStringSpanA ident(idStart, idEnd);

		if (ident == "if") {
			return kTokIf;
		} else if (ident == "return") {
			return kTokReturn;
		} else if (ident == "int") {
			return kTokInt;
		} else if (ident == "void") {
			return kTokVoid;
		} else if (ident == "function") {
			return kTokFunction;
		} else if (ident == "else") {
			return kTokElse;
		} else if (ident == "true") {
			return kTokTrue;
		} else if (ident == "false") {
			return kTokFalse;
		} else if (ident == "loop") {
			return kTokLoop;
		} else if (ident == "break") {
			return kTokBreak;
		} else if (ident == "continue") {
			return kTokContinue;
		} else if (ident == "do") {
			return kTokDo;
		} else if (ident == "while") {
			return kTokWhile;
		} else if (ident == "event") {
			return kTokEvent;
		} else if (ident == "option") {
			return kTokOption;
		} else if (ident == "const") {
			return kTokConst;
		} else {
			mTokIdent = ident;
			return kTokIdentifier;
		}
	}

	if (strchr("()<>[]=+-*/;{}.,&^|~%!:", c))
		return (uint32)c;

	if (c >= 0x20 && c < 0x7F)
		ReportErrorF("Unexpected character '%c'", c);
	else
		ReportErrorF("Unexpected character 0x%02X", (unsigned char)c);

	return kTokError;
}

bool ATVMCompiler::ReportError(const char *msg) {
	if (mError.empty()) {
		mError = msg;
		mErrorPos = (sint32)(mpSrc - mpSrc0);
	}

	return false;
}

bool ATVMCompiler::ReportError(uint32 srcOffset, const char *msg) {
	if (mError.empty()) {
		mError = msg;
		mErrorPos = srcOffset;
	}

	return false;
}

bool ATVMCompiler::ReportErrorF(const char *format, ...) {
	if (mError.empty()) {
		va_list val;
		va_start(val, format);
		mError.append_vsprintf(format, val);
		va_end(val);

		mErrorPos = (sint32)(mpSrc - mpSrc0);
	}

	return false;
}

size_t ATVMCompiler::TypeInfoListHashPred::operator()(vdvector_view<const ATVMTypeInfo> typeList) const {
	size_t hash = 0;

	for(const ATVMTypeInfo& info : typeList) {
		hash += (uint32)info.mClass;
		hash += info.mIndex << 16;
		hash += (size_t)(uintptr_t)info.mpObjectClass;

		hash = (hash << 7) | (hash >> (sizeof(size_t) * 8 - 7));
	}

	return hash;
}

bool ATVMCompiler::TypeInfoListHashPred::operator()(vdvector_view<const ATVMTypeInfo> x, vdvector_view<const ATVMTypeInfo> y) const {
	auto n = x.size();

	if (n != y.size())
		return false;

	for(decltype(n) i = 0; i < n; ++i) {
		if (x[i] != y[i])
			return false;
	}

	return true;
}
