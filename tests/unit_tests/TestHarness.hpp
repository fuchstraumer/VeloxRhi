#pragma once
#ifndef VELOX_TESTS_TEST_HARNESS_HPP
#define VELOX_TESTS_TEST_HARNESS_HPP
#include <cstddef>
#include <string_view>

// Not GTest or Catch2: a counter, a comparison helper, and a nonzero exit code. Each test executable
// runs by hand or under ctest, and prints only failures plus a one-line summary.
namespace velox::tests
{

class TestRunner final
{
public:
    explicit TestRunner(std::string_view suite_name) noexcept;

    /** @brief Heading for subsequent checks, printed only if one of them fails */
    void BeginSection(std::string_view name) noexcept;

    void Check(bool condition, std::string_view description) noexcept;

    /** @brief Relative, scaled by operand magnitude; degrades to absolute when expected is zero */
    void CheckNear(float actual,
                   float expected,
                   float tolerance,
                   std::string_view description) noexcept;

    /** @brief Prints the summary; returns what main() should hand back */
    int Report() const noexcept;

    size_t Failures() const noexcept;

private:
    void ReportFailure(std::string_view description) noexcept;

    std::string_view suiteName;
    std::string_view currentSection;
    bool sectionHeadingPrinted;
    size_t checksRun;
    size_t failures;
};

/** @brief Which backend this executable was built against, for the summary line */
std::string_view ActiveBackendName() noexcept;

} // namespace velox::tests

#endif // !VELOX_TESTS_TEST_HARNESS_HPP
