#pragma once
#ifndef VELOX_TESTS_TEST_HARNESS_HPP
#define VELOX_TESTS_TEST_HARNESS_HPP
#include <cstddef>
#include <string_view>

// Deliberately not GTest or Catch2. These tests exist because silently broken math is expensive to
// find through rendering code, not because the project wants a test framework - so the harness is a
// counter, a comparison helper, and a nonzero exit code. Each test executable is runnable by hand and
// prints only failures plus a one-line summary.
namespace velox::tests
{

class TestRunner final
{
public:
    explicit TestRunner(std::string_view suite_name) noexcept;

    /** @brief Groups subsequent checks under a heading, printed only if something in it fails */
    void BeginSection(std::string_view name) noexcept;

    void Check(bool condition, std::string_view description) noexcept;

    /**
     * @brief Relative comparison, scaled by operand magnitude so it stays meaningful for both tiny
     * and large values. An exact-zero expectation falls back to an absolute comparison.
     */
    void CheckNear(float actual,
                   float expected,
                   float tolerance,
                   std::string_view description) noexcept;

    /** @brief Prints the summary and returns the value main() should hand back to the shell */
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

/** @brief Which SIMD backend this executable was compiled against, for the summary line */
std::string_view ActiveBackendName() noexcept;

} // namespace velox::tests

#endif // !VELOX_TESTS_TEST_HARNESS_HPP
