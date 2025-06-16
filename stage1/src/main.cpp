// clang-format off
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <FlexLexer.h>
#include "ast.h"
#include "codegen.h"
#include "parser.tab.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
// clang-format on

extern int yyparse();
extern std::unique_ptr<ExprAST> root;
std::vector<std::unique_ptr<ExprAST>> externs;
std::vector<std::unique_ptr<FunctionAST>> functions;

yyFlexLexer lexer;
int yylex() { return lexer.yylex(); }

int main(int argc, char **argv) {
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

  std::string inputFile;
  std::string outFile;
  bool runIt = false, emitOnlyIR = false, noCleanup = false;
  bool compileOnly = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit-only-ir" || arg == "-ir")
      emitOnlyIR = true;
    else if (arg == "--run")
      runIt = true;
    else if ((arg == "--out" || arg == "-o") && i + 1 < argc)
      outFile = argv[++i];
    else if (arg == "--no-cleanup")
      noCleanup = true;
    else if (arg == "--compile-only" || arg == "-C")
      compileOnly = true;
    else {
      inputFile = arg;
#ifdef _WIN32
      outFile =
          std::filesystem::path(inputFile).replace_extension(".exe").string();
#else
      outFile = std::filesystem::path(inputFile).stem().string();
#endif
      if (inputFile.size() < 3 ||
          inputFile.substr(inputFile.size() - 3) != ".bc") {
        std::cerr << "Error: input file must have an .bc extension!\n";
        return 1;
      }
    }
  }

  if (inputFile.empty()) {
    std::cerr
        << "Usage: bcc [options] <file.b>\n"
        << "Options:\n"
        << "\t--emit-only-ir | -ir\t\tEmit IR and exit (no compilation)\n"
        << "\t--run\t\t\t\tRun the compiled binary\n"
        << "\t--out | -o <filename>\t\tSet output binary name (default: out)\n"
        << "\t--no-cleanup\t\t\tDoesn't remove all of the IR files\n"
        << "\t--compile-only | -C\t\tOnly compiles without linking.\n";
    return 1;
  }

  std::ifstream in(inputFile);
  if (!in) {
    std::cerr << "Could not open file: " << inputFile << "\n";
    return 1;
  }

  std::string code((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  std::istringstream input(code);

  TheContext = LLVMContextCreate();
  TheModule = LLVMModuleCreateWithNameInContext("B+ Compiler", TheContext);
  Builder = LLVMCreateBuilderInContext(TheContext);

  lexer.switch_streams(&input, nullptr);
  yyparse();

  for (auto &ext : externs) {
    ext->codegen(TheModule, Builder);
  }

  for (auto &func : functions) {
    func->codegen(TheModule, Builder);
  }

  char *errStr = nullptr;
  if (LLVMVerifyModule(TheModule, LLVMPrintMessageAction, &errStr)) {
    std::cerr << "Generated IR is invalid: " << errStr << "\n";
    LLVMDisposeMessage(errStr);
    return 1;
  }

  if (emitOnlyIR) {
    std::string irFileName = outFile + ".ll";
    if (LLVMPrintModuleToFile(TheModule, irFileName.c_str(), &errStr)) {
      std::cerr << "Could not write IR to file: " << errStr << "\n";
      LLVMDisposeMessage(errStr);
      return 1;
    }
    std::cout << "Successfully emitted IR: " << irFileName << "\n";
    LLVMDisposeBuilder(Builder);
    LLVMDisposeModule(TheModule);
    LLVMContextDispose(TheContext);
    return 0;
  }

  std::string targetTriple = LLVMGetDefaultTargetTriple();
  LLVMSetTarget(TheModule, targetTriple.c_str());

  char *error = nullptr;
  LLVMTargetRef target;
  if (LLVMGetTargetFromTriple(targetTriple.c_str(), &target, &error)) {
    std::cerr << "Failed to lookup target: " << error << "\n";
    LLVMDisposeMessage(error);
    return 1;
  }

  LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
      target, targetTriple.c_str(), "generic", "", LLVMCodeGenLevelDefault,
      LLVMRelocPIC, LLVMCodeModelDefault);

  LLVMSetModuleDataLayout(TheModule, LLVMCreateTargetDataLayout(tm));

#ifdef _WIN32
  std::string objFileName = outFile + ".obj";
#else
  std::string objFileName = outFile + ".o";
#endif

  if (LLVMTargetMachineEmitToFile(tm, TheModule, objFileName.c_str(),
                                  LLVMObjectFile, &error)) {
    std::cerr << "Could not emit object file: " << error << "\n";
    LLVMDisposeMessage(error);
    return 1;
  }

  if (!compileOnly) {
    std::string linkCmd = "clang " + objFileName + " -o " + outFile;
    int linkResult = system(linkCmd.c_str());
    if (linkResult != 0) {
      std::cerr << "Linking failed!\n";
      return 1;
    }
  }

  if (runIt) {
#ifdef _WIN32
    std::string execCmd = ".\\" + outFile;
#else
    std::string execCmd = "./" + outFile;
#endif
    return system(execCmd.c_str());
  }

  LLVMDisposeBuilder(Builder);
  LLVMDisposeModule(TheModule);
  LLVMDisposeTargetMachine(tm);
  LLVMContextDispose(TheContext);
  LLVMDisposeMessage(error);
  LLVMDisposeMessage(errStr);

  return 0;
}
