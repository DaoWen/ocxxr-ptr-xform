//------------------------------------------------------------------------------
#include <set>
#include <string>

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

struct StuffDumper : public MatchFinder::MatchCallback {
  virtual void run(const MatchFinder::MatchResult &Result) {
    auto *d = Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("stuff");
    d->dump();
    const SourceManager &SM = Result.Context->getSourceManager();
    auto loc = d->getPointOfInstantiation();
    if (loc.isValid()) {
      loc.print(llvm::outs(), SM);
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
      classTemplateSpecializationDecl(hasName("::ocxxr::TaskTemplate"))
      .bind("stuff"),
      &HandlerForStuff);

  llvm::outs() << "Running tool with AST matchers\n";
  Tool.run(newFrontendActionFactory(&Finder).get());

  return 0;
}
