#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "parser/ASTPrinter.h"
#include "semantic/SemanticAnalyzer.h"
#include "bytecode/CodeGenerator.h"
#include "vm/VM.h"
#include "common/Error.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

using namespace jscpp;

static void printUsage() {
    std::cout << "Java-Safe C++ Compiler & VM (jscpp)\n"
              << "Usage:\n"
              << "  jscpp run <source_or_bytecode_file> [--trace] [--stats]\n"
              << "  jscpp compile <source_file> [-o <output_file>]\n"
              << "  jscpp dump <source_or_bytecode_file>\n"
              << "  jscpp tokens <source_file>     show the lexer output\n"
              << "  jscpp ast <source_file>        show the parse tree\n"
              << "  jscpp --help\n";
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3); // UTF-8 byte-order mark
    return text;
}

static bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Source text of the file being compiled, kept so errors can quote the offending line
static std::string currentSource;

static void reportCompilerError(const CompilerError& e) {
    const SourceLocation& loc = e.getLocation();
    std::cerr << loc.file << ":" << loc.line << ":" << loc.column << ": error: " << e.what() << "\n";

    std::istringstream lines(currentSource);
    std::string text;
    for (int i = 1; std::getline(lines, text); ++i) {
        if (i == loc.line) {
            if (!text.empty() && text.back() == '\r') text.pop_back();
            std::cerr << std::setw(5) << loc.line << " | " << text << "\n"
                      << "      | " << std::string(loc.column > 1 ? loc.column - 1 : 0, ' ') << "^\n";
            break;
        }
    }
}

static std::unique_ptr<Program> parseSource(const std::string& sourceFile) {
    currentSource = readFile(sourceFile);
    Lexer lexer(currentSource, sourceFile);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

static BytecodeProgram compileSource(const std::string& sourceFile) {
    auto ast = parseSource(sourceFile);

    SemanticAnalyzer semantic;
    semantic.analyze(*ast);

    CodeGenerator codegen(semantic);
    return codegen.generate(*ast);
}

static BytecodeProgram loadOrCompile(const std::string& inputFile) {
    if (endsWith(inputFile, ".jbc")) {
        return BytecodeProgram::load(inputFile);
    }
    return compileSource(inputFile);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }
    if (argc < 3) {
        std::cerr << "Error: missing input file.\n";
        printUsage();
        return 1;
    }
    std::string inputFile = argv[2];

    try {
        if (command == "compile") {
            std::string outputFile = "out.jbc";
            for (int i = 3; i < argc; ++i) {
                if (std::string(argv[i]) == "-o" && i + 1 < argc) {
                    outputFile = argv[++i];
                }
            }
            auto prog = compileSource(inputFile);
            prog.save(outputFile);
            std::cout << "Successfully compiled to " << outputFile << "\n";

        } else if (command == "run") {
            bool trace = false;
            bool stats = false;
            for (int i = 3; i < argc; ++i) {
                std::string flag = argv[i];
                if (flag == "--trace") trace = true;
                else if (flag == "--stats") stats = true;
                else {
                    std::cerr << "Unknown option: " << flag << "\n";
                    return 1;
                }
            }

            BytecodeProgram prog = loadOrCompile(inputFile);
            VM vm(prog, trace);
            int status = vm.run();
            if (stats) vm.printStats(std::cout);
            return status;

        } else if (command == "dump") {
            loadOrCompile(inputFile).dump(std::cout);

        } else if (command == "tokens") {
            currentSource = readFile(inputFile);
            Lexer lexer(currentSource, inputFile);
            for (const Token& token : lexer.tokenize()) {
                std::cout << std::setw(4) << token.loc.line << ":" << std::left << std::setw(4) << token.loc.column
                          << std::setw(16) << Token::typeToString(token.type) << std::right << token.lexeme << "\n";
            }

        } else if (command == "ast") {
            auto ast = parseSource(inputFile);
            ASTPrinter printer(std::cout);
            ast->accept(printer);

        } else {
            std::cerr << "Unknown command: " << command << "\n";
            printUsage();
            return 1;
        }
    } catch (const CompilerError& e) {
        reportCompilerError(e);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
