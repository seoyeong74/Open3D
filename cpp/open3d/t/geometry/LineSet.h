// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018-2021 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

#include <string>

#include "open3d/core/Tensor.h"
#include "open3d/geometry/LineSet.h"
#include "open3d/t/geometry/Geometry.h"
#include "open3d/t/geometry/TensorMap.h"
#include "open3d/utility/Logging.h"

namespace open3d {
namespace t {
namespace geometry {

/// \class LineSet
/// \brief A LineSet contains points and lines joining them and optionally
/// attributes on the points and lines.
///
/// The LineSet class stores the attribute data in key-value pairs for
/// flexibility, where the key is a string representing the attribute name and
/// value is a Tensor containing the attribute data.
///
/// By default, there are two sets of dictionaries, i.e. `point_attr_` and
/// `line_attr_`. In most cases, the length of an attribute should be
/// equal to the length of the data corresponding to the master key. For
/// instance `point_attr_["colors"]` should have the same length as
/// `point_attr_["positions"]`.
///
/// Although the attributes are all stored in a key-value pair dictionary, the
/// attributes have different levels:
///
/// - Level 0: Default attribute {"positions", "indices"}.
///     - Created by default, required for all LineSets.
///     - The "positions" tensor must be of shape N x {3,} while the
///     "indices" tensor must be of shape N x {2,} and DtypeCode Int.
///     - Convenience functions:
///         - LineSet::GetPointPositions()
///         - LineSet::SetPointPositions(points_tensor)
///         - LineSet::HasPointPositions()
///         - LineSet::GetLineIndices()
///         - LineSet::SetLineIndices(lines_tensor)
///         - LineSet::HasLineIndices()
///     - The device of "positions" and "indices" must be consistent and they
///       determine the device of the LineSet.
/// - Level 1: Commonly used attributes: line colors.
///     - Not created by default.
///     - The tensor must be of shape N x 3.
///     - Convenience functions:
///         - Line colors (stored at line_attr_["colors"])
///             - LineSet::GetLineColors()
///             - LineSet::SetLineColors(line_colors_tensor)
///             - LineSet::HasLineColors()
///     - For all attributes, the device must be consistent with the device of
///       the LineSet. Dtype can be different.
/// - Level 2: Custom attributes, e.g. {"labels"}.
///     - Not created by default. Created by users.
///     - No convenience functions.
///     - Use generalized helper functions. Examples:
///         - LineSet::GetPointAttr("labels")
///         - LineSet::SetPointAttr("labels",
///                                       point_labels_tensor)
///         - LineSet::HasPointAttr("labels")
///         - LineSet::GetLineAttr("labels")
///         - LineSet::SetLineAttr("labels",
///                                         line_labels_tensor)
///         - LineSet::HasLineAttr("labels")
///     - For all attributes, the device must be consistent with the device of
///       the LineSet. Dtype can be different.
///
/// Note that the level 0 and level 1 convenience functions can also be achieved
/// via the generalized helper functions.

class LineSet : public Geometry {
public:
    /// Construct an empty LineSet on the provided device.
    LineSet(const core::Device &device = core::Device("CPU:0"));

    /// Construct a LineSet from points and lines.
    ///
    /// The input tensors will be directly used as the underlying storage of
    /// the line set (no memory copy). If the tensor is created in-place
    /// from a pre-allocated buffer, the tensor has a fixed size and thus
    /// the resulting LineSet will have a fixed size and calling to
    /// functions like `SynchronizedPushBack` will raise an exception.
    ///
    /// The resulting LineSet will have the same dtype and device as the
    /// tensor. The device for \p points must be consistent with
    /// \p lines.
    ///
    /// \param points A tensor with element shape (3,).
    /// \param lines A tensor with element shape (2,) and Int Dtype.
    LineSet(const core::Tensor &points, const core::Tensor &lines);

    virtual ~LineSet() override {}

    /// Transfer the line set to a specified device.
    /// \param device The targeted device to convert to.
    /// \param copy If true, a new line set is always created; if false,
    /// the copy is avoided when the original line set is already on the
    /// targeted device.
    LineSet To(const core::Device &device, bool copy = false) const;

    /// Returns copy of the line set on the same device.
    LineSet Clone() const { return To(GetDevice(), /*copy=*/true); }

    /// \brief Text description.
    std::string ToString() const;

    /// Getter for point_attr_ TensorMap. Used in Pybind.
    const TensorMap &GetPointAttr() const { return point_attr_; }

