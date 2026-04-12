#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cad::tests {

struct TestCase
{
    const char* name;
    void (*function)();
};

class TestFailure : public std::runtime_error
{
public:
    explicit TestFailure(std::string message)
        : std::runtime_error(std::move(message))
    {
    }
};

std::vector<TestCase>& registry();

class TestRegistrar
{
public:
    TestRegistrar(const char* name, void (*function)());
};

[[noreturn]] void fail(const char* file, int line, const std::string& message);
int runAllTests();

template <typename T>
std::string toString(const T& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline std::string toString(const std::string& value)
{
    return value;
}

inline std::string toString(const char* value)
{
    return value == nullptr ? std::string{"<null>"} : std::string{value};
}

template <typename T, typename U>
void expectEqual(
    const T& actual,
    const U& expected,
    const char* actual_expression,
    const char* expected_expression,
    const char* file,
    int line)
{
    if (!(actual == expected)) {
        std::ostringstream message;
        message << "expected " << actual_expression << " == " << expected_expression
                << " but got [" << toString(actual) << "] vs [" << toString(expected) << "]";
        fail(file, line, message.str());
    }
}

template <typename T, typename U>
void expectNotEqual(
    const T& actual,
    const U& unexpected,
    const char* actual_expression,
    const char* unexpected_expression,
    const char* file,
    int line)
{
    if (actual == unexpected) {
        std::ostringstream message;
        message << "expected " << actual_expression << " != " << unexpected_expression
                << " but both were [" << toString(actual) << "]";
        fail(file, line, message.str());
    }
}

} // namespace cad::tests

#define CAD_TEST(name)                                                                                                 \
    static void name();                                                                                                \
    static ::cad::tests::TestRegistrar name##_registrar(#name, &name);                                                 \
    static void name()

#define CAD_EXPECT(condition)                                                                                          \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            ::cad::tests::fail(__FILE__, __LINE__, "expected " #condition);                                            \
        }                                                                                                              \
    } while (false)

#define CAD_EXPECT_EQ(actual, expected)                                                                                \
    ::cad::tests::expectEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define CAD_EXPECT_NE(actual, unexpected)                                                                              \
    ::cad::tests::expectNotEqual((actual), (unexpected), #actual, #unexpected, __FILE__, __LINE__)
