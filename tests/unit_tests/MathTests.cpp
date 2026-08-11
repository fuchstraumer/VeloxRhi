#include "TestHarness.hpp"
#include "Math.hpp"
#include "math/MathHashes.hpp"
#include <cmath>
#include <unordered_map>

// Checks for velox::math. The bias throughout is toward identities and known-answer cases rather
// than golden values, because the failures worth catching here are the ones that look plausible: a
// transposed shuffle index, a sign dropped from a cofactor, a coefficient off in the third decimal.
// A wrong dot product still returns a number of about the right size.
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

// Deliberately not a similarity transform: mixed signs, a non-trivial last column, and no shared
// structure between rows, so a wrong cofactor lane cannot cancel out by luck
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

    // operand order: clear lanes take the second argument, set lanes the third. The two backends
    // spell this in opposite orders underneath, so it is worth pinning down explicitly
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

    // both backends round halves to even, so 0.5 goes down and 1.5 goes up
    const Vector halves{ 0.5f, 1.5f, 2.5f, -0.5f };
    const Vector rounded = halves.Round();
    runner.CheckNear(rounded.x(), 0.0f, k_Tight, "Round(0.5) ties to even");
    runner.CheckNear(rounded.y(), 2.0f, k_Tight, "Round(1.5) ties to even");
    runner.CheckNear(rounded.z(), 2.0f, k_Tight, "Round(2.5) ties to even");

    const Vector mixed{ 1.7f, -1.7f, 1.2f, -1.2f };
    runner.CheckNear(mixed.Truncate().y(), -1.0f, k_Tight, "Truncate goes toward zero");
    runner.CheckNear(mixed.Floor().y(), -2.0f, k_Tight, "Floor goes toward negative infinity");
    runner.CheckNear(mixed.Ceil().y(), -1.0f, k_Tight, "Ceil goes toward positive infinity");

    // Mod truncates, so the remainder keeps the dividend's sign
    const Vector modded = Vector{ 7.0f, -7.0f, 5.5f, 1.0f }.Mod(Vector::Replicate(3.0f));
    runner.CheckNear(modded.x(), 1.0f, k_Tight, "Mod of a positive dividend");
    runner.CheckNear(modded.y(), -1.0f, k_Tight, "Mod keeps the dividend's sign");
    runner.CheckNear(modded.z(), 2.5f, k_Tight, "Mod of a fractional dividend");

    // ModAngles must wrap into [-pi, pi], including the negative half - which is exactly what a
    // truncating implementation would get wrong
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

    // the narrow widths must ignore the unused lanes rather than assuming they are zeroed. Huge
    // values there would swamp the result if they leaked in
    const Vector dirty{ 1.0f, 2.0f, 1e30f, -1e30f };
    const Vector clean{ 5.0f, 6.0f, 1e30f, 1e30f };
    runner.CheckNear(dirty.Dot<2>(clean), 17.0f, k_Tight, "Dot<2> ignores lanes 2 and 3");
    runner.CheckNear(Vector{ 1.0f, 2.0f, 3.0f, 1e30f }.Dot<3>(Vector{ 5.0f, 6.0f, 7.0f, 1e30f }),
                     38.0f,
                     k_Tight,
                     "Dot<3> ignores lane 3");

    // DotVec exists so callers get the result already broadcast; if it stopped splatting, everything
    // built on it would still "work" but silently only in lane 0
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

    // reflecting off the +Y plane should flip only y
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

    // the inverse is a long shuffle sequence where a single wrong lane index produces a matrix that
    // still looks like a matrix, so the round trip is checked from both sides
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

    // the eye must land at the view-space origin, whatever the handedness conventions do elsewhere
    const Vector eye{ 4.0f, 5.0f, 6.0f };
    const Matrix view = Matrix::LookAt(eye, Vector::Zero(), Vector{ 0.0f, 1.0f, 0.0f });
    runner.Check(VectorNear(Transform<3>(eye, view), Vector{ 0.0f, 0.0f, 0.0f, 1.0f }, k_Loose),
                 "LookAt maps the eye to the view-space origin");

    // TransformNormal must ignore translation entirely - that is what the zeroed w buys
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

    // a pure rotation about a pivot must leave that pivot where it is
    const Vector pivot{ 3.0f, 4.0f, -1.0f };
    const Matrix aboutPivot =
        Matrix::TRS(Vector::Zero(), rotation, Vector{ 1.0f, 1.0f, 1.0f }, pivot);
    runner.Check(VectorNear(Transform<3>(pivot, aboutPivot), Vector{ 3.0f, 4.0f, -1.0f, 1.0f }, k_Loose),
                 "TRS with a rotation origin leaves that point fixed");

    runner.Check(MatrixNear(Matrix::TRS(translation, rotation, scale, Vector::Zero()), analytical, k_Tight),
                 "TRS with a zero pivot matches the three-argument form");

    // local-frame scale: scaling then rotating is not the same as rotating then scaling, and TRS
    // promises the former
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

    // for a unit quaternion the conjugate and the inverse coincide
    runner.Check(VectorNear(rotation.Conjugate(), rotation.Inverse(), k_Tight),
                 "conjugate and inverse agree for a unit quaternion");
    runner.Check(rotation.Multiply(rotation.Inverse()).IsIdentity() ||
                     VectorNear(rotation.Multiply(rotation.Inverse()), identity, k_Loose),
                 "a rotation composed with its inverse is the identity");

    // Inverse must also handle a non-unit quaternion, which is the case Conjugate cannot cover
    const Quaternion scaled{ rotation.x() * 3.0f, rotation.y() * 3.0f, rotation.z() * 3.0f,
                             rotation.w() * 3.0f };
    runner.Check(VectorNear(scaled.Multiply(scaled.Inverse()), identity, k_Loose),
                 "Inverse handles a non-unit quaternion");

    runner.Check(VectorNear(identity.Multiply(rotation), rotation, k_Tight),
                 "the identity is a left identity under Multiply");
    runner.Check(VectorNear(rotation.Multiply(identity), rotation, k_Tight),
                 "the identity is a right identity under Multiply");

    // ---- the argument-order trap ----
    // Multiply applies *this first, then the argument. The matrix equivalent under this library's
    // row-vector convention is therefore thisMatrix * argumentMatrix, in that order. If the
    // convention were ever flipped, this is the check that would notice
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

    // rotating a vector must agree with going through the matrix
    const Vector point{ 0.3f, -0.7f, 1.4f };
    runner.Check(VectorNear(rotation.RotateVector(point),
                            TransformNormal(point, rotation.ToMatrix()),
                            k_Loose),
                 "RotateVector agrees with the matrix form");
    runner.CheckNear(rotation.RotateVector(point).Length<3>(),
                     point.Length<3>(),
                     k_Loose,
                     "rotation preserves length");
    // the rotation axis is the one direction a rotation leaves alone
    const Vector normalizedAxis = axis.Normalize<3>();
    runner.Check(VectorNear(rotation.RotateVector(normalizedAxis), normalizedAxis, k_Loose),
                 "rotating the axis itself is a no-op");

    // quaternion to matrix and back. Shepperd's method picks one of four branches, so several
    // different rotations are needed to exercise more than one of them
    const Quaternion samples[]{ Quaternion::RotationAxis(Vector{ 1.0f, 0.0f, 0.0f }, 0.4f),
                                Quaternion::RotationAxis(Vector{ 0.0f, 1.0f, 0.0f }, 2.9f),
                                Quaternion::RotationAxis(Vector{ 0.0f, 0.0f, 1.0f }, -2.0f),
                                Quaternion::RotationAxis(Vector{ 1.0f, 1.0f, 1.0f }, 3.0f),
                                Quaternion::RotationAxis(Vector{ -2.0f, 0.5f, 1.0f }, 1.7f),
                                identity };
    for (const Quaternion& sample : samples)
    {
        const Quaternion roundTripped = Quaternion::FromMatrix(sample.ToMatrix());
        // q and -q are the same rotation, so compare the matrices rather than the components
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

    // Euler composition order, stated once in the doc comment and pinned here
    runner.Check(MatrixNear(Quaternion::RotationRollPitchYaw(0.3f, 0.7f, -0.4f).ToMatrix(),
                            Matrix::RotationX(0.3f) * Matrix::RotationY(0.7f) * Matrix::RotationZ(-0.4f),
                            k_Loose),
                 "RotationRollPitchYaw composes X then Y then Z");
    runner.Check(MatrixNear(Matrix::RotationRollPitchYaw(0.3f, 0.7f, -0.4f),
                            Quaternion::RotationRollPitchYaw(0.3f, 0.7f, -0.4f).ToMatrix(),
                            k_Tight),
                 "Matrix and Quaternion RotationRollPitchYaw agree");

    // single-axis quaternions must match the dedicated matrix builders
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

    // Matrix::RotationAxis and the quaternion path are independent implementations of the same thing
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

    // a Quaternion has to survive being stored through the Vector path
    const Quaternion rotation = Quaternion::RotationAxis(Vector{ 1.0f, 2.0f, 3.0f }, 0.5f);
    const Float4 storedRotation = FromVector(static_cast<Vector>(rotation));
    const Quaternion restored{ ToVector(storedRotation) };
    runner.Check(VectorNear(restored, rotation, k_Tight), "Quaternion survives a Float4 round trip");

    // hashes only need to be usable and self-consistent
    std::unordered_map<Float3, int> lookup;
    lookup[asFloat3] = 7;
    runner.Check(lookup.at(asFloat3) == 7, "Float3 works as an unordered_map key");
    runner.Check(std::hash<Float3>{}(asFloat3) == std::hash<Float3>{}(asFloat3),
                 "Float3 hashing is deterministic");
    runner.Check(std::hash<Float3>{}(asFloat3) != std::hash<Float3>{}(Float3{ 3.0f, 2.0f, 1.0f }),
                 "Float3 hashing distinguishes a permutation");
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
    TestStorageConversions(runner);

    return runner.Report();
}
