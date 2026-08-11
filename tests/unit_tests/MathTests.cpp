#include "TestHarness.hpp"
#include "Math.hpp"
#include "math/MathHashes.hpp"
#include <cmath>
#include <print>
#include <string_view>
#include <unordered_map>

// Identities and known-answer cases rather than golden values, because the failures worth catching
// look plausible: a transposed shuffle index, a dropped cofactor sign, a coefficient off in the fifth
// decimal. A wrong dot product still returns a number of roughly the right size.
namespace
{

using namespace velox::math;
using velox::tests::TestRunner;

constexpr float k_Tight = 2e-4f;
constexpr float k_Loose = 1e-3f;
constexpr float k_Pi = std::numbers::pi_v<float>;

bool MatrixNear(const Matrix& lhs, const Matrix& rhs, float tolerance) noexcept
{
    for (size_t row = 0u; row < 4u; ++row)
    {
        for (size_t column = 0u; column < 4u; ++column)
        {
            const float difference = std::fabs(lhs[row, column] - rhs[row, column]);
            const float scale = 1.0f + std::fabs(lhs[row, column]) + std::fabs(rhs[row, column]);
            if (difference > tolerance * scale)
            {
                return false;
            }
        }
    }
    return true;
}

bool VectorNear(Vector actual, Vector expected, float tolerance) noexcept
{
    return actual.CompareNearEqual(expected, Vector::Replicate(tolerance)).AllTrue<4>();
}

// Not a similarity transform, so a wrong cofactor lane cannot cancel out by luck
Matrix MakeAwkwardMatrix() noexcept
{
    return Matrix{ 2.0f,  3.0f,  1.0f,  0.5f,
                   -1.0f, 4.0f,  2.0f,  0.25f,
                   3.0f,  1.0f,  -5.0f, 0.75f,
                   1.5f,  -2.0f, 0.5f,  2.0f };
}

void TestMasksAndSelect(TestRunner& runner)
{
    runner.BeginSection("masks and select");

    const Vector ascending{ 1.0f, 2.0f, 3.0f, 4.0f };
    const Vector descending{ 4.0f, 3.0f, 2.0f, 1.0f };

    runner.Check(ascending.CompareLess(descending).LaneBits() == 0x3u, "CompareLess lane bits");
    runner.Check(ascending.CompareGreater(descending).LaneBits() == 0xCu, "CompareGreater lane bits");
    runner.Check(ascending.CompareEqual(ascending).LaneBits() == 0xFu, "CompareEqual with self");
    runner.Check(ascending.CompareNotEqual(ascending).LaneBits() == 0x0u, "CompareNotEqual with self");
    runner.Check(ascending.CompareLessOrEqual(ascending).AllTrue<4>(), "CompareLessOrEqual with self");
    runner.Check(ascending.CompareGreaterOrEqual(ascending).AllTrue<4>(),
                 "CompareGreaterOrEqual with self");

    // the width parameter has to actually restrict which lanes are consulted
    const VectorMask lowTwo = ascending.CompareLess(descending);
    runner.Check(lowTwo.AllTrue<2>(), "AllTrue<2> consults only lanes 0 and 1");
    runner.Check(!lowTwo.AllTrue<3>(), "AllTrue<3> catches lane 2");
    runner.Check(!lowTwo.AllTrue<4>(), "AllTrue<4> catches lanes 2 and 3");
    runner.Check(lowTwo.AnyTrue<4>(), "AnyTrue<4> sees lanes 0 and 1");
    runner.Check(!ascending.CompareGreater(descending).AnyTrue<2>(),
                 "AnyTrue<2> does not see lanes 2 and 3");

    runner.Check(VectorMask::AllSet().AllTrue<4>(), "AllSet is all true");
    runner.Check(!VectorMask::AllClear().AnyTrue<4>(), "AllClear is none true");

    runner.Check((lowTwo & ~lowTwo).LaneBits() == 0x0u, "mask and complement is empty");
    runner.Check((lowTwo | ~lowTwo).LaneBits() == 0xFu, "mask or complement is full");
    runner.Check((lowTwo ^ lowTwo).LaneBits() == 0x0u, "mask xor itself is empty");

    // the backends spell this in opposite orders underneath, so pin the operand order down
    const Vector picked = Select(lowTwo, descending, ascending);
    runner.CheckNear(picked.x(), 1.0f, k_Tight, "Select takes when_set where the mask is set");
    runner.CheckNear(picked.z(), 2.0f, k_Tight, "Select takes when_clear where the mask is clear");

    runner.Check(Vector::QuietNaN().IsNaN().AllTrue<4>(), "QuietNaN is NaN");
    runner.Check(!ascending.IsNaN().AnyTrue<4>(), "a finite vector is not NaN");
    runner.Check(Vector::Infinity().IsInfinite().AllTrue<4>(), "Infinity is infinite");
    runner.Check((-Vector::Infinity()).IsInfinite().AllTrue<4>(), "negative infinity is infinite");
    runner.Check(!ascending.IsInfinite().AnyTrue<4>(), "a finite vector is not infinite");

    const Vector nudged{ 1.0f + 1e-6f, 2.0f, 3.0f, 4.0f };
    runner.Check(ascending.CompareNearEqual(nudged, Vector::Replicate(1e-4f)).AllTrue<4>(),
                 "CompareNearEqual inside tolerance");
    runner.Check(!ascending.CompareNearEqual(descending, Vector::Replicate(1e-4f)).AnyTrue<4>(),
                 "CompareNearEqual outside tolerance");
}

void TestRoundingAndLanes(TestRunner& runner)
{
    runner.BeginSection("rounding and lane movement");

    // both backends round halves to even
    const Vector halves{ 0.5f, 1.5f, 2.5f, -0.5f };
    const Vector rounded = halves.Round();
    runner.CheckNear(rounded.x(), 0.0f, k_Tight, "Round(0.5) ties to even");
    runner.CheckNear(rounded.y(), 2.0f, k_Tight, "Round(1.5) ties to even");
    runner.CheckNear(rounded.z(), 2.0f, k_Tight, "Round(2.5) ties to even");

    const Vector mixed{ 1.7f, -1.7f, 1.2f, -1.2f };
    runner.CheckNear(mixed.Truncate().y(), -1.0f, k_Tight, "Truncate goes toward zero");
    runner.CheckNear(mixed.Floor().y(), -2.0f, k_Tight, "Floor goes toward negative infinity");
    runner.CheckNear(mixed.Ceil().y(), -1.0f, k_Tight, "Ceil goes toward positive infinity");

    // truncating, so the remainder keeps the dividend's sign
    const Vector modded = Vector{ 7.0f, -7.0f, 5.5f, 1.0f }.Mod(Vector::Replicate(3.0f));
    runner.CheckNear(modded.x(), 1.0f, k_Tight, "Mod of a positive dividend");
    runner.CheckNear(modded.y(), -1.0f, k_Tight, "Mod keeps the dividend's sign");
    runner.CheckNear(modded.z(), 2.5f, k_Tight, "Mod of a fractional dividend");

    // must wrap into [-pi, pi] including the negative half, which truncation would get wrong
    const Vector angles{ 2.0f * k_Pi + 1.0f, -2.0f * k_Pi - 1.0f, 0.5f, 4.0f * k_Pi };
    const Vector wrapped = angles.ModAngles();
    runner.CheckNear(wrapped.x(), 1.0f, k_Loose, "ModAngles wraps 2pi+1 down to 1");
    runner.CheckNear(wrapped.y(), -1.0f, k_Loose, "ModAngles wraps -2pi-1 up to -1");
    runner.CheckNear(wrapped.z(), 0.5f, k_Loose, "ModAngles leaves an in-range angle alone");
    runner.CheckNear(wrapped.w(), 0.0f, k_Loose, "ModAngles wraps 4pi to 0");

    const Vector ascending{ 1.0f, 2.0f, 3.0f, 4.0f };
    const Vector descending{ 4.0f, 3.0f, 2.0f, 1.0f };

    runner.CheckNear(ascending.SplatX().w(), 1.0f, k_Tight, "SplatX reaches every lane");
    runner.CheckNear(ascending.SplatY().x(), 2.0f, k_Tight, "SplatY");
    runner.CheckNear(ascending.SplatZ().x(), 3.0f, k_Tight, "SplatZ");
    runner.CheckNear(ascending.SplatW().x(), 4.0f, k_Tight, "SplatW");

    runner.Check(VectorNear(ascending.MergeXY(descending), Vector{ 1.0f, 4.0f, 2.0f, 3.0f }, k_Tight),
                 "MergeXY interleaves the x and y lanes");
    runner.Check(VectorNear(ascending.MergeZW(descending), Vector{ 3.0f, 2.0f, 4.0f, 1.0f }, k_Tight),
                 "MergeZW interleaves the z and w lanes");
    runner.Check(VectorNear(ascending.Swizzle<3, 2, 1, 0>(), descending, k_Tight),
                 "Swizzle reverses the lanes");
    // 0-3 index this vector, 4-7 the argument
    runner.Check(VectorNear(ascending.Permute<0, 4, 1, 5>(descending),
                            Vector{ 1.0f, 4.0f, 2.0f, 3.0f },
                            k_Tight),
                 "Permute indexes the second vector with 4-7");

    runner.CheckNear(ascending.AndInt(ascending).x(), 1.0f, k_Tight, "AndInt with self is identity");
    runner.CheckNear(ascending.XorInt(ascending).x(), 0.0f, k_Tight, "XorInt with self is zero");
    runner.CheckNear(ascending.OrInt(Vector::Zero()).x(), 1.0f, k_Tight, "OrInt with zero is identity");
}

void TestDotProducts(TestRunner& runner)
{
    runner.BeginSection("dot products");

    const Vector lhs{ 1.0f, 2.0f, 3.0f, 4.0f };
    const Vector rhs{ 5.0f, 6.0f, 7.0f, 8.0f };

    runner.CheckNear(lhs.Dot<2>(rhs), 17.0f, k_Tight, "Dot<2>");
    runner.CheckNear(lhs.Dot<3>(rhs), 38.0f, k_Tight, "Dot<3>");
    runner.CheckNear(lhs.Dot<4>(rhs), 70.0f, k_Tight, "Dot<4>");

    // narrow widths must ignore unused lanes, not assume they are zeroed - huge values there would
    // swamp the result if they leaked in
    const Vector dirty{ 1.0f, 2.0f, 1e30f, -1e30f };
    const Vector clean{ 5.0f, 6.0f, 1e30f, 1e30f };
    runner.CheckNear(dirty.Dot<2>(clean), 17.0f, k_Tight, "Dot<2> ignores lanes 2 and 3");
    runner.CheckNear(Vector{ 1.0f, 2.0f, 3.0f, 1e30f }.Dot<3>(Vector{ 5.0f, 6.0f, 7.0f, 1e30f }),
                     38.0f,
                     k_Tight,
                     "Dot<3> ignores lane 3");

    // if DotVec stopped splatting, everything built on it would still "work" but only in lane 0
    const Vector broadcast = lhs.DotVec<4>(rhs);
    runner.Check(VectorNear(broadcast, Vector::Replicate(70.0f), k_Tight),
                 "DotVec<4> splats to all four lanes");
    runner.Check(VectorNear(lhs.DotVec<3>(rhs), Vector::Replicate(38.0f), k_Tight),
                 "DotVec<3> splats to all four lanes");
}

void TestVectorGeometry(TestRunner& runner)
{
    runner.BeginSection("vector geometry");

    const Vector unitX{ 1.0f, 0.0f, 0.0f };
    const Vector unitY{ 0.0f, 1.0f, 0.0f };
    const Vector unitZ{ 0.0f, 0.0f, 1.0f };

    runner.Check(VectorNear(unitX.Cross(unitY), unitZ, k_Tight), "Cross(x, y) is z");
    runner.Check(VectorNear(unitY.Cross(unitZ), unitX, k_Tight), "Cross(y, z) is x");
    runner.Check(VectorNear(unitZ.Cross(unitX), unitY, k_Tight), "Cross(z, x) is y");
    runner.Check(VectorNear(unitY.Cross(unitX), -unitZ, k_Tight), "Cross anticommutes");

    const Vector threeFour{ 3.0f, 4.0f, 0.0f };
    runner.CheckNear(threeFour.Length<3>(), 5.0f, k_Tight, "Length<3> of a 3-4-5 triangle");
    runner.CheckNear(threeFour.LengthSq<3>(), 25.0f, k_Tight, "LengthSq<3>");
    runner.CheckNear(threeFour.Normalize<3>().Length<3>(), 1.0f, k_Tight, "Normalize<3> gives unit length");

    // reflecting off +Y flips only y
    const Vector reflected = Vector{ 1.0f, -1.0f, 0.0f }.Reflect<3>(unitY);
    runner.CheckNear(reflected.x(), 1.0f, k_Tight, "Reflect<3> preserves the tangential component");
    runner.CheckNear(reflected.y(), 1.0f, k_Tight, "Reflect<3> flips the normal component");

    const Vector wild{ -1.0f, 0.25f, 5.0f, 0.5f };
    runner.CheckNear(wild.Saturate().x(), 0.0f, k_Tight, "Saturate clamps below zero");
    runner.CheckNear(wild.Saturate().y(), 0.25f, k_Tight, "Saturate leaves in-range values alone");
    runner.CheckNear(wild.Saturate().z(), 1.0f, k_Tight, "Saturate clamps above one");
    const Vector clamped = wild.Clamp(Vector::Zero(), Vector::Replicate(2.0f));
    runner.CheckNear(clamped.z(), 2.0f, k_Tight, "Clamp respects the upper bound");
    runner.CheckNear(wild.Abs().x(), 1.0f, k_Tight, "Abs");
    runner.CheckNear(wild.Min(Vector::Zero()).z(), 0.0f, k_Tight, "Min");
    runner.CheckNear(wild.Max(Vector::Zero()).x(), 0.0f, k_Tight, "Max");
    runner.CheckNear(unitX.Lerp(unitY, 0.5f).x(), 0.5f, k_Tight, "Lerp midpoint");
}

void TestMatrixCore(TestRunner& runner)
{
    runner.BeginSection("matrix core");

    const Matrix identity = Matrix::Identity();
    const Matrix awkward = MakeAwkwardMatrix();

    runner.Check(MatrixNear(awkward.Transpose().Transpose(), awkward, k_Tight),
                 "Transpose is an involution");
    runner.Check(MatrixNear(identity * awkward, awkward, k_Tight), "Identity is a left identity");
    runner.Check(MatrixNear(awkward * identity, awkward, k_Tight), "Identity is a right identity");
    runner.Check(identity.IsIdentity(), "IsIdentity on the identity");
    runner.Check(!awkward.IsIdentity(), "IsIdentity rejects a general matrix");

    // one wrong lane index still produces something that looks like a matrix, so check both sides
    runner.Check(MatrixNear(awkward * awkward.Inverse(), identity, k_Tight),
                 "M times its inverse is the identity");
    runner.Check(MatrixNear(awkward.Inverse() * awkward, identity, k_Tight),
                 "the inverse times M is the identity");
    runner.Check(MatrixNear(identity.Inverse(), identity, k_Tight), "the identity is its own inverse");

    const Matrix rotation = Matrix::RotationAxis(Vector{ 0.3f, -0.5f, 0.8f }, 1.1f);
    runner.Check(MatrixNear(rotation.Inverse(), rotation.Transpose(), k_Loose),
                 "a rotation inverts to its transpose");
    runner.CheckNear(rotation.Determinant(), 1.0f, k_Loose, "a rotation has unit determinant");
    runner.CheckNear(Matrix::Scale(2.0f, 3.0f, 4.0f).Determinant(),
                     24.0f,
                     k_Tight,
                     "the determinant of a scale is the product of its factors");

    runner.CheckNear(awkward[2, 1], 1.0f, k_Tight, "operator[] reads the right element");
    Matrix mutated = awkward;
    mutated.SetElement(2, 1, 42.0f);
    runner.CheckNear(mutated[2, 1], 42.0f, k_Tight, "SetElement writes the right element");
    runner.CheckNear(awkward.GetColumn(2).x(), awkward[0, 2], k_Tight, "GetColumn reads down a column");
    runner.CheckNear(awkward.GetColumn(2).w(), awkward[3, 2], k_Tight, "GetColumn reaches the last row");
    Matrix columnWritten = Matrix::Zero();
    columnWritten.SetColumn(1, Vector{ 7.0f, 8.0f, 9.0f, 10.0f });
    runner.CheckNear(columnWritten[0, 1], 7.0f, k_Tight, "SetColumn writes the first row");
    runner.CheckNear(columnWritten[3, 1], 10.0f, k_Tight, "SetColumn writes the last row");
    runner.Check(VectorNear(awkward.GetRow(1), Vector{ -1.0f, 4.0f, 2.0f, 0.25f }, k_Tight), "GetRow");

    // the eye lands at the view-space origin regardless of handedness conventions
    const Vector eye{ 4.0f, 5.0f, 6.0f };
    const Matrix view = Matrix::LookAt(eye, Vector::Zero(), Vector{ 0.0f, 1.0f, 0.0f });
    runner.Check(VectorNear(Transform<3>(eye, view), Vector{ 0.0f, 0.0f, 0.0f, 1.0f }, k_Loose),
                 "LookAt maps the eye to the view-space origin");

    // zeroing w is what makes translation drop out
    const Matrix translated = Matrix::Translation(100.0f, 200.0f, 300.0f);
    runner.Check(VectorNear(TransformNormal(Vector{ 1.0f, 0.0f, 0.0f }, translated),
                            Vector{ 1.0f, 0.0f, 0.0f, 0.0f },
                            k_Tight),
                 "TransformNormal ignores translation");
    runner.Check(VectorNear(Transform<3>(Vector{ 1.0f, 0.0f, 0.0f }, translated),
                            Vector{ 101.0f, 200.0f, 300.0f, 1.0f },
                            k_Tight),
                 "Transform<3> applies translation");
}

void TestTRS(TestRunner& runner)
{
    runner.BeginSection("TRS");

    const Vector translation{ 5.0f, -3.0f, 2.0f };
    const Vector scale{ 2.0f, 0.5f, 1.5f };
    const Quaternion rotation = Quaternion::RotationAxis(Vector{ 1.0f, 2.0f, 3.0f }, 0.7f);

    // the analytical form has to agree with the general composition it replaces
    const Matrix composed = (Matrix::Scale(scale) * Matrix::RotationQuaternion(rotation)) *
                            Matrix::Translation(translation);
    const Matrix analytical = Matrix::TRS(translation, rotation, scale);
    runner.Check(MatrixNear(composed, analytical, k_Loose),
                 "TRS equals Scale times RotationQuaternion times Translation");

    runner.Check(MatrixNear(analytical * analytical.Inverse(), Matrix::Identity(), k_Loose),
                 "a TRS matrix inverts cleanly");

    // a rotation about a pivot leaves the pivot where it is
    const Vector pivot{ 3.0f, 4.0f, -1.0f };
    const Matrix aboutPivot =
        Matrix::TRS(Vector::Zero(), rotation, Vector{ 1.0f, 1.0f, 1.0f }, pivot);
    runner.Check(VectorNear(Transform<3>(pivot, aboutPivot), Vector{ 3.0f, 4.0f, -1.0f, 1.0f }, k_Loose),
                 "TRS with a rotation origin leaves that point fixed");

    runner.Check(MatrixNear(Matrix::TRS(translation, rotation, scale, Vector::Zero()), analytical, k_Tight),
                 "TRS with a zero pivot matches the three-argument form");

    // TRS promises scale-then-rotate, which differs from rotate-then-scale
    const Matrix scaleThenRotate = Matrix::Scale(scale) * Matrix::RotationQuaternion(rotation);
    const Matrix rotateThenScale = Matrix::RotationQuaternion(rotation) * Matrix::Scale(scale);
    runner.Check(!MatrixNear(scaleThenRotate, rotateThenScale, k_Loose),
                 "non-uniform scale does not commute with rotation (guards the S*R convention)");
    runner.Check(MatrixNear(Matrix::TRS(Vector::Zero(), rotation, scale), scaleThenRotate, k_Loose),
                 "TRS applies scale in the object's local frame");
}

void TestQuaternions(TestRunner& runner)
{
    runner.BeginSection("quaternions");

    const Quaternion identity = Quaternion::Identity();
    runner.Check(identity.IsIdentity(), "the default quaternion is the identity");
    runner.Check(Quaternion{}.IsIdentity(), "the default constructor gives the identity");
    runner.CheckNear(identity.w(), 1.0f, k_Tight, "the identity has w == 1");
    runner.CheckNear(identity.x(), 0.0f, k_Tight, "the identity has a zero vector part");

    const Vector axis{ 1.0f, 2.0f, 3.0f };
    const Quaternion rotation = Quaternion::RotationAxis(axis, 0.9f);
    runner.CheckNear(rotation.Length(), 1.0f, k_Tight, "RotationAxis produces a unit quaternion");
    runner.CheckNear(rotation.Normalize().Length(), 1.0f, k_Tight, "Normalize gives unit length");
    runner.CheckNear(rotation.LengthSq(), 1.0f, k_Tight, "LengthSq of a unit quaternion");
    runner.CheckNear(rotation.Dot(rotation), 1.0f, k_Tight, "Dot with self is the squared length");

    // conjugate and inverse coincide for a unit quaternion
    runner.Check(VectorNear(rotation.Conjugate(), rotation.Inverse(), k_Tight),
                 "conjugate and inverse agree for a unit quaternion");
    runner.Check(rotation.Multiply(rotation.Inverse()).IsIdentity() ||
                     VectorNear(rotation.Multiply(rotation.Inverse()), identity, k_Loose),
                 "a rotation composed with its inverse is the identity");

    // the case Conjugate cannot cover
    const Quaternion scaled{ rotation.x() * 3.0f, rotation.y() * 3.0f, rotation.z() * 3.0f,
                             rotation.w() * 3.0f };
    runner.Check(VectorNear(scaled.Multiply(scaled.Inverse()), identity, k_Loose),
                 "Inverse handles a non-unit quaternion");

    runner.Check(VectorNear(identity.Multiply(rotation), rotation, k_Tight),
                 "the identity is a left identity under Multiply");
    runner.Check(VectorNear(rotation.Multiply(identity), rotation, k_Tight),
                 "the identity is a right identity under Multiply");

    // Multiply applies *this first, so the row-vector matrix equivalent is thisMatrix * argMatrix.
    // This is the check that notices if that ever flips
    const Quaternion first = Quaternion::RotationAxis(Vector{ 1.0f, 0.0f, 0.0f }, 0.5f);
    const Quaternion second = Quaternion::RotationAxis(Vector{ 0.0f, 1.0f, 0.0f }, 0.8f);
    runner.Check(MatrixNear(first.Multiply(second).ToMatrix(),
                            first.ToMatrix() * second.ToMatrix(),
                            k_Loose),
                 "Multiply composes in the same order as the equivalent matrix product");
    runner.Check(!MatrixNear(first.Multiply(second).ToMatrix(),
                             second.ToMatrix() * first.ToMatrix(),
                             k_Loose),
                 "composition is not commutative (so the previous check is meaningful)");

    // must agree with going through the matrix
    const Vector point{ 0.3f, -0.7f, 1.4f };
    runner.Check(VectorNear(rotation.RotateVector(point),
                            TransformNormal(point, rotation.ToMatrix()),
                            k_Loose),
                 "RotateVector agrees with the matrix form");
    runner.CheckNear(rotation.RotateVector(point).Length<3>(),
                     point.Length<3>(),
                     k_Loose,
                     "rotation preserves length");
    // the axis is the one direction a rotation leaves alone
    const Vector normalizedAxis = axis.Normalize<3>();
    runner.Check(VectorNear(rotation.RotateVector(normalizedAxis), normalizedAxis, k_Loose),
                 "rotating the axis itself is a no-op");

    // Shepperd's method picks one of four branches, so several rotations are needed to reach them
    const Quaternion samples[]{ Quaternion::RotationAxis(Vector{ 1.0f, 0.0f, 0.0f }, 0.4f),
                                Quaternion::RotationAxis(Vector{ 0.0f, 1.0f, 0.0f }, 2.9f),
                                Quaternion::RotationAxis(Vector{ 0.0f, 0.0f, 1.0f }, -2.0f),
                                Quaternion::RotationAxis(Vector{ 1.0f, 1.0f, 1.0f }, 3.0f),
                                Quaternion::RotationAxis(Vector{ -2.0f, 0.5f, 1.0f }, 1.7f),
                                identity };
    for (const Quaternion& sample : samples)
    {
        const Quaternion roundTripped = Quaternion::FromMatrix(sample.ToMatrix());
        // q and -q are the same rotation, so compare matrices rather than components
        runner.Check(MatrixNear(roundTripped.ToMatrix(), sample.ToMatrix(), k_Loose),
                     "FromMatrix inverts ToMatrix across all Shepperd branches");
        runner.CheckNear(roundTripped.Length(), 1.0f, k_Loose, "FromMatrix returns a unit quaternion");
    }

    // axis-angle round trip
    Vector recoveredAxis{};
    float recoveredAngle = 0.0f;
    Quaternion::RotationAxis(axis, 1.2f).ToAxisAngle(recoveredAxis, recoveredAngle);
    runner.CheckNear(recoveredAngle, 1.2f, k_Loose, "ToAxisAngle recovers the angle");
    runner.Check(VectorNear(recoveredAxis.Normalize<3>(), axis.Normalize<3>(), k_Loose),
                 "ToAxisAngle recovers the axis direction");

    // Euler composition order
    runner.Check(MatrixNear(Quaternion::RotationRollPitchYaw(0.3f, 0.7f, -0.4f).ToMatrix(),
                            Matrix::RotationX(0.3f) * Matrix::RotationY(0.7f) * Matrix::RotationZ(-0.4f),
                            k_Loose),
                 "RotationRollPitchYaw composes X then Y then Z");
    runner.Check(MatrixNear(Matrix::RotationRollPitchYaw(0.3f, 0.7f, -0.4f),
                            Quaternion::RotationRollPitchYaw(0.3f, 0.7f, -0.4f).ToMatrix(),
                            k_Tight),
                 "Matrix and Quaternion RotationRollPitchYaw agree");

    // single-axis quaternions match the dedicated matrix builders
    runner.Check(MatrixNear(Quaternion::RotationAxis(Vector{ 1.0f, 0.0f, 0.0f }, 0.6f).ToMatrix(),
                            Matrix::RotationX(0.6f),
                            k_Loose),
                 "a quaternion about X matches Matrix::RotationX");
    runner.Check(MatrixNear(Quaternion::RotationAxis(Vector{ 0.0f, 1.0f, 0.0f }, 0.6f).ToMatrix(),
                            Matrix::RotationY(0.6f),
                            k_Loose),
                 "a quaternion about Y matches Matrix::RotationY");
    runner.Check(MatrixNear(Quaternion::RotationAxis(Vector{ 0.0f, 0.0f, 1.0f }, 0.6f).ToMatrix(),
                            Matrix::RotationZ(0.6f),
                            k_Loose),
                 "a quaternion about Z matches Matrix::RotationZ");
    // RotationNormal skips the normalize, so it should agree when handed an already-unit axis
    runner.Check(VectorNear(Quaternion::RotationNormal(normalizedAxis, 0.9f), rotation, k_Tight),
                 "RotationNormal matches RotationAxis for a unit axis");

    // two independent implementations of the same thing
    runner.Check(MatrixNear(Matrix::RotationAxis(axis, 1.1f),
                            Quaternion::RotationAxis(axis, 1.1f).ToMatrix(),
                            k_Loose),
                 "Matrix::RotationAxis agrees with the quaternion path");
}

void TestStorageConversions(TestRunner& runner)
{
    runner.BeginSection("storage conversions");

    const Float4 asFloat4{ 1.0f, 2.0f, 3.0f, 4.0f };
    const Float4 roundTripped = FromVector(ToVector(asFloat4));
    runner.CheckNear(roundTripped.x, 1.0f, k_Tight, "Float4 round trip x");
    runner.CheckNear(roundTripped.w, 4.0f, k_Tight, "Float4 round trip w");

    const Float3 asFloat3{ 1.0f, 2.0f, 3.0f };
    const Float3 roundTripped3 = FromVector(ToVector(asFloat3));
    runner.CheckNear(roundTripped3.z, 3.0f, k_Tight, "Float3 round trip z");

    const Float2 asFloat2{ 1.0f, 2.0f };
    const Float2 roundTripped2 = FromVector(ToVector(asFloat2));
    runner.CheckNear(roundTripped2.y, 2.0f, k_Tight, "Float2 round trip y");

    const Matrix awkward = MakeAwkwardMatrix();
    runner.Check(MatrixNear(ToMatrix(FromMatrix<Float4x4>(awkward)), awkward, k_Tight),
                 "Float4x4 storage round trip");

    const Float3x3 upperLeft = FromMatrix<Float3x3>(awkward);
    runner.CheckNear(upperLeft[0, 0], awkward[0, 0], k_Tight, "Float3x3 keeps the upper-left corner");
    runner.CheckNear(upperLeft[2, 2], awkward[2, 2], k_Tight, "Float3x3 keeps the lower-right corner");

    const Float4x3 affine = FromMatrix<Float4x3>(awkward);
    runner.CheckNear(affine[3, 0], awkward[3, 0], k_Tight, "Float4x3 keeps the translation row");

    // a Quaternion has to survive storage through the Vector path
    const Quaternion rotation = Quaternion::RotationAxis(Vector{ 1.0f, 2.0f, 3.0f }, 0.5f);
    const Float4 storedRotation = FromVector(static_cast<Vector>(rotation));
    const Quaternion restored{ ToVector(storedRotation) };
    runner.Check(VectorNear(restored, rotation, k_Tight), "Quaternion survives a Float4 round trip");

    // hashes need only be usable and self-consistent
    std::unordered_map<Float3, int> lookup;
    lookup[asFloat3] = 7;
    runner.Check(lookup.at(asFloat3) == 7, "Float3 works as an unordered_map key");
    runner.Check(std::hash<Float3>{}(asFloat3) == std::hash<Float3>{}(asFloat3),
                 "Float3 hashing is deterministic");
    runner.Check(std::hash<Float3>{}(asFloat3) != std::hash<Float3>{}(Float3{ 3.0f, 2.0f, 1.0f }),
                 "Float3 hashing distinguishes a permutation");
}

// Sweeps a domain for the worst error rather than checking a few points. The only thing that catches a
// mistyped coefficient: a wrong constant in the fifth decimal still gives a curve of the right shape
// and magnitude everywhere, so spot checks and identities both sail past it.
struct SweepResult
{
    double worstError;
    float worstInput;
};

template<typename ApproximateFn, typename ReferenceFn>
SweepResult SweepAbsolute(float first,
                          float last,
                          int steps,
                          ApproximateFn&& approximate,
                          ReferenceFn&& reference)
{
    SweepResult result{ 0.0, first };
    for (int step = 0; step <= steps; ++step)
    {
        const float input = first + (last - first) * (static_cast<float>(step) / static_cast<float>(steps));
        const double error = std::fabs(static_cast<double>(approximate(input)) - reference(input));
        if (error > result.worstError)
        {
            result.worstError = error;
            result.worstInput = input;
        }
    }
    return result;
}

template<typename ApproximateFn, typename ReferenceFn>
SweepResult SweepRelative(float first,
                          float last,
                          int steps,
                          ApproximateFn&& approximate,
                          ReferenceFn&& reference)
{
    SweepResult result{ 0.0, first };
    for (int step = 0; step <= steps; ++step)
    {
        const float input = first + (last - first) * (static_cast<float>(step) / static_cast<float>(steps));
        const double expected = reference(input);
        const double error =
            std::fabs((static_cast<double>(approximate(input)) - expected) / (expected == 0.0 ? 1.0 : expected));
        if (error > result.worstError)
        {
            result.worstError = error;
            result.worstInput = input;
        }
    }
    return result;
}

// The measured number distinguishes "the polynomial is wrong" from "the bound was optimistic"
void CheckSweep(TestRunner& runner, const SweepResult& result, double bound, std::string_view description)
{
    const bool withinBound = result.worstError < bound;
    runner.Check(withinBound, description);
    if (!withinBound)
    {
        std::println("          worst error {:.3e} at input {}, bound {:.3e}",
                     result.worstError,
                     result.worstInput,
                     bound);
    }
}

// Lets one sweep drive both the vector and scalar forms
float VectorSin(float radians)
{
    return Vector::Replicate(radians).Sin().x();
}
float VectorCos(float radians)
{
    return Vector::Replicate(radians).Cos().x();
}
float VectorSinEst(float radians)
{
    return Vector::Replicate(radians).SinEst().x();
}
float VectorCosEst(float radians)
{
    return Vector::Replicate(radians).CosEst().x();
}
float VectorExp2(float value)
{
    return Vector::Replicate(value).Exp2().x();
}
float VectorLog2(float value)
{
    return Vector::Replicate(value).Log2().x();
}
float VectorExp2Est(float value)
{
    return Vector::Replicate(value).Exp2Est().x();
}
float VectorLog2Est(float value)
{
    return Vector::Replicate(value).Log2Est().x();
}

void TestTranscendentalAccuracy(TestRunner& runner)
{
    runner.BeginSection("transcendental accuracy sweeps");

    const auto referenceSin = [](float x) { return std::sin(static_cast<double>(x)); };
    const auto referenceCos = [](float x) { return std::cos(static_cast<double>(x)); };
    const auto referenceExp2 = [](float x) { return std::exp2(static_cast<double>(x)); };
    const auto referenceLog2 = [](float x) { return std::log2(static_cast<double>(x)); };

    // one period, then far outside it so range reduction is exercised
    const SweepResult sinNear = SweepAbsolute(-k_Pi, k_Pi, 20000, VectorSin, referenceSin);
    CheckSweep(runner, sinNear, 3e-7, "Vector::Sin within 3e-7 over one period");
    const SweepResult cosNear = SweepAbsolute(-k_Pi, k_Pi, 20000, VectorCos, referenceCos);
    CheckSweep(runner, cosNear, 3e-7, "Vector::Cos within 3e-7 over one period");

    const SweepResult sinFar = SweepAbsolute(-40.0f, 40.0f, 20000, VectorSin, referenceSin);
    CheckSweep(runner, sinFar, 5e-6, "Vector::Sin survives range reduction out to +-40 radians");
    const SweepResult cosFar = SweepAbsolute(-40.0f, 40.0f, 20000, VectorCos, referenceCos);
    CheckSweep(runner, cosFar, 5e-6, "Vector::Cos survives range reduction out to +-40 radians");

    const SweepResult sinEst = SweepAbsolute(-k_Pi, k_Pi, 20000, VectorSinEst, referenceSin);
    CheckSweep(runner, sinEst, 2e-5, "Vector::SinEst within 2e-5");
    const SweepResult cosEst = SweepAbsolute(-k_Pi, k_Pi, 20000, VectorCosEst, referenceCos);
    CheckSweep(runner, cosEst, 2e-5, "Vector::CosEst within 2e-5");

    // Exp2 over the range that does not overflow float, relative because the magnitude spans 2^120.
    // The bound is set by DirectXMath: XMVectorExp2 measures ~1.0e-5, while the WASM six-term series
    // measures ~1.9e-7. Backend-fastest means the weaker one sets what may be documented.
    const SweepResult exp2Sweep = SweepRelative(-60.0f, 60.0f, 20000, VectorExp2, referenceExp2);
    CheckSweep(runner, exp2Sweep, 2e-5, "Vector::Exp2 relative error within 2e-5");
    const SweepResult exp2EstSweep = SweepRelative(-60.0f, 60.0f, 20000, VectorExp2Est, referenceExp2);
    CheckSweep(runner, exp2EstSweep, 1e-4, "Vector::Exp2Est relative error within 1e-4");
    // 2^n is exactly representable, so the polynomial should return 1 for integer input
    runner.CheckNear(VectorExp2(0.0f), 1.0f, 1e-6f, "Exp2(0) is 1");
    runner.CheckNear(VectorExp2(10.0f), 1024.0f, 1e-6f, "Exp2(10) is 1024");
    runner.CheckNear(VectorExp2(-3.0f), 0.125f, 1e-6f, "Exp2(-3) is 0.125");

    const SweepResult log2Sweep = SweepAbsolute(1e-4f, 1e4f, 20000, VectorLog2, referenceLog2);
    CheckSweep(runner, log2Sweep, 2e-6, "Vector::Log2 absolute error within 2e-6");
    const SweepResult log2EstSweep = SweepAbsolute(1e-4f, 1e4f, 20000, VectorLog2Est, referenceLog2);
    CheckSweep(runner, log2EstSweep, 1e-5, "Vector::Log2Est absolute error within 1e-5");
    runner.CheckNear(VectorLog2(1.0f), 0.0f, 1e-6f, "Log2(1) is 0");
    runner.CheckNear(VectorLog2(1024.0f), 10.0f, 1e-6f, "Log2(1024) is 10");
    runner.CheckNear(VectorLog2(0.25f), -2.0f, 1e-6f, "Log2(0.25) is -2");
    // either side of the sqrt2 fold in the mantissa reduction
    runner.CheckNear(VectorLog2(1.4f), std::log2(1.4f), 1e-5f, "Log2 just below the sqrt2 fold");
    runner.CheckNear(VectorLog2(1.45f), std::log2(1.45f), 1e-5f, "Log2 just above the sqrt2 fold");

    // sharing coefficient tables, these should track the vector forms closely
    const SweepResult scalarSin = SweepAbsolute(-k_Pi, k_Pi, 20000, Sin, referenceSin);
    CheckSweep(runner, scalarSin, 3e-7, "scalar Sin within 3e-7");
    const SweepResult scalarCos = SweepAbsolute(-k_Pi, k_Pi, 20000, Cos, referenceCos);
    CheckSweep(runner, scalarCos, 3e-7, "scalar Cos within 3e-7");
    const SweepResult scalarExp2 = SweepRelative(-60.0f, 60.0f, 20000, Exp2, referenceExp2);
    CheckSweep(runner, scalarExp2, 1e-6, "scalar Exp2 relative error within 1e-6");
    const SweepResult scalarLog2 = SweepAbsolute(1e-4f, 1e4f, 20000, Log2, referenceLog2);
    CheckSweep(runner, scalarLog2, 2e-6, "scalar Log2 absolute error within 2e-6");
}

void TestTranscendentalIdentities(TestRunner& runner)
{
    runner.BeginSection("transcendental identities");

    // these hold regardless of what libm thinks
    for (float angle : { -7.3f, -1.2f, -0.4f, 0.0f, 0.4f, 1.2f, 3.9f, 7.3f, 100.0f })
    {
        const Vector broadcast = Vector::Replicate(angle);
        const auto [sine, cosine] = broadcast.SinCos();

        // disagreement here means the shared reduction has diverged
        runner.CheckNear(sine.x(), broadcast.Sin().x(), 1e-6f, "SinCos sine matches Sin");
        runner.CheckNear(cosine.x(), broadcast.Cos().x(), 1e-6f, "SinCos cosine matches Cos");

        // catches a wrong quadrant sign that a magnitude check would miss
        const float pythagorean = sine.x() * sine.x() + cosine.x() * cosine.x();
        runner.CheckNear(pythagorean, 1.0f, 1e-5f, "sin^2 + cos^2 is 1");

        const ScalarSinCos scalarPair = SinCos(angle);
        runner.CheckNear(scalarPair.sin, sine.x(), 1e-5f, "scalar SinCos tracks the vector form");
        runner.CheckNear(scalarPair.cos, cosine.x(), 1e-5f, "scalar SinCos cosine tracks the vector form");

        const ScalarSinCos estimatePair = SinCosEst(angle);
        runner.CheckNear(estimatePair.sin, sine.x(), 2e-4f, "scalar SinCosEst is within its bound");
    }

    // must invert each other
    for (float value : { 0.01f, 0.5f, 1.0f, 3.7f, 17.0f, 1000.0f })
    {
        const Vector broadcast = Vector::Replicate(value);
        runner.CheckNear(broadcast.Log2().Exp2().x(), value, 1e-5f, "Exp2(Log2(x)) is x");
    }
    for (float value : { -12.0f, -1.5f, 0.0f, 2.25f, 30.0f })
    {
        const Vector broadcast = Vector::Replicate(value);
        runner.CheckNear(broadcast.Exp2().Log2().x(), value, 1e-5f, "Log2(Exp2(x)) is x");
    }

    // odd/even symmetry
    runner.CheckNear(VectorSin(-1.3f), -VectorSin(1.3f), 1e-6f, "sine is odd");
    runner.CheckNear(VectorCos(-1.3f), VectorCos(1.3f), 1e-6f, "cosine is even");

    // quadrant boundaries, where the reflection takes over
    runner.CheckNear(VectorSin(k_Pi * 0.5f), 1.0f, 1e-5f, "sin(pi/2) is 1");
    runner.CheckNear(VectorCos(k_Pi * 0.5f), 0.0f, 1e-5f, "cos(pi/2) is 0");
    runner.CheckNear(VectorSin(k_Pi), 0.0f, 1e-5f, "sin(pi) is 0");
    runner.CheckNear(VectorCos(k_Pi), -1.0f, 1e-5f, "cos(pi) is -1");
    runner.CheckNear(VectorCos(k_Pi * 1.5f), 0.0f, 1e-5f, "cos(3pi/2) is 0");
    runner.CheckNear(VectorSin(k_Pi * 1.5f), -1.0f, 1e-5f, "sin(3pi/2) is -1");

    // four genuinely independent lanes, not lane 0 splattered everywhere
    const Vector distinct{ 0.0f, k_Pi * 0.5f, k_Pi, k_Pi * 1.5f };
    const auto [lanes, cosLanes] = distinct.SinCos();
    runner.CheckNear(lanes.x(), 0.0f, 1e-5f, "lane 0 sine");
    runner.CheckNear(lanes.y(), 1.0f, 1e-5f, "lane 1 sine");
    runner.CheckNear(lanes.z(), 0.0f, 1e-5f, "lane 2 sine");
    runner.CheckNear(lanes.w(), -1.0f, 1e-5f, "lane 3 sine");
    runner.CheckNear(cosLanes.x(), 1.0f, 1e-5f, "lane 0 cosine");
    runner.CheckNear(cosLanes.z(), -1.0f, 1e-5f, "lane 2 cosine");

    // conversion helpers
    runner.CheckNear(DegreesToRadians(180.0f), k_Pi, 1e-6f, "DegreesToRadians");
    runner.CheckNear(RadiansToDegrees(k_Pi), 180.0f, 1e-6f, "RadiansToDegrees");
    runner.CheckNear(Lerp(2.0f, 4.0f, 0.25f), 2.5f, 1e-6f, "scalar Lerp");
    runner.CheckNear(Clamp(5.0f, 0.0f, 1.0f), 1.0f, 1e-6f, "scalar Clamp");
    runner.CheckNear(Saturate(-3.0f), 0.0f, 1e-6f, "scalar Saturate");
    runner.CheckNear(ModAngles(2.0f * k_Pi + 0.5f), 0.5f, 1e-5f, "scalar ModAngles");

    // RotationX/Y/Z go through SinCos; confirm that did not shift them
    runner.Check(MatrixNear(Matrix::RotationX(0.7f).Inverse(), Matrix::RotationX(-0.7f), k_Loose),
                 "RotationX(-a) inverts RotationX(a) after the SinCos rewrite");
    runner.CheckNear(Matrix::RotationY(0.7f).Determinant(), 1.0f, k_Loose, "RotationY stays orthonormal");
    runner.Check(MatrixNear(Matrix::RotationZ(0.0f), Matrix::Identity(), k_Tight),
                 "RotationZ(0) is the identity");
}

} // namespace

int main()
{
    TestRunner runner{ "MathTests" };

    TestMasksAndSelect(runner);
    TestRoundingAndLanes(runner);
    TestDotProducts(runner);
    TestVectorGeometry(runner);
    TestMatrixCore(runner);
    TestTRS(runner);
    TestQuaternions(runner);
    TestTranscendentalAccuracy(runner);
    TestTranscendentalIdentities(runner);
    TestStorageConversions(runner);

    return runner.Report();
}
