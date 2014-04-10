#include "testCommon.h"
#include "fake_qobjectdefs.h"

#include "../Qt5SignalSlotSyntaxConverter.h"
#include "../llvmutils.h"
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Rewrite/Frontend/FixItRewriter.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/FileSystemStatCache.h>
#include <clang/Frontend/FrontendActions.h>
#include <llvm/Support/Signals.h>

#include <boost/algorithm/string/split.hpp>

#include <fstream>
#include <sstream>


using namespace clang::tooling;
using llvm::outs;

std::string readFile(const std::string& name) {
    std::string filePath = getRealPath(name);
    std::ifstream t(filePath);
    if (!t.good()) {
        throw std::runtime_error(std::string("Failed to open ") + filePath);
    }
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string ret = buffer.str();
    if (ret.empty()) {
        throw std::runtime_error(std::string("Did not read any data from ") + filePath);
    }
    return buffer.str();
}

typedef std::vector<std::string> StringList;
static const auto isNewline = [](char c) {return c == '\n';};

static bool runTool(clang::FrontendAction *toolAction, const std::string code, const AdditionalFiles& additionalFiles, const std::vector<std::string>& extraOptions) {
    std::vector<std::string> commands { "clang", "-Wall", "-fsyntax-only", "-std=c++11", "-I", INCLUDE_DIR };
    for (const auto& opt : extraOptions) {
        commands.push_back(opt);
    }
    commands.push_back(FILE_NAME);
    clang::FileSystemOptions opt;
    opt.WorkingDir = BASE_DIR;
    clang::FileManager* files = new clang::FileManager{ opt };
    ToolInvocation Invocation(commands, toolAction, files);
    // this seems to be fixed in later clang versions: for some reason free() is called on these strings, although StringRef should be non-owning
    Invocation.mapVirtualFile(FILE_NAME, code);
    Invocation.mapVirtualFile(INCLUDE_DIR "QObject", fakeQObjectCode);
    Invocation.mapVirtualFile(INCLUDE_DIR "qobjectdefs.h", fakeQObjectDefsCode);
    for (auto file : additionalFiles) {
        Invocation.mapVirtualFile(file.first, file.second);
    }
    return Invocation.run();
}

bool codeCompiles(const std::string& code, const AdditionalFiles& additionalFiles, const std::vector<std::string>& extraOptions) {
    auto action = new clang::SyntaxOnlyAction();
    return runTool(action, code, additionalFiles, extraOptions);
}

int testMain(std::string input, std::string expected, int found, int converted, const std::vector<const char*>& converterOptions) {
    StringList refactoringFiles { FILE_NAME };
    MatchFinder matchFinder;
    Replacements replacements;
    ConnectConverter converter(&replacements, refactoringFiles);
    converter.setupMatchers(&matchFinder);
    auto factory = newFrontendActionFactory(&matchFinder, converter.sourceCallback());
    auto action = factory->create();

    bool success = runTool(action, input, {}, {});
    if (!success) {
        outs() << "Failed to run conversion!\n";
        return 1;
    }
    outs() << "\n";
    converter.printStats();
    outs().flush();
    if (converter.matchesFound() <= 0) {
        colouredOut(llvm::raw_ostream::RED) << "Failure: No matches found!\n";
        return 1;
    }
    if (converter.matchesFailed() > 0) {
        colouredOut(llvm::raw_ostream::RED) << "Failure: Failed to convert " <<  converter.matchesFailed() << " matches!\n";
        return 1;
    }
    if (converter.matchesSkipped() > 0) {
        colouredOut(llvm::raw_ostream::RED) << "Failure: Skipped " <<  converter.matchesSkipped() << " matches!\n";
        return 1;
    }
    if (found != converter.matchesFound()) {
        colouredOut(llvm::raw_ostream::RED) << "Failure: Expected to find " << found
                << " matches, but found " << converter.matchesFound() << "!\n";
        return 1;
    }
    if (converted != converter.matchesFound()) {
        colouredOut(llvm::raw_ostream::RED) << "Failure: Expected to convert " << found
                << " matches, but converted " << converter.matchesConverted() << "!\n";
        return 1;
    }

    ssize_t offsetAdjust = 0;
    for (const Replacement& rep : replacements) {
        if (rep.getFilePath() != FILE_NAME) {
            outs() << "Attempting to write to illegal file:" << rep.getFilePath() << "\n";
            success = false;
        }
        input.replace(size_t(offsetAdjust + ssize_t(rep.getOffset())), rep.getLength(), rep.getReplacementText().str());
        offsetAdjust += ssize_t(rep.getReplacementText().size()) - ssize_t(rep.getLength());
    }
    outs() << "\nComparing result with expected result...";
    if (input == expected) {
        colouredOut(llvm::raw_ostream::GREEN) << " Success!\n";
        return 0;
    }
    outs() << "\n\n";
    // something is wrong
    StringList expectedLines;
    boost::split(expectedLines, expected, isNewline);
    StringList resultLines;
    boost::split(resultLines, input, isNewline);
    auto lineCount = std::min(resultLines.size(), expectedLines.size());
    for (size_t i = 0; i < lineCount; ++i) {
        if (expectedLines[i] != resultLines[i]) {
            outs() << i << " expected:";
            colouredOut(llvm::raw_ostream::GREEN).writeEscaped(expectedLines[i]) << "\n";
            outs() << i << " result  :";
            colouredOut(llvm::raw_ostream::RED).writeEscaped(resultLines[i]) << "\n";
        } else {
            //outs() << i << "  okay   :" << expectedLines[i] << "\n";
            // nothing
        }
    }
    if (resultLines.size() > lineCount) {
        outs() << "Additional lines in result:\n";
        for (size_t i = lineCount; i < resultLines.size(); ++i) {
            colouredOut(llvm::raw_ostream::RED) << i << " = '" << resultLines[i] << "'\n";
        }
    }
    if (expectedLines.size() > lineCount) {
        outs() << "Additional lines in expected:\n";
        for (size_t i = lineCount; i < expectedLines.size(); ++i) {
            colouredOut(llvm::raw_ostream::RED) << i << " = '" << expectedLines[i] << "'\n";
        }
    }
    colouredOut(llvm::raw_ostream::RED) << "\nFailed!\n";
    return 1;
}
