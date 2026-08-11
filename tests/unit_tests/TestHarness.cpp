#include "TestHarness.hpp"
#include "Math.hpp"
#include <cmath>
#include <print>

namespace velox::tests
{

TestRunner::TestRunner(std::string_view suite_name) noexcept :
    suiteName{ suite_name },
    currentSection{ "" },
    sectionHeadingPrinted{ false },
    checksRun{ 0u },
    failures{ 0u }
{
}

void TestRunner::BeginSection(std::string_view name) noexcept
{
    currentSection = name;
    sectionHeadingPrinted = false;
}

void TestRunner::ReportFailure(std::string_view description) noexcept
{
    if (!sectionHeadingPrinted && !currentSection.empty())
    {
        std::println("  [{}]", currentSection);
        sectionHeadingPrinted = true;
    }
    std::println("    FAIL: {}", description);
}

void TestRunner::Check(bool condition, std::string_view description) noexcept
{
    ++checksRun;
    if (condition)
    {
        return;
    }
    ++failures;
    ReportFailure(description);
}

void TestRunner::CheckNear(float actual,
                           float expected,
                           float tolerance,
                           std::string_view description) noexcept
{
    ++checksRun;

    const float difference = std::fabs(actual - expected);
    const float scale = 1.0f + std::fabs(actual) + std::fabs(expected);
    const bool withinTolerance = difference <= tolerance * scale;

    if (withinTolerance)
    {
        return;
    }

    ++failures;
    ReportFailure(description);
    std::println("          expected {}, got {} (difference {})", expected, actual, difference);
}

int TestRunner::Report() const noexcept
{
    if (failures == 0u)
    {
        std::println("{} [{}]: {} checks passed", suiteName, ActiveBackendName(), checksRun);
        return 0;
    }

    std::println("{} [{}]: {} of {} checks FAILED", suiteName, ActiveBackendName(), failures, checksRun);
    return 1;
}

size_t TestRunner::Failures() const noexcept
{
    return failures;
}

std::string_view ActiveBackendName() noexcept
{
#if VX_MATH_BACKEND_WASM
#if defined(VX_MATH_RELAXED_SIMD)
    return "wasm simd128 + relaxed";
#else
    return "wasm simd128";
#endif
#else
    return "directxmath";
#endif
}

} // namespace velox::tests
