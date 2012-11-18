// Copyright 2012 Google Inc. All Rights Reserved.
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

#include <node.h>

#include "clang-c/Index.h"

#include <climits>
#include <cstring>

class StringValue {
 public:
  StringValue(CXString str) : str_(str) { }

  ~StringValue() { clang_disposeString(str_); }

  v8::Handle<v8::String> operator* () {
    return v8::String::New(clang_getCString(str_));
  }

 private:
  CXString str_;
};

template<typename T>
class Wrapper {
 public:
  v8::Handle<v8::Object> Wrap(const T& val) {
    v8::HandleScope scope;
    v8::Handle<v8::Object> obj = ctor_->NewInstance();
    T* pval = new T(val);
    obj->SetPointerInInternalField(0, pval);
    v8::Persistent<v8::Object> weak = v8::Persistent<v8::Object>::New(obj);
    weak.MakeWeak(pval, &Dtor);
    weak.MarkIndependent();
    return scope.Close(obj);
  }

  const T& Unwrap(v8::Handle<v8::Value> value) {
    assert(template_->HasInstance(value));
    return *static_cast<T*>(v8::Handle<v8::Object>::Cast(value)->GetPointerFromInternalField(0));
  }

  v8::Handle<v8::FunctionTemplate> Template() {
    if (template_.IsEmpty()) Init();
    return template_;
  }

  v8::Handle<v8::Function> Constructor() {
    if (ctor_.IsEmpty()) {
      ctor_ = v8::Persistent<v8::Function>::New(Template()->GetFunction());
    }
    return ctor_;
  }

  v8::Handle<v8::ObjectTemplate> Prototype() {
    return Template()->PrototypeTemplate();
  }

 private:
  void Init() {
    template_ = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New());
    template_->InstanceTemplate()->SetInternalFieldCount(1);
  }

  static void Dtor(v8::Persistent<v8::Value> obj, void* ptr) {
    obj.Dispose();
    obj.Clear();
    delete static_cast<T*>(ptr);
  }

  v8::Persistent<v8::FunctionTemplate> template_;
  v8::Persistent<v8::Function> ctor_;
};

Wrapper<CXCursor> Cursor;
Wrapper<CXType> Type;

struct VisitorData {
  v8::Handle<v8::Function> callback_;
  v8::TryCatch try_catch_;
};

static CXChildVisitResult Visitor(CXCursor cursor,
                                  CXCursor parent,
                                  CXClientData data) {
  v8::HandleScope scope;

  VisitorData* visitor_data = static_cast<VisitorData*>(data);

  assert(!visitor_data->try_catch_.HasCaught());

  v8::Handle<v8::Value> args[2] = { Cursor.Wrap(cursor), Cursor.Wrap(parent) };

  v8::Local<v8::Value> result =
      visitor_data->callback_->Call(v8::Context::GetCurrent()->Global(), 2, args);

  if (visitor_data->try_catch_.HasCaught()) {
    return CXChildVisit_Break;
  }

  return static_cast<CXChildVisitResult>(result->Int32Value());
}

static v8::Handle<v8::Value> CursorVisit(const v8::Arguments& args) {
  v8::HandleScope scope;

  assert(args.Length() == 1);
  assert(args[0]->IsFunction());

  VisitorData data;
  data.callback_ = v8::Handle<v8::Function>::Cast(args[0]);
  unsigned result = clang_visitChildren(Cursor.Unwrap(args.This()), &Visitor, &data);
  if (data.try_catch_.HasCaught()) {
    return data.try_catch_.ReThrow();
  }
  return v8::Integer::NewFromUnsigned(result);
}

#define SIMPLE_METHOD0(Name, WrapResult, func, UnwrapThis)              \
  static v8::Handle<v8::Value> Name (const v8::Arguments& args) {       \
    v8::HandleScope scope;                                              \
    assert(args.Length() == 0);                                         \
    return scope.Close(WrapResult (func (UnwrapThis(args.This()))));     \
  }

