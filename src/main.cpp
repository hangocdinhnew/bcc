// clang-format off
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <memory>
#include <FlexLexer.h>
#include "codegen.h"
#include "parser.tab.h"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/IR/LegacyPassManager.h>
// clang-format on

extern int yyparse();
extern std::unique_ptr<bcc::ExprAST> root;

yyFlexLexer lexer;
int yylex() { return lexer.yylex(); }

int main(int argc, char **argv) {
  std::string inputFile;
  std::string outFile = "out";
  bool runIt = false, emitOnlyIR = false, noCleanup = false;

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
    else
      inputFile = arg;
  }

  if (inputFile.empty()) {
    std::cerr << "Usage: bcc [options] <file.b>\n"
              << "Options:\n"
              << "\t--emit-only-ir | -ir\t\tShow IR and exit (no compilation)\n"
              << "\t--run\t\t\t\tRun the compiled binary\n"
              << "\t--out | -o <filename>\t\tSet output binary name (default: "
                 "out)\n"
              << "\t--no-cleanup\t\t\tDoesn't remove all of the IR files\n";
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
  lexer.switch_streams(&input, nullptr);
  yyparse(); // Fills `root`

  // Setup LLVM for printf
  llvm::FunctionType *printfType = llvm::FunctionType::get(
      llvm::Type::getInt32Ty(bcc::TheContext),
      llvm::PointerType::get(llvm::Type::getInt8Ty(bcc::TheContext), 0), true);
  bcc::PrintfFunc = bcc::TheModule->getOrInsertFunction("printf", printfType);

  bcc::generateMain(root);

  if (emitOnlyIR) {
    bcc::TheModule->print(llvm::outs(), nullptr);
    return 0;
  }

  // Compile to object file
  std::string llFile = outFile + ".ll";

  std::error_code EC;
  llvm::raw_fd_ostream out(llFile, EC, llvm::sys::fs::OF_Text);
  if (EC) {
    std::cerr << "Could not write IR: " << EC.message() << "\n";
    return 1;
  }
  bcc::TheModule->print(out, nullptr);
  out.flush();

  // Link to executable
  std::string linkCmd = "clang -x ir " + llFile + " -o " + outFile;
  int rc = system(linkCmd.c_str());
  if (rc != 0) {
    std::cerr << "Linking failed.\n";
    return 1;
  }

  if (!noCleanup) {
    if (std::remove(llFile.c_str()) != 0) {
      std::cerr << "Warning: Failed to delete intermediate file: " << llFile
                << "\n";
    }
  }

  if (runIt) {
    std::string execCmd = "./" + outFile;
    return system(execCmd.c_str());
  }

  return 0;
}
