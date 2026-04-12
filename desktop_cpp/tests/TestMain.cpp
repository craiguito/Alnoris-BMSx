#include "TestHarness.h"

#include <exception>
#include <iostream>
#include <utility>

namespace cad::tests {

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

TestRegistrar::TestRegistrar(const char* name, void (*function)())
{
    registry().push_back({name, function});
}

[[noreturn]] void fail(const char* file, int line, const std::string& message)
{
    throw TestFailure(std::string(file) + ":" + std::to_string(line) + ": " + message);
}

int runAllTests()
{
    int failures = 0;

    for (const TestCase& test : registry()) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << test.name << '\n';
            std::cerr << "  " << error.what() << '\n';
            ++failures;
        } catch (...) {
            std::cerr << "[FAIL] " << test.name << '\n';
            std::cerr << "  unknown error\n";
            ++failures;
        }
    }

    if (failures == 0) {
        std::cout << registry().size() << " tests passed.\n";
        return 0;
    }

    std::cerr << failures << " of " << registry().size() << " tests failed.\n";
    return 1;
}

} // namespace cad::tests

int main()
{
    return cad::tests::runAllTests();
}