SIMPLE_METHOD0(CursorSpelling, *StringValue, clang_getCursorSpelling, Cursor.Unwrap)
SIMPLE_METHOD0(CursorUSR, *StringValue, clang_getCursorUSR, Cursor.Unwrap)
SIMPLE_METHOD0(CursorKind, v8::Integer::New, clang_getCursorKind, Cursor.Unwrap)
SIMPLE_METHOD0(CursorDefinition, Cursor.Wrap, clang_getCursorDefinition, Cursor.Unwrap)
SIMPLE_METHOD0(CursorCanonical, Cursor.Wrap, clang_getCanonicalCursor, Cursor.Unwrap)
SIMPLE_METHOD0(CursorType, Type.Wrap, clang_getCursorType, Cursor.Unwrap)
SIMPLE_METHOD0(CursorIsStatic, v8::Boolean::New, clang_CXXMethod_isStatic, Cursor.Unwrap)
SIMPLE_METHOD0(CursorAccess, v8::Integer::New, clang_getCXXAccessSpecifier, Cursor.Unwrap)
SIMPLE_METHOD0(CursorDisplay, *StringValue, clang_getCursorDisplayName, Cursor.Unwrap)
SIMPLE_METHOD0(CursorParent, Cursor.Wrap, clang_getCursorSemanticParent, Cursor.Unwrap)
SIMPLE_METHOD0(CursorUnderlyingType, Type.Wrap, clang_getTypedefDeclUnderlyingType, Cursor.Unwrap)
SIMPLE_METHOD0(CursorSpecialized, Cursor.Wrap, clang_getSpecializedCursorTemplate, Cursor.Unwrap)
SIMPLE_METHOD0(CursorIsNull, v8::Boolean::New, clang_Cursor_isNull, Cursor.Unwrap)

#define BIND(proto, name, func) (proto)->Set(v8::String::New(#name), v8::FunctionTemplate::New(&func))

#define BINDCONST(t, name, value) (t)->Set(v8::String::New(#name), v8::Integer::New(value))

