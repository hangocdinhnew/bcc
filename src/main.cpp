// clang-format off
#include <iostream>
#include <fstream>
#include <llvm/Support/CodeGen.h>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>
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
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

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
    std::cerr << "Usage: bcc [options] <file.b>\n"
              << "Options:\n"
              << "\t--emit-only-ir | -ir\t\tEmit IR and exit (no compilation)\n"
              << "\t--run\t\t\t\tRun the compiled binary\n"
              << "\t--out | -o <filename>\t\tSet output binary name (default: "
                 "out)\n"
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
  lexer.switch_streams(&input, nullptr);
  yyparse();

  for (auto &ext : bcc::externs) {
    ext->codegen();
  }

  for (auto &func : bcc::functions) {
    func->codegen();
  }

  if (!bcc::FunctionProtos.count("main")) {
    return 1;
  }

  if (emitOnlyIR) {
    std::error_code ec;
    std::string irFileName = outFile + ".ll";
    llvm::raw_fd_ostream irOut(irFileName, ec, llvm::sys::fs::OF_None);

    if (ec) {
      std::cerr << "Could not open IR file for writing! Error: " << ec.message()
                << "\n";
      return 1;
    }

    bcc::TheModule->print(irOut, nullptr);
    irOut.flush();

    std::cout << "Successfully emitted IR: " << irFileName << "\n";
    return 0;
  }

  auto targetTriple = llvm::sys::getDefaultTargetTriple();
  bcc::TheModule->setTargetTriple(targetTriple);

  std::string error;
  auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
  if (!target) {
    std::cerr << "Failed to lookup target! Error: " << error << "\n";
    return 1;
  }

  auto cpu = "generic";
  auto features = "";

  llvm::TargetOptions opt;
  auto RM = llvm::Reloc::PIC_;
  auto targetMachine =
      target->createTargetMachine(targetTriple, cpu, features, opt, RM);
  bcc::TheModule->setDataLayout(targetMachine->createDataLayout());

#ifdef _WIN32
  std::string objFileName = outFile + ".obj";
#else
  std::string objFileName = outFile + ".o";
#endif

  std::error_code ec;
  llvm::raw_fd_ostream dest(objFileName, ec, llvm::sys::fs::OF_None);

  if (ec) {
    std::cerr << "Could not open file! Error: " << ec.message() << "\n";
    return 1;
  }

  llvm::legacy::PassManager pass;
  if (targetMachine->addPassesToEmitFile(pass, dest, nullptr,
                                         llvm::CodeGenFileType::ObjectFile)) {
    std::cerr << "TargetMachine can't emit a file of this type!\n";
  }

  pass.run(*bcc::TheModule);
  dest.flush();

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
    std::string execCmd = ".\" + outFile;
#else
    std::string execCmd = "./" + outFile;
#endif

        return system(execCmd.c_str());
  }

  return 0;
}
