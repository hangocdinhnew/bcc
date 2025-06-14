// clang-format off
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include "parser.h"
#include "codegen.h"
// clang-format on

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: bcc <input.b> [-o output]\n";
    return 1;
  }

  std::string inputFile = argv[1];
  std::string outFile = "a.out";
  bool emitAsm = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) {
      outFile = argv[++i];
    } else if (arg == "-S") {
      emitAsm = true;
    }
  }

  std::ifstream in(inputFile);
  if (!in) {
    std::cerr << "Error: Cannot open file " << inputFile << "\n";
    return 1;
  }

  std::string source((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());

  Parser parser;
  if (!parser.parse(source)) {
    std::cerr << "Parsing failed!\n";
    return 1;
  }

  CodeGenerator codegen;
  std::string asmCode = codegen.generate(parser.getProgram());

  if (emitAsm) {
    std::ofstream asmOut(outFile);
    asmOut << asmCode;
    std::cout << "Assembly written to " << outFile << "\n";
  } else {
    std::string asmFile = outFile + ".s";
    std::ofstream asmOut(asmFile);
    asmOut << asmCode;
    asmOut.close();

    std::string assemble = "as --64 -o " + outFile + ".o " + asmFile;
    if (system(assemble.c_str()) != 0) {
      std::cerr << "Assembling failed!\n";
      return 1;
    }

    std::string link = "ld -m elf_x86_64 -o " + outFile + " " + outFile + ".o";
    if (system(link.c_str()) != 0) {
      std::cerr << "Linking failed!\n";
      return 1;
    }

    std::cout << "Successfully built: " << outFile << "\n";
  }

  return 0;
}