    /// Get point attributes in point_attr_. Throws exception if the attribute
    /// does not exist.
    ///
    /// \param key Attribute name.
    core::Tensor &GetPointAttr(const std::string &key) {
        return point_attr_.at(key);
    }

    /// Get the value of the "positions" attribute in point_attr_.
    /// Convenience function.
    core::Tensor &GetPointPositions() { return GetPointAttr("positions"); }

    /// Getter for line_attr_ TensorMap. Used in Pybind.
    const TensorMap &GetLineAttr() const { return line_attr_; }

    /// Get line attributes in line_attr_. Throws exception if the
    /// attribute does not exist.
    ///
    /// \param key Attribute name.
    core::Tensor &GetLineAttr(const std::string &key) {
        return line_attr_.at(key);
    }

    /// Get the value of the "indices" attribute in line_attr_.
    /// Convenience function.
    core::Tensor &GetLineIndices() { return GetLineAttr("indices"); }

    /// Get the value of the "colors" attribute in line_attr_.
    /// Convenience function.
    core::Tensor &GetLineColors() { return GetLineAttr("colors"); }

    /// Get point attributes. Throws exception if the attribute does not exist.
    ///
    /// \param key Attribute name.
    const core::Tensor &GetPointAttr(const std::string &key) const {
        return point_attr_.at(key);
    }

    /// Removes point attribute by key value. Primary attribute "positions"
    /// cannot be removed. Throws warning if attribute key does not exists.
    ///
    /// \param key Attribute name.
    void RemovePointAttr(const std::string &key) { point_attr_.Erase(key); }

    /// Get the value of the "positions" attribute in point_attr_.
    /// Convenience function.
    const core::Tensor &GetPointPositions() const {
        return GetPointAttr("positions");
    }

    /// Get line attributes in line_attr_. Throws exception if the
    /// attribute does not exist.
    ///
    /// \param key Attribute name.
    const core::Tensor &GetLineAttr(const std::string &key) const {
        return line_attr_.at(key);
    }

    /// Removes line attribute by key value. Primary attribute "indices"
    /// cannot be removed. Throws warning if attribute key does not exists.
    ///
    /// \param key Attribute name.
    void RemoveLineAttr(const std::string &key) { line_attr_.Erase(key); }

    /// Get the value of the "indices" attribute in line_attr_.
    /// Convenience function.
    const core::Tensor &GetLineIndices() const {
        return GetLineAttr("indices");
    }

    /// Get the value of the "colors" attribute in line_attr_.
    /// Convenience function.
    const core::Tensor &GetLineColors() const { return GetLineAttr("colors"); }

    /// Set point attributes. If the attribute key already exists, its value
    /// will be overwritten, otherwise, the new key will be created.
    ///
    /// \param key Attribute name.
    /// \param value A tensor.
    void SetPointAttr(const std::string &key, const core::Tensor &value) {
        value.AssertDevice(device_);
        point_attr_[key] = value;
    }

    /// Set the value of the "positions" attribute in point_attr_.
    /// Convenience function.
    void SetPointPositions(const core::Tensor &value) {
        value.AssertShapeCompatible({utility::nullopt, 3});
        SetPointAttr("positions", value);
    }

    /// Set line attributes. If the attribute key already exists, its value
    /// will be overwritten, otherwise, the new key will be created.
    ///
    /// \param key Attribute name.
    /// \param value A tensor.
    void SetLineAttr(const std::string &key, const core::Tensor &value) {
        value.AssertDevice(device_);
        line_attr_[key] = value;
    }

    /// Set the value of the "indices" attribute in line_attr_.
    void SetLineIndices(const core::Tensor &value) {
        value.AssertShapeCompatible({utility::nullopt, 2});
        SetLineAttr("indices", value);
    }

    /// Set the value of the "colors" attribute in line_attr_.
    /// This is a convenience function.
    void SetLineColors(const core::Tensor &value) {
        value.AssertShapeCompatible({utility::nullopt, 3});
        SetLineAttr("colors", value);
    }

    /// Returns true if all of the followings are true in point_attr_:
    /// 1) attribute key exist
    /// 2) attribute's length as "points"'s length
    /// 3) attribute's length > 0
    bool HasPointAttr(const std::string &key) const {
        return point_attr_.Contains(key) && GetPointAttr(key).GetLength() > 0 &&
               GetPointAttr(key).GetLength() == GetPointPositions().GetLength();
    }

    /// Check if the "positions" attribute's value in point_attr_ has length >
    /// 0. Convenience function.
    bool HasPointPositions() const { return HasPointAttr("positions"); }

