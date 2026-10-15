#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "bytecode/CodeGenerator.h"
#include "vm/VM.h"
#include "common/Error.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace jscpp;

void printUsage() {
    std::cout << "Java-Safe C++ Compiler & VM (jscpp)\n"
              << "Usage:\n"
              << "  jscpp compile <source_file> [-o <output_file>]\n"
              << "  jscpp run <source_or_bytecode_file> [--trace]\n"
              << "  jscpp dump <bytecode_file>\n"
              << "  jscpp --help\n";
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

BytecodeProgram compileSource(const std::string& sourceFile) {
    std::string source = readFile(sourceFile);

    Lexer lexer(source, sourceFile);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    auto ast = parser.parse();

    SemanticAnalyzer semantic;
    semantic.analyze(*ast);

    CodeGenerator codegen(semantic);
    return codegen.generate(*ast);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "--help" || command == "-h") {
            printUsage();
            return 0;
        } else if (command == "compile") {
            if (argc < 3) {
                std::cerr << "Error: missing source file.\n";
                return 1;
            }
            std::string sourceFile = argv[2];
            std::string outputFile = "out.jbc";

            for (int i = 3; i < argc; ++i) {
                if (std::string(argv[i]) == "-o" && i + 1 < argc) {
                    outputFile = argv[++i];
                }
            }

            auto prog = compileSource(sourceFile);
            prog.save(outputFile);
            std::cout << "Successfully compiled to " << outputFile << "\n";

        } else if (command == "run") {
            if (argc < 3) {
                std::cerr << "Error: missing input file.\n";
                return 1;
            }
            std::string inputFile = argv[2];
            bool trace = false;

            for (int i = 3; i < argc; ++i) {
                if (std::string(argv[i]) == "--trace") {
                    trace = true;
                }
            }

            BytecodeProgram prog;
            if (endsWith(inputFile, ".jbc")) {
                prog = BytecodeProgram::load(inputFile);
            } else {
                prog = compileSource(inputFile);
            }

            VM vm(prog, trace);
            vm.run();

        } else if (command == "dump") {
            if (argc < 3) {
                std::cerr << "Error: missing bytecode file.\n";
                return 1;
            }
            std::string inputFile = argv[2];
            auto prog = BytecodeProgram::load(inputFile);
            prog.dump(std::cout);

        } else {
            std::cerr << "Unknown command: " << command << "\n";
            printUsage();
            return 1;
        }
    } catch (const CompilerError& e) {
        std::cerr << "Compiler Error at " << e.getLocation().file << ":"
                  << e.getLocation().line << ":" << e.getLocation().column << "\n"
                  << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