static v8::Handle<v8::Function> RegisterCursor() {
  v8::HandleScope scope;
  BIND(Cursor.Prototype(), spelling, CursorSpelling);
  BIND(Cursor.Prototype(), usr, CursorUSR);
  BIND(Cursor.Prototype(), kind, CursorKind);
  BIND(Cursor.Prototype(), definition, CursorDefinition);
  BIND(Cursor.Prototype(), canonical, CursorCanonical);
  BIND(Cursor.Prototype(), visit, CursorVisit);
  BIND(Cursor.Prototype(), type, CursorType);
  BIND(Cursor.Prototype(), isStatic, CursorIsStatic);
  BIND(Cursor.Prototype(), access, CursorAccess);
  BIND(Cursor.Prototype(), display, CursorDisplay);
  BIND(Cursor.Prototype(), parent, CursorParent);
  BIND(Cursor.Prototype(), underlyingType, CursorUnderlyingType);
  BIND(Cursor.Prototype(), specialized, CursorSpecialized);
  BIND(Cursor.Prototype(), isNull, CursorIsNull);

  /*
  ** Generated with:
  **   cat /usr/local/include/clang-c/Index.h |                         \
  **   perl -n -e'/CXCursor_([\w_]+).*=/ && print "BINDCONST(Cursor.Template(), $1, CXCursor_$1);\n"' | \
  **   sort |                                                           \
  **   uniq
  */
  BINDCONST(Cursor.Template(), AddrLabelExpr, CXCursor_AddrLabelExpr);
  BINDCONST(Cursor.Template(), AnnotateAttr, CXCursor_AnnotateAttr);
  BINDCONST(Cursor.Template(), ArraySubscriptExpr, CXCursor_ArraySubscriptExpr);
  BINDCONST(Cursor.Template(), AsmLabelAttr, CXCursor_AsmLabelAttr);
  BINDCONST(Cursor.Template(), BinaryOperator, CXCursor_BinaryOperator);
  BINDCONST(Cursor.Template(), BlockExpr, CXCursor_BlockExpr);
  BINDCONST(Cursor.Template(), BreakStmt, CXCursor_BreakStmt);
  BINDCONST(Cursor.Template(), CStyleCastExpr, CXCursor_CStyleCastExpr);
  BINDCONST(Cursor.Template(), CXXAccessSpecifier, CXCursor_CXXAccessSpecifier);
  BINDCONST(Cursor.Template(), CXXBaseSpecifier, CXCursor_CXXBaseSpecifier);
  BINDCONST(Cursor.Template(), CXXBoolLiteralExpr, CXCursor_CXXBoolLiteralExpr);
  BINDCONST(Cursor.Template(), CXXCatchStmt, CXCursor_CXXCatchStmt);
  BINDCONST(Cursor.Template(), CXXConstCastExpr, CXCursor_CXXConstCastExpr);
  BINDCONST(Cursor.Template(), CXXDeleteExpr, CXCursor_CXXDeleteExpr);
  BINDCONST(Cursor.Template(), CXXDynamicCastExpr, CXCursor_CXXDynamicCastExpr);
  BINDCONST(Cursor.Template(), CXXFinalAttr, CXCursor_CXXFinalAttr);
  BINDCONST(Cursor.Template(), CXXForRangeStmt, CXCursor_CXXForRangeStmt);
  BINDCONST(Cursor.Template(), CXXFunctionalCastExpr, CXCursor_CXXFunctionalCastExpr);
  BINDCONST(Cursor.Template(), CXXMethod, CXCursor_CXXMethod);
  BINDCONST(Cursor.Template(), CXXNewExpr, CXCursor_CXXNewExpr);
  BINDCONST(Cursor.Template(), CXXNullPtrLiteralExpr, CXCursor_CXXNullPtrLiteralExpr);
  BINDCONST(Cursor.Template(), CXXOverrideAttr, CXCursor_CXXOverrideAttr);
  BINDCONST(Cursor.Template(), CXXReinterpretCastExpr, CXCursor_CXXReinterpretCastExpr);
  BINDCONST(Cursor.Template(), CXXStaticCastExpr, CXCursor_CXXStaticCastExpr);
  BINDCONST(Cursor.Template(), CXXThisExpr, CXCursor_CXXThisExpr);
  BINDCONST(Cursor.Template(), CXXThrowExpr, CXCursor_CXXThrowExpr);
  BINDCONST(Cursor.Template(), CXXTryStmt, CXCursor_CXXTryStmt);
  BINDCONST(Cursor.Template(), CXXTypeidExpr, CXCursor_CXXTypeidExpr);
  BINDCONST(Cursor.Template(), CallExpr, CXCursor_CallExpr);
  BINDCONST(Cursor.Template(), CaseStmt, CXCursor_CaseStmt);
  BINDCONST(Cursor.Template(), CharacterLiteral, CXCursor_CharacterLiteral);
  BINDCONST(Cursor.Template(), ClassDecl, CXCursor_ClassDecl);
  BINDCONST(Cursor.Template(), ClassTemplate, CXCursor_ClassTemplate);
  BINDCONST(Cursor.Template(), ClassTemplatePartialSpecialization, CXCursor_ClassTemplatePartialSpecialization);
  BINDCONST(Cursor.Template(), CompoundAssignOperator, CXCursor_CompoundAssignOperator);
  BINDCONST(Cursor.Template(), CompoundLiteralExpr, CXCursor_CompoundLiteralExpr);
  BINDCONST(Cursor.Template(), CompoundStmt, CXCursor_CompoundStmt);
  BINDCONST(Cursor.Template(), ConditionalOperator, CXCursor_ConditionalOperator);
  BINDCONST(Cursor.Template(), Constructor, CXCursor_Constructor);
  BINDCONST(Cursor.Template(), ContinueStmt, CXCursor_ContinueStmt);
  BINDCONST(Cursor.Template(), ConversionFunction, CXCursor_ConversionFunction);
  BINDCONST(Cursor.Template(), DeclRefExpr, CXCursor_DeclRefExpr);
  BINDCONST(Cursor.Template(), DeclStmt, CXCursor_DeclStmt);
  BINDCONST(Cursor.Template(), DefaultStmt, CXCursor_DefaultStmt);
  BINDCONST(Cursor.Template(), Destructor, CXCursor_Destructor);
  BINDCONST(Cursor.Template(), DoStmt, CXCursor_DoStmt);
  BINDCONST(Cursor.Template(), EnumConstantDecl, CXCursor_EnumConstantDecl);
  BINDCONST(Cursor.Template(), EnumDecl, CXCursor_EnumDecl);
  BINDCONST(Cursor.Template(), FieldDecl, CXCursor_FieldDecl);
  BINDCONST(Cursor.Template(), FirstAttr, CXCursor_FirstAttr);
  BINDCONST(Cursor.Template(), FirstDecl, CXCursor_FirstDecl);
  BINDCONST(Cursor.Template(), FirstExpr, CXCursor_FirstExpr);
  BINDCONST(Cursor.Template(), FirstInvalid, CXCursor_FirstInvalid);
  BINDCONST(Cursor.Template(), FirstPreprocessing, CXCursor_FirstPreprocessing);
  BINDCONST(Cursor.Template(), FirstRef, CXCursor_FirstRef);
  BINDCONST(Cursor.Template(), FirstStmt, CXCursor_FirstStmt);
  BINDCONST(Cursor.Template(), FloatingLiteral, CXCursor_FloatingLiteral);
  BINDCONST(Cursor.Template(), ForStmt, CXCursor_ForStmt);
  BINDCONST(Cursor.Template(), FunctionDecl, CXCursor_FunctionDecl);
  BINDCONST(Cursor.Template(), FunctionTemplate, CXCursor_FunctionTemplate);
  BINDCONST(Cursor.Template(), GCCAsmStmt, CXCursor_GCCAsmStmt);
  BINDCONST(Cursor.Template(), GNUNullExpr, CXCursor_GNUNullExpr);
  BINDCONST(Cursor.Template(), GenericSelectionExpr, CXCursor_GenericSelectionExpr);
  BINDCONST(Cursor.Template(), GotoStmt, CXCursor_GotoStmt);
  BINDCONST(Cursor.Template(), IBActionAttr, CXCursor_IBActionAttr);
  BINDCONST(Cursor.Template(), IBOutletAttr, CXCursor_IBOutletAttr);
  BINDCONST(Cursor.Template(), IBOutletCollectionAttr, CXCursor_IBOutletCollectionAttr);
  BINDCONST(Cursor.Template(), IfStmt, CXCursor_IfStmt);
  BINDCONST(Cursor.Template(), ImaginaryLiteral, CXCursor_ImaginaryLiteral);
  BINDCONST(Cursor.Template(), InclusionDirective, CXCursor_InclusionDirective);
  BINDCONST(Cursor.Template(), IndirectGotoStmt, CXCursor_IndirectGotoStmt);
  BINDCONST(Cursor.Template(), InitListExpr, CXCursor_InitListExpr);
  BINDCONST(Cursor.Template(), IntegerLiteral, CXCursor_IntegerLiteral);
  BINDCONST(Cursor.Template(), InvalidCode, CXCursor_InvalidCode);
  BINDCONST(Cursor.Template(), InvalidFile, CXCursor_InvalidFile);
  BINDCONST(Cursor.Template(), LabelRef, CXCursor_LabelRef);
  BINDCONST(Cursor.Template(), LabelStmt, CXCursor_LabelStmt);
  BINDCONST(Cursor.Template(), LambdaExpr, CXCursor_LambdaExpr);
  BINDCONST(Cursor.Template(), LastAttr, CXCursor_LastAttr);
  BINDCONST(Cursor.Template(), LastDecl, CXCursor_LastDecl);
  BINDCONST(Cursor.Template(), LastExpr, CXCursor_LastExpr);
  BINDCONST(Cursor.Template(), LastInvalid, CXCursor_LastInvalid);
  BINDCONST(Cursor.Template(), LastPreprocessing, CXCursor_LastPreprocessing);
  BINDCONST(Cursor.Template(), LastRef, CXCursor_LastRef);
  BINDCONST(Cursor.Template(), LastStmt, CXCursor_LastStmt);
  BINDCONST(Cursor.Template(), LinkageSpec, CXCursor_LinkageSpec);
  BINDCONST(Cursor.Template(), MSAsmStmt, CXCursor_MSAsmStmt);
  BINDCONST(Cursor.Template(), MacroDefinition, CXCursor_MacroDefinition);
  BINDCONST(Cursor.Template(), MacroExpansion, CXCursor_MacroExpansion);
  BINDCONST(Cursor.Template(), MacroInstantiation, CXCursor_MacroInstantiation);
  BINDCONST(Cursor.Template(), MemberRef, CXCursor_MemberRef);
  BINDCONST(Cursor.Template(), MemberRefExpr, CXCursor_MemberRefExpr);
  BINDCONST(Cursor.Template(), Namespace, CXCursor_Namespace);
  BINDCONST(Cursor.Template(), NamespaceAlias, CXCursor_NamespaceAlias);
  BINDCONST(Cursor.Template(), NamespaceRef, CXCursor_NamespaceRef);
  BINDCONST(Cursor.Template(), NoDeclFound, CXCursor_NoDeclFound);
  BINDCONST(Cursor.Template(), NonTypeTemplateParameter, CXCursor_NonTypeTemplateParameter);
  BINDCONST(Cursor.Template(), NotImplemented, CXCursor_NotImplemented);
  BINDCONST(Cursor.Template(), NullStmt, CXCursor_NullStmt);
  BINDCONST(Cursor.Template(), ObjCAtCatchStmt, CXCursor_ObjCAtCatchStmt);
  BINDCONST(Cursor.Template(), ObjCAtFinallyStmt, CXCursor_ObjCAtFinallyStmt);
  BINDCONST(Cursor.Template(), ObjCAtSynchronizedStmt, CXCursor_ObjCAtSynchronizedStmt);
  BINDCONST(Cursor.Template(), ObjCAtThrowStmt, CXCursor_ObjCAtThrowStmt);
  BINDCONST(Cursor.Template(), ObjCAtTryStmt, CXCursor_ObjCAtTryStmt);
  BINDCONST(Cursor.Template(), ObjCAutoreleasePoolStmt, CXCursor_ObjCAutoreleasePoolStmt);
  BINDCONST(Cursor.Template(), ObjCBoolLiteralExpr, CXCursor_ObjCBoolLiteralExpr);
  BINDCONST(Cursor.Template(), ObjCBridgedCastExpr, CXCursor_ObjCBridgedCastExpr);
  BINDCONST(Cursor.Template(), ObjCCategoryDecl, CXCursor_ObjCCategoryDecl);
  BINDCONST(Cursor.Template(), ObjCCategoryImplDecl, CXCursor_ObjCCategoryImplDecl);
  BINDCONST(Cursor.Template(), ObjCClassMethodDecl, CXCursor_ObjCClassMethodDecl);
  BINDCONST(Cursor.Template(), ObjCClassRef, CXCursor_ObjCClassRef);
  BINDCONST(Cursor.Template(), ObjCDynamicDecl, CXCursor_ObjCDynamicDecl);
  BINDCONST(Cursor.Template(), ObjCEncodeExpr, CXCursor_ObjCEncodeExpr);
  BINDCONST(Cursor.Template(), ObjCForCollectionStmt, CXCursor_ObjCForCollectionStmt);
  BINDCONST(Cursor.Template(), ObjCImplementationDecl, CXCursor_ObjCImplementationDecl);
  BINDCONST(Cursor.Template(), ObjCInstanceMethodDecl, CXCursor_ObjCInstanceMethodDecl);
  BINDCONST(Cursor.Template(), ObjCInterfaceDecl, CXCursor_ObjCInterfaceDecl);
  BINDCONST(Cursor.Template(), ObjCIvarDecl, CXCursor_ObjCIvarDecl);
  BINDCONST(Cursor.Template(), ObjCMessageExpr, CXCursor_ObjCMessageExpr);
  BINDCONST(Cursor.Template(), ObjCPropertyDecl, CXCursor_ObjCPropertyDecl);
  BINDCONST(Cursor.Template(), ObjCProtocolDecl, CXCursor_ObjCProtocolDecl);
  BINDCONST(Cursor.Template(), ObjCProtocolExpr, CXCursor_ObjCProtocolExpr);
  BINDCONST(Cursor.Template(), ObjCProtocolRef, CXCursor_ObjCProtocolRef);
  BINDCONST(Cursor.Template(), ObjCSelectorExpr, CXCursor_ObjCSelectorExpr);
  BINDCONST(Cursor.Template(), ObjCStringLiteral, CXCursor_ObjCStringLiteral);
  BINDCONST(Cursor.Template(), ObjCSuperClassRef, CXCursor_ObjCSuperClassRef);
  BINDCONST(Cursor.Template(), ObjCSynthesizeDecl, CXCursor_ObjCSynthesizeDecl);
  BINDCONST(Cursor.Template(), OverloadedDeclRef, CXCursor_OverloadedDeclRef);
  BINDCONST(Cursor.Template(), PackExpansionExpr, CXCursor_PackExpansionExpr);
  BINDCONST(Cursor.Template(), ParenExpr, CXCursor_ParenExpr);
  BINDCONST(Cursor.Template(), ParmDecl, CXCursor_ParmDecl);
  BINDCONST(Cursor.Template(), PreprocessingDirective, CXCursor_PreprocessingDirective);
  BINDCONST(Cursor.Template(), ReturnStmt, CXCursor_ReturnStmt);
  BINDCONST(Cursor.Template(), SEHExceptStmt, CXCursor_SEHExceptStmt);
  BINDCONST(Cursor.Template(), SEHFinallyStmt, CXCursor_SEHFinallyStmt);
  BINDCONST(Cursor.Template(), SEHTryStmt, CXCursor_SEHTryStmt);
  BINDCONST(Cursor.Template(), SizeOfPackExpr, CXCursor_SizeOfPackExpr);
  BINDCONST(Cursor.Template(), StmtExpr, CXCursor_StmtExpr);
  BINDCONST(Cursor.Template(), StringLiteral, CXCursor_StringLiteral);
  BINDCONST(Cursor.Template(), StructDecl, CXCursor_StructDecl);
  BINDCONST(Cursor.Template(), SwitchStmt, CXCursor_SwitchStmt);
  BINDCONST(Cursor.Template(), TemplateRef, CXCursor_TemplateRef);
  BINDCONST(Cursor.Template(), TemplateTemplateParameter, CXCursor_TemplateTemplateParameter);
  BINDCONST(Cursor.Template(), TemplateTypeParameter, CXCursor_TemplateTypeParameter);
  BINDCONST(Cursor.Template(), TranslationUnit, CXCursor_TranslationUnit);
  BINDCONST(Cursor.Template(), TypeAliasDecl, CXCursor_TypeAliasDecl);
  BINDCONST(Cursor.Template(), TypeRef, CXCursor_TypeRef);
  BINDCONST(Cursor.Template(), TypedefDecl, CXCursor_TypedefDecl);
  BINDCONST(Cursor.Template(), UnaryExpr, CXCursor_UnaryExpr);
  BINDCONST(Cursor.Template(), UnaryOperator, CXCursor_UnaryOperator);
  BINDCONST(Cursor.Template(), UnexposedAttr, CXCursor_UnexposedAttr);
  BINDCONST(Cursor.Template(), UnexposedDecl, CXCursor_UnexposedDecl);
  BINDCONST(Cursor.Template(), UnexposedExpr, CXCursor_UnexposedExpr);
  BINDCONST(Cursor.Template(), UnexposedStmt, CXCursor_UnexposedStmt);
  BINDCONST(Cursor.Template(), UnionDecl, CXCursor_UnionDecl);
  BINDCONST(Cursor.Template(), UsingDeclaration, CXCursor_UsingDeclaration);
  BINDCONST(Cursor.Template(), UsingDirective, CXCursor_UsingDirective);
  BINDCONST(Cursor.Template(), VarDecl, CXCursor_VarDecl);
  BINDCONST(Cursor.Template(), VariableRef, CXCursor_VariableRef);
  BINDCONST(Cursor.Template(), WhileStmt, CXCursor_WhileStmt);

  BINDCONST(Cursor.Template(), VisitBreak, CXChildVisit_Break);
  BINDCONST(Cursor.Template(), VisitContinue, CXChildVisit_Continue);
  BINDCONST(Cursor.Template(), VisitRecurse, CXChildVisit_Recurse);

  BINDCONST(Cursor.Template(), CXXInvalidAccessSpecifier, CX_CXXInvalidAccessSpecifier);
  BINDCONST(Cursor.Template(), CXXPublic, CX_CXXPublic);
  BINDCONST(Cursor.Template(), CXXProtected, CX_CXXProtected);
  BINDCONST(Cursor.Template(), CXXPrivate, CX_CXXPrivate);

  return scope.Close(Cursor.Constructor());
}