    /// Returns true if all of the followings are true in line_attr_:
    /// 1) attribute key exist
    /// 2) attribute's length as "indices"'s length
    /// 3) attribute's length > 0
    bool HasLineAttr(const std::string &key) const {
        return line_attr_.Contains(key) && GetLineAttr(key).GetLength() > 0 &&
               GetLineAttr(key).GetLength() == GetLineIndices().GetLength();
    }

    /// Check if the "indices" attribute's value in line_attr_ has length
    /// > 0.
    /// Convenience function.
    bool HasLineIndices() const { return HasLineAttr("indices"); }

    /// Returns true if all of the followings are true in line_attr_:
    /// 1) attribute "colors" exist
    /// 2) attribute "colors"'s length as "indices"'s length
    /// 3) attribute "colors"'s length > 0
    /// Convenience function.
    bool HasLineColors() const { return HasLineAttr("colors"); }

    /// Clear all data in the line set.
    LineSet &Clear() override {
        point_attr_.clear();
        line_attr_.clear();
        return *this;
    }

    /// Returns !HasPointPositions(), line indices are ignored.
    bool IsEmpty() const override { return !HasPointPositions(); }

    /// Returns the max bound for point coordinates.
    core::Tensor GetMinBound() const { return GetPointPositions().Min({0}); }

    /// Returns the max bound for point coordinates.
    core::Tensor GetMaxBound() const { return GetPointPositions().Max({0}); }

    /// Returns the center for point coordinates.
    core::Tensor GetCenter() const { return GetPointPositions().Mean({0}); }

    /// \brief Transforms the points and lines of the LineSet.
    ///
    /// Custom attributes (e.g.: point or line normals) are not transformed.
    ///
    /// Extracts R, t from Transformation
    ///  T (4x4) =   [[ R(3x3)  t(3x1) ],
    ///               [ O(1x3)  s(1x1) ]]
    ///  (s = 1 for Transformation without scaling)
    /// PS. It Assumes s = 1 and O = [0,0,0]
    /// and applies the transformation as P = R(P) + t
    /// \param transformation Transformation [Tensor of shape {4,4}].
    /// Should be on the same device as the LineSet.
    /// \return Transformed line set.
    LineSet &Transform(const core::Tensor &transformation);

    /// \brief Translates the points and lines of the LineSet.
    /// \param translation Translation tensor of shape {3}
    /// Should be on the same device as the LineSet.
    /// \param relative If true (default) translates relative to center of
    /// LineSet.
    /// \return Translated line set.
    LineSet &Translate(const core::Tensor &translation, bool relative = true);

    /// \brief Scales the points and lines of the LineSet.
    /// \param scale Scale magnitude.
    /// \param center Center [Tensor of shape {3}] about which the LineSet is
    /// to be scaled. Should be on the same device as the LineSet.
    /// \return Scaled line set.
    LineSet &Scale(double scale, const core::Tensor &center);

    /// \brief Rotates the points and lines of the line set. Custom attributes
    /// (e.g.: point or line normals) are not transformed.
    /// \param R Rotation [Tensor of shape {3,3}].
    /// Should be on the same device as the LineSet
    /// \param center Center [Tensor of shape {3}] about which the LineSet is
    /// to be scaled. Should be on the same device as the LineSet.
    /// \return Rotated line set.
    LineSet &Rotate(const core::Tensor &R, const core::Tensor &center);

    /// \brief Returns the device attribute of this LineSet.
    core::Device GetDevice() const { return device_; }

    /// Create a LineSet from a legacy Open3D LineSet.
    /// \param lineset_legacy Legacy Open3D LineSet.
    /// \param float_dtype Float32 or Float64, used to store floating point
    /// values, e.g. points, normals, colors.
    /// \param int_dtype Int32 or Int64, used to store index values, e.g.
    /// line indices.
    /// \param device The device where the resulting LineSet resides.
    static geometry::LineSet FromLegacy(
            const open3d::geometry::LineSet &lineset_legacy,
            core::Dtype float_dtype = core::Float32,
            core::Dtype int_dtype = core::Int64,
            const core::Device &device = core::Device("CPU:0"));

    /// Convert to a legacy Open3D LineSet.
    open3d::geometry::LineSet ToLegacy() const;

protected:
    core::Device device_ = core::Device("CPU:0");
    TensorMap point_attr_;
    TensorMap line_attr_;
};

}  // namespace geometry
}  // namespace t
}  // namespace open3d
