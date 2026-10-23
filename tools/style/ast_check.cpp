#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace {
    llvm::cl::OptionCategory STYLE_OPTIONS("AHO style checks");
    llvm::cl::opt<std::string> ROOT("root", llvm::cl::Required, llvm::cl::cat(STYLE_OPTIONS));

    struct Position {
        std::string file;
        unsigned line, column, offset;
    };

    struct Declaration {
        Position position;
        std::string symbol;
        bool definition;
    };

    class Visitor : public clang::RecursiveASTVisitor<Visitor> {
        clang::ASTContext& _context;
        std::map<std::string, std::vector<Declaration>> _scopes;
        std::set<std::pair<unsigned, std::string>> _emitted;

        bool position(clang::SourceLocation location, Position& result) const;
        void report(const Position& position, llvm::StringRef rule, llvm::StringRef symbol, llvm::StringRef message);
        bool exempt_record(const clang::DeclContext* context) const;

    public:
        explicit Visitor(clang::ASTContext& context);
        // Clangのvisitorプロトコルで指定された名前を維持する。
        bool VisitNamedDecl(clang::NamedDecl* declaration);
        bool VisitFunctionDecl(clang::FunctionDecl* declaration);
        void finish();
    };

    class Consumer : public clang::ASTConsumer {
    public:
        void HandleTranslationUnit(clang::ASTContext& context) override;
    };

    class Action : public clang::ASTFrontendAction {
    public:
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef) override;
    };

    Visitor::Visitor(clang::ASTContext& context) : _context(context) {}

    bool Visitor::position(clang::SourceLocation location, Position& result) const {
        if (location.isInvalid() || location.isMacroID())
            return false;
        const auto& source = _context.getSourceManager();
        if (source.isInSystemHeader(location))
            return false;
        llvm::SmallString<256> path(source.getFilename(location));
        if (path.empty())
            return false;
        llvm::sys::path::remove_dots(path, true);
        std::string root = ROOT;
        if (!root.ends_with('/'))
            root += '/';
        const std::string absolute = path.str().str();
        if (!absolute.starts_with(root))
            return false;
        result = {absolute.substr(root.size()), source.getSpellingLineNumber(location),
                  source.getSpellingColumnNumber(location), source.getFileOffset(location)};
        return true;
    }

    void Visitor::report(const Position& position, llvm::StringRef rule, llvm::StringRef symbol, llvm::StringRef message) {
        // ヘッダーを複数TUから読む場合の重複は実行側でも除去する。
        if (!_emitted.emplace(position.offset, position.file + rule.str()).second)
            return;
        llvm::outs() << llvm::json::Value(llvm::json::Object{
                            {"file", position.file}, {"line", position.line}, {"column", position.column}, {"rule", rule}, {"symbol", symbol}, {"message", message}})
                     << '\n';
    }

    bool Visitor::exempt_record(const clang::DeclContext* context) const {
        for (; context; context = context->getParent())
            if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(context))
                if (record->isLambda() || record->isLocalClass())
                    return true;
        return false;
    }

    bool Visitor::VisitNamedDecl(clang::NamedDecl* declaration) {
        Position where;
        if (declaration->isImplicit() || !position(declaration->getLocation(), where))
            return true;
        const auto name = declaration->getNameAsString();
        const bool type = llvm::isa<clang::TagDecl, clang::TypedefNameDecl>(declaration);
        const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration);
        static const std::regex ACRONYM("(Gpu|Cpu|Vsl|Aho)([A-Z0-9_]|$)");
        if ((type || (method && method->isStatic())) && std::regex_search(name, ACRONYM))
            report(where, "aho-acronym", declaration->getQualifiedNameAsString(), "GPU/CPU/VSL/AHOの大文字表記を維持してください");

        // clang-tidy 20が扱わない構造化束縛も、通常変数と同じ規則で検査する。
        if (const auto* binding = llvm::dyn_cast<clang::BindingDecl>(declaration)) {
            const auto* variable = llvm::dyn_cast_or_null<clang::VarDecl>(binding->getDecomposedDecl());
            const bool upper = variable && (variable->isStaticLocal() || variable->getDeclContext()->isFileContext());
            static const std::regex LOWER("[a-z][a-z0-9]*(_[a-z0-9]+)*_?");
            static const std::regex UPPER("[A-Z][A-Z0-9]*(_[A-Z0-9]+)*_?");
            if (!std::regex_match(name, upper ? UPPER : LOWER))
                report(where, "aho-binding-name", declaration->getQualifiedNameAsString(), upper ? "static/globalの構造化束縛はUPPER_SNAKE_CASEです" : "構造化束縛はsnake_caseです");
        }

        const auto* context = declaration->getLexicalDeclContext();
        if (context->isFileContext() && type && !llvm::isa<clang::ClassTemplateSpecializationDecl>(declaration)) {
            const auto* named = llvm::dyn_cast<clang::NamedDecl>(context);
            const std::string scope = where.file + ":" + (named ? named->getQualifiedNameAsString() : "");
            _scopes[scope].push_back({where, declaration->getQualifiedNameAsString(), false});
        }
        return true;
    }

    bool Visitor::VisitFunctionDecl(clang::FunctionDecl* declaration) {
        Position where;
        if (declaration->isImplicit() || !position(declaration->getLocation(), where) || exempt_record(declaration->getLexicalDeclContext()))
            return true;
        const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration);
        if (method && (method->isDefaulted() || method->isDeleted()))
            return true;
        const bool definition = declaration->doesThisDeclarationHaveABody();
        const auto symbol = declaration->getQualifiedNameAsString();
        const auto* context = declaration->getLexicalDeclContext();
        if (definition && context->isRecord())
            report(where, "aho-in-class-definition", symbol, "クラス内は宣言とし、関数本体を型定義の後へ移してください");

        const bool header = llvm::StringRef(where.file).ends_with(".hpp") || llvm::StringRef(where.file).ends_with(".h") ||
                            llvm::StringRef(where.file).ends_with(".inl");
        if (definition && header && !declaration->isInlined() && !declaration->isTemplated() && !declaration->getDeclContext()->isDependentContext())
            report(where, "aho-header-definition", symbol, "通常の非inline・非template関数の定義はsrcへ配置してください");

        if (context->isFileContext()) {
            const auto* named = llvm::dyn_cast<clang::NamedDecl>(context);
            const std::string scope = where.file + ":" + (named ? named->getQualifiedNameAsString() : "");
            _scopes[scope].push_back({where, symbol, definition});
        }
        return true;
    }

    void Visitor::finish() {
        for (auto& [scope, declarations] : _scopes) {
            std::sort(declarations.begin(), declarations.end(), [](const auto& a, const auto& b) { return a.position.offset < b.position.offset; });
            bool implementation = false;
            for (const auto& declaration : declarations) {
                if (!declaration.definition && implementation)
                    report(declaration.position, "aho-declaration-order", declaration.symbol, "同じファイル・namespaceの型定義と関数宣言を実装より上へまとめてください");
                implementation |= declaration.definition;
            }
        }
    }

    void Consumer::HandleTranslationUnit(clang::ASTContext& context) {
        Visitor visitor(context);
        visitor.TraverseDecl(context.getTranslationUnitDecl());
        visitor.finish();
        llvm::json::Array files;
        const auto& source = context.getSourceManager();
        for (auto entry = source.fileinfo_begin(); entry != source.fileinfo_end(); ++entry) {
            llvm::SmallString<256> path(entry->first.getName());
            source.getFileManager().makeAbsolutePath(path);
            llvm::sys::path::remove_dots(path, true);
            const std::string root = ROOT + "/";
            if (path.str().starts_with(root))
                files.push_back(path.str().substr(root.size()).str());
        }
        llvm::outs() << llvm::json::Value(llvm::json::Object{{"coverage", std::move(files)}}) << '\n';
    }

    std::unique_ptr<clang::ASTConsumer> Action::CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef) {
        return std::make_unique<Consumer>();
    }
} // namespace

int main(int argc, const char** argv) {
    auto options = clang::tooling::CommonOptionsParser::create(argc, argv, STYLE_OPTIONS);
    if (!options) {
        llvm::errs() << options.takeError();
        return 2;
    }
    clang::tooling::ClangTool tool(options->getCompilations(), options->getSourcePathList());
    tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster("-resource-dir=" AHO_STYLE_RESOURCE_DIR));
    return tool.run(clang::tooling::newFrontendActionFactory<Action>().get());
}
