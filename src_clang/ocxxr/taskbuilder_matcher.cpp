//------------------------------------------------------------------------------
#include <typeinfo>

#include <set>
#include <string>
#include <cstring>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

#define ENABLE_DEBUG 1

#define ASSERT(cond) do { \
  if (ENABLE_DEBUG) { \
    if (!(cond)) { \
      llvm::errs() << "ASSERT FAILED: " << (#cond) << "\n" \
      << "\tat " << __FILE__ << ":" << __LINE__ << "\n"; \
      std::abort(); \
    } \
  } \
} while (0)

namespace clang {
namespace ast_matchers {

// AST matcher for names in a given set
AST_MATCHER_P(clang::NamedDecl, NameInSet, std::set<std::string>,
              nameset) {
  if (auto id = Node.getIdentifier()) {
    return nameset.count(id->getName().str()) > 0;
  }
  return false;
}

}  // namespace ast_matchers
}  // namespace clang

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("Matcher Sample");

static bool StringStartsWith(const std::string &str, const std::string &prefix) {
  return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

static const std::string ocxxr_db_prefix = "class ocxxr::Datablock<";
static const std::string ocxxr_arena_prefix = "class ocxxr::Arena<";

static bool IsDatablockType(const QualType &type) {
  if (!type->isClassType()) return false;
  const QualType &raw_type = type->getCanonicalTypeUnqualified();
  const std::string &raw_type_str = raw_type.getAsString();
  return StringStartsWith(raw_type_str, ocxxr_db_prefix)
    || StringStartsWith(raw_type_str, ocxxr_arena_prefix);
}

static QualType UnpackDbType(const QualType &db_type) {
  auto *record = db_type->getAsCXXRecordDecl();
  auto *specialization = llvm::dyn_cast<ClassTemplateSpecializationDecl>(record);
  ASSERT(specialization);
  auto &template_args = specialization->getTemplateArgs();
  ASSERT(template_args.size() == 1);
  return template_args[0].getAsType();
}

bool ContainsPointerType(const QualType &t) {
  const QualType &raw_type = t->getCanonicalTypeUnqualified();
  if (raw_type->isSpecifierType())
    return false;
  else if (/*const PointerType *x =*/ raw_type->getAs<PointerType>())
    return true;
  else if (const ArrayType *x = dyn_cast<ArrayType>(raw_type))
    return ContainsPointerType(x->getElementType());
  else if (/*const ReferenceType *x =*/ raw_type->getAs<ReferenceType>())
    return true;
  else if (const AutoType *x = raw_type->getAs<AutoType>())
    return ContainsPointerType(x->getDeducedType());
  else
    return false;
}

bool ParamContainsPointerType(const QualType &t) {
  const QualType &raw_type = t->getCanonicalTypeUnqualified();
  // Allow top-level reference for paramv entries
  if (const ReferenceType *x = raw_type->getAs<ReferenceType>())
    return ContainsPointerType(x->getPointeeType());
  else
    return ContainsPointerType(raw_type);
}

static const char *OK_STR = "[OK]";
static const char *BAD_STR = "[BAD]";

static inline const char *StrOkBad(bool isOK) {
  return isOK ? OK_STR : BAD_STR;
}

// FIXME - for some reason, QualType::GetBaseType considers function return types
// to be a "base type", which means this won't handle function pointers correctly.
// (Might need to rewrite this functionality to get the correct behavior.)

static void FindPersistedClassTypes(llvm::raw_ostream &out, const ClassTemplateSpecializationDecl &d) {
  auto &template_args = d.getTemplateArgs();
  ASSERT(template_args.size() > 0);
  auto const &arg_f = template_args[0].getAsType();
  ASSERT(arg_f->isFunctionProtoType());
  auto const &fn_type = *arg_f->getAs<FunctionProtoType>();
  const size_t args_count = fn_type.getNumParams();
  for (size_t i=0; i<args_count; i++) {
    const QualType &type = fn_type.getParamType(i);
    const bool is_db = IsDatablockType(type);
    //Type.canDecayToPointerType
    if (is_db) { // db-dependence input type
      const QualType inner_type = UnpackDbType(type);
      const bool needs_transform = ContainsPointerType(inner_type);
      out << "    Datablock: " << inner_type.getAsString();
      out << " " << StrOkBad(!needs_transform);
      out << "\n";
    }
    else { // paramv input type
      const bool needs_transform = ParamContainsPointerType(type);
      out << "    Parameter: " << type.getAsString();
      out << " " << StrOkBad(!needs_transform);
      out << "\n";
    }
  }
}

struct StuffDumper : public MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    auto *d = Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("stuff");
    //d->dump();
    const SourceManager &SM = Result.Context->getSourceManager();
    auto loc = d->getPointOfInstantiation();
    if (loc.isValid()) {
      auto &out = llvm::outs();
      out << "Found task template!\n  Location:\n    ";
      loc.print(out, SM);
      out << "\n  Types:\n";
      FindPersistedClassTypes(out, *d);
      out << "\n";
    }
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  // Set up AST matcher callbacks.
  StuffDumper HandlerForStuff;

  MatchFinder Finder;
  const TypeMatcher AnyType = anything();

  Finder.addMatcher(
      classTemplateSpecializationDecl(hasName("::ocxxr::TaskBuilder"))
      .bind("stuff"),
      &HandlerForStuff);

  llvm::outs() << "Running tool with AST matchers\n";
  Tool.run(newFrontendActionFactory(&Finder).get());

  return 0;
}