#define COMPLEX_METHOD0(Name, WrapResult, R, func, A, UnwrapThis)       \
  R Name##Impl (A arg0) { func; }                                       \
  static v8::Handle<v8::Value> Name (const v8::Arguments& args) {       \
    v8::HandleScope scope;                                              \
    assert(args.Length() == 0);                                         \
    return scope.Close(WrapResult (Name##Impl (UnwrapThis(args.This())))); \
  }


SIMPLE_METHOD0(TypePointee, Type.Wrap, clang_getPointeeType, Type.Unwrap)
SIMPLE_METHOD0(TypeCanonical, Type.Wrap, clang_getCanonicalType, Type.Unwrap)
SIMPLE_METHOD0(TypeDeclaration, Cursor.Wrap, clang_getTypeDeclaration, Type.Unwrap)
COMPLEX_METHOD0(TypeKind, v8::Integer::New, CXTypeKind, return arg0.kind, const CXType&, Type.Unwrap)
COMPLEX_METHOD0(TypeSpelling, *StringValue, CXString, return clang_getTypeKindSpelling(arg0.kind), const CXType&, Type.Unwrap)
SIMPLE_METHOD0(TypeResult, Type.Wrap, clang_getResultType, Type.Unwrap)
COMPLEX_METHOD0(TypeArgs, v8::Handle<v8::Value>, v8::Handle<v8::Value>, { \
    unsigned argc = clang_getNumArgTypes(arg0);                         \
    if (argc == UINT_MAX) return v8::Null();                            \
    v8::Handle<v8::Array> args = v8::Array::New(argc);                  \
    for (unsigned i = 0; i < argc; i++) args->Set(i, Type.Wrap(clang_getArgType(arg0, i))); \
    return args;                                                        \
  }, const CXType&, Type.Unwrap)

SIMPLE_METHOD0(TypeIsVariadic, v8::Boolean::New, clang_isFunctionTypeVariadic, Type.Unwrap);

static v8::Handle<v8::Function> RegisterType() {
  v8::HandleScope scope;
  BIND(Type.Prototype(), declaration, TypeDeclaration);
  BIND(Type.Prototype(), kind, TypeKind);
  BIND(Type.Prototype(), canonical, TypeCanonical);
  BIND(Type.Prototype(), result, TypeResult);
  BIND(Type.Prototype(), args, TypeArgs);
  BIND(Type.Prototype(), pointee, TypePointee);
  BIND(Type.Prototype(), spelling, TypeSpelling);
  BIND(Type.Prototype(), isVariadic, TypeIsVariadic);

  /*
  ** Generated with:
  **   cat /usr/local/include/clang-c/Index.h |                         \
  **   perl -n -e'/CXType_([\w_]+).*=/ && print "BINDCONST(Type.Template(), $1, CXType_$1);\n"' | \
  **   sort |                                                           \
  **   uniq
  */
  BINDCONST(Type.Template(), BlockPointer, CXType_BlockPointer);
  BINDCONST(Type.Template(), Bool, CXType_Bool);
  BINDCONST(Type.Template(), Char16, CXType_Char16);
  BINDCONST(Type.Template(), Char32, CXType_Char32);
  BINDCONST(Type.Template(), Char_S, CXType_Char_S);
  BINDCONST(Type.Template(), Char_U, CXType_Char_U);
  BINDCONST(Type.Template(), Complex, CXType_Complex);
  BINDCONST(Type.Template(), ConstantArray, CXType_ConstantArray);
  BINDCONST(Type.Template(), Dependent, CXType_Dependent);
  BINDCONST(Type.Template(), Double, CXType_Double);
  BINDCONST(Type.Template(), Enum, CXType_Enum);
  BINDCONST(Type.Template(), FirstBuiltin, CXType_FirstBuiltin);
  BINDCONST(Type.Template(), Float, CXType_Float);
  BINDCONST(Type.Template(), FunctionNoProto, CXType_FunctionNoProto);
  BINDCONST(Type.Template(), FunctionProto, CXType_FunctionProto);
  BINDCONST(Type.Template(), Int, CXType_Int);
  BINDCONST(Type.Template(), Int128, CXType_Int128);
  BINDCONST(Type.Template(), Invalid, CXType_Invalid);
  BINDCONST(Type.Template(), LValueReference, CXType_LValueReference);
  BINDCONST(Type.Template(), LastBuiltin, CXType_LastBuiltin);
  BINDCONST(Type.Template(), Long, CXType_Long);
  BINDCONST(Type.Template(), LongDouble, CXType_LongDouble);
  BINDCONST(Type.Template(), LongLong, CXType_LongLong);
  BINDCONST(Type.Template(), NullPtr, CXType_NullPtr);
  BINDCONST(Type.Template(), ObjCClass, CXType_ObjCClass);
  BINDCONST(Type.Template(), ObjCId, CXType_ObjCId);
  BINDCONST(Type.Template(), ObjCInterface, CXType_ObjCInterface);
  BINDCONST(Type.Template(), ObjCObjectPointer, CXType_ObjCObjectPointer);
  BINDCONST(Type.Template(), ObjCSel, CXType_ObjCSel);
  BINDCONST(Type.Template(), Overload, CXType_Overload);
  BINDCONST(Type.Template(), Pointer, CXType_Pointer);
  BINDCONST(Type.Template(), RValueReference, CXType_RValueReference);
  BINDCONST(Type.Template(), Record, CXType_Record);
  BINDCONST(Type.Template(), SChar, CXType_SChar);
  BINDCONST(Type.Template(), Short, CXType_Short);
  BINDCONST(Type.Template(), Typedef, CXType_Typedef);
  BINDCONST(Type.Template(), UChar, CXType_UChar);
  BINDCONST(Type.Template(), UInt, CXType_UInt);
  BINDCONST(Type.Template(), UInt128, CXType_UInt128);
  BINDCONST(Type.Template(), ULong, CXType_ULong);
  BINDCONST(Type.Template(), ULongLong, CXType_ULongLong);
  BINDCONST(Type.Template(), UShort, CXType_UShort);
  BINDCONST(Type.Template(), Unexposed, CXType_Unexposed);
  BINDCONST(Type.Template(), Vector, CXType_Vector);
  BINDCONST(Type.Template(), Void, CXType_Void);
  BINDCONST(Type.Template(), WChar, CXType_WChar);

  return scope.Close(Type.Constructor());
}

class Context {
 public:
  Context(CXIndex index, CXTranslationUnit tu) : index_(index), tu_(tu) { }
  ~Context() {
    if (tu_ != NULL) clang_disposeTranslationUnit(tu_);
    if (index_ != NULL) clang_disposeIndex(index_);
  }

  Context(const Context& other) {
    index_ = other.index_;
    tu_ = other.tu_;
    const_cast<Context&>(other).index_ = NULL;
    const_cast<Context&>(other).tu_ = NULL;
  }

  CXTranslationUnit tu() const { return tu_; }
 private:
  CXIndex index_;
  CXTranslationUnit tu_;
};

Wrapper<Context> ContextT;

COMPLEX_METHOD0(ContextCursor,
                Cursor.Wrap,
                CXCursor,
                return clang_getTranslationUnitCursor(arg0.tu()),
                const Context&,
                ContextT.Unwrap);

static v8::Handle<v8::Function> RegisterContext() {
  v8::HandleScope scope;
  BIND(ContextT.Prototype(), cursor, ContextCursor);
  return ContextT.Constructor();
}


class String {
 public:
  String() : str_(NULL) { }
  ~String() { delete[] str_; }

  void Copy(const char* str, int length) {
    str_ = new char[length + 1];
    strncpy(str_, str, length);
    str_[length] = '\0';
  }

  const char* str() { return str_; }
 private:
  char* str_;
};

static v8::Handle<v8::Value> Parse(const v8::Arguments& args) {
  v8::HandleScope scope;

  int argc;

  v8::Handle<v8::Array> arr;
  if (args.Length() == 1 && args[0]->IsArray()) {
    arr = v8::Handle<v8::Array>::Cast(args[0]);
    argc = arr->Length();
  } else {
    argc = args.Length();
  }

  String* strargs = new String[argc];
  for (int i = 0; i < argc; i++) {
    v8::String::AsciiValue arg(arr.IsEmpty() ? args[i] : arr->Get(i));
    if (arg.length() == 0) {
      delete[] strargs;
      return v8::ThrowException(v8::Exception::Error(v8::String::New("expected string arguments")));
    }
    strargs[i].Copy(*arg, arg.length());
  }

  const char** argv = new const char* [argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = strargs[i].str();
  }

  CXIndex index = clang_createIndex(0, 0);
  CXTranslationUnit tu = clang_parseTranslationUnit(index, 0, argv, argc, 0, 0, CXTranslationUnit_None);

  delete[] argv;
  delete[] strargs;

  if (tu == NULL) {
    clang_disposeIndex(index);
    return v8::ThrowException(v8::Exception::Error(v8::String::New("failed to parse translation unit")));
  }

  for (unsigned i = 0, n = clang_getNumDiagnostics(tu); i != n; ++i) {
    CXString str = clang_formatDiagnostic(
        clang_getDiagnostic(tu, i), clang_defaultDiagnosticDisplayOptions());
    fprintf(stderr, "%s\n", clang_getCString(str));
    clang_disposeString(str);
  }

  return scope.Close(ContextT.Wrap(Context(index, tu)));
}


static void Register(v8::Handle<v8::Object> exports) {
  v8::HandleScope scope;
  exports->Set(v8::String::New("Cursor"), RegisterCursor());
  exports->Set(v8::String::New("Type"), RegisterType());
  RegisterContext();
  exports->Set(v8::String::New("Parse"), v8::FunctionTemplate::New(&Parse)->GetFunction());
}

NODE_MODULE(libclang, Register);
