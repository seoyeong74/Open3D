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

#include "open3d/t/pipelines/registration/TransformationEstimation.h"

#include "open3d/t/pipelines/kernel/ComputeTransform.h"
#include "open3d/t/pipelines/kernel/TransformationConverter.h"

namespace open3d {
namespace t {
namespace pipelines {
namespace registration {

double TransformationEstimationPointToPoint::ComputeRMSE(
        const geometry::PointCloud &source,
        const geometry::PointCloud &target,
        const core::Tensor &correspondences) const {
    core::Device device = source.GetDevice();

    target.GetPoints().AssertDtype(source.GetPoints().GetDtype());
    if (target.GetDevice() != device) {
        utility::LogError(
                "Target Pointcloud device {} != Source Pointcloud's device {}.",
                target.GetDevice().ToString(), device.ToString());
    }

    // TODO: Optimise using kernel.
    core::Tensor valid = correspondences.Ne(-1).Reshape({-1});
    core::Tensor neighbour_indices =
            correspondences.IndexGet({valid}).Reshape({-1});
    core::Tensor source_points_indexed = source.GetPoints().IndexGet({valid});
    core::Tensor target_points_indexed =
            target.GetPoints().IndexGet({neighbour_indices});

    core::Tensor error_t = (source_points_indexed - target_points_indexed);
    error_t.Mul_(error_t);
    double error = error_t.Sum({0, 1}).To(core::Float64).Item<double>();
    return std::sqrt(error /
                     static_cast<double>(neighbour_indices.GetLength()));
}

core::Tensor TransformationEstimationPointToPoint::ComputeTransformation(
        const geometry::PointCloud &source,
        const geometry::PointCloud &target,
        const core::Tensor &correspondences) const {
    core::Tensor R, t;
    // Get tuple of Rotation {3, 3} and Translation {3} of type Float64.
    // [Checks are added in kernel functions].
    std::tie(R, t) = pipelines::kernel::ComputeRtPointToPoint(
            source.GetPoints(), target.GetPoints(), correspondences);

    // Get rigid transformation tensor of {4, 4} of type Float64 on CPU:0
    // device, from rotation {3, 3} and translation {3}.
    return t::pipelines::kernel::RtToTransformation(R, t);
}

double TransformationEstimationPointToPlane::ComputeRMSE(
        const geometry::PointCloud &source,
        const geometry::PointCloud &target,
        const core::Tensor &correspondences) const {
    if (!target.HasPointNormals()) {
        utility::LogError("Target pointcloud missing normals attribute.");
    }

    const core::Device device = source.GetDevice();
    target.GetPoints().AssertDevice(device);
    correspondences.AssertDevice(device);

    const core::Dtype dtype = source.GetPoints().GetDtype();
    target.GetPoints().AssertDtype(dtype);
    target.GetPointNormals().AssertDtype(dtype);
    correspondences.AssertDtype(core::Dtype::Int64);

    core::Tensor valid = correspondences.Ne(-1).Reshape({-1});
    core::Tensor neighbour_indices =
            correspondences.IndexGet({valid}).Reshape({-1});
    core::Tensor source_points_indexed = source.GetPoints().IndexGet({valid});
    core::Tensor target_points_indexed =
            target.GetPoints().IndexGet({neighbour_indices});
    core::Tensor target_normals_indexed =
            target.GetPointNormals().IndexGet({neighbour_indices});

    core::Tensor error_t = (source_points_indexed - target_points_indexed)
                                   .Mul_(target_normals_indexed);
    error_t.Mul_(error_t);
    double error = error_t.Sum({0, 1}).To(core::Float64).Item<double>();
    return std::sqrt(error /
                     static_cast<double>(neighbour_indices.GetLength()));
}

core::Tensor TransformationEstimationPointToPlane::ComputeTransformation(
        const geometry::PointCloud &source,
        const geometry::PointCloud &target,
        const core::Tensor &correspondences) const {
    if (!target.HasPointNormals()) {
        utility::LogError("Target pointcloud missing normals attribute.");
    }

    // Get pose {6} of type Float64. [Checks are added in kernel functions].
    core::Tensor pose = pipelines::kernel::ComputePosePointToPlane(
            source.GetPoints(), target.GetPoints(), target.GetPointNormals(),
            correspondences, this->kernel_);

    // Get rigid transformation tensor of {4, 4} of type Float64 on CPU:0
    // device, from pose {6}.
    return pipelines::kernel::PoseToTransformation(pose);
}

double TransformationEstimationForColoredICP::ComputeRMSE(
        const geometry::PointCloud &source,
        const geometry::PointCloud &target,
        const core::Tensor &correspondences) const {
    if (!target.HasPointColors() || !source.HasPointColors()) {
        utility::LogError(
                "Source and/or Target pointcloud missing colors attribute.");
    }
    if (!target.HasPointNormals()) {
        utility::LogError("Target pointcloud missing normals attribute.");
    }
    if (!target.HasPointAttr("color_gradients")) {
        utility::LogError(
                "Target pointcloud missing color_gradients attribute.");
    }

    const core::Device device = source.GetPoints().GetDevice();
    target.GetPoints().AssertDevice(device);
    correspondences.AssertDevice(device);

    const core::Dtype dtype = source.GetPoints().GetDtype();
    source.GetPointColors().AssertDtype(dtype);
    target.GetPoints().AssertDtype(dtype);
    target.GetPointNormals().AssertDtype(dtype);
    target.GetPointColors().AssertDtype(dtype);
    target.GetPointAttr("color_gradients").AssertDtype(dtype);
    correspondences.AssertDtype(core::Dtype::Int64);

    double sqrt_lambda_geometric = sqrt(lambda_geometric_);
    double lambda_photometric = 1.0 - lambda_geometric_;
    double sqrt_lambda_photometric = sqrt(lambda_photometric);

    core::Tensor valid = correspondences.Ne(-1).Reshape({-1});
    core::Tensor neighbour_indices =
            correspondences.IndexGet({valid}).Reshape({-1});

    // vs - source points (or vertices)
    // vt - target points
    // nt - target normals
    // cs - source colors
    // ct - target colors
    // dit - target color gradients
    // is - source intensity
    // it - target intensity
    // vs_proj - source points projection
    // is_proj - source intensity projection

    core::Tensor vs = source.GetPoints().IndexGet({valid});
    core::Tensor cs = source.GetPointColors().IndexGet({valid});

    core::Tensor vt = target.GetPoints().IndexGet({neighbour_indices});
    core::Tensor nt = target.GetPointNormals().IndexGet({neighbour_indices});
    core::Tensor ct = target.GetPointColors().IndexGet({neighbour_indices});
    core::Tensor dit = target.GetPointAttr("color_gradients")
                               .IndexGet({neighbour_indices});

    // vs_proj = vs - (vs - vt).dot(nt) * nt
    // d = (vs - vt).dot(nt)
    const core::Tensor d = (vs - vt).Mul(nt).Sum({1});
    core::Tensor vs_proj = vs - d.Mul(nt);

    core::Tensor is = cs.Mean({1});
    core::Tensor it = ct.Mean({1});

    // is_proj = (dit.dot(vs_proj - vt)) + it
    core::Tensor is_proj = (dit.Mul(vs_proj - vt)).Sum({1}).Add(it);

    core::Tensor residual_geometric = d.Mul(sqrt_lambda_geometric).Sum({1});
    core::Tensor sq_residual_geometric =
            residual_geometric.Mul(residual_geometric);
    core::Tensor residual_photometric =
            (is - is_proj).Mul(sqrt_lambda_photometric).Sum({1});
    core::Tensor sq_residual_photometric =
            residual_photometric.Mul(residual_photometric);

    double residual = sq_residual_geometric.Add_(sq_residual_photometric)
                              .Sum({0})
                              .To(core::Float64)
                              .Item<double>();

    return residual;
}

core::Tensor TransformationEstimationForColoredICP::ComputeTransformation(
        const geometry::PointCloud &source,
        const geometry::PointCloud &target,
        const core::Tensor &correspondences) const {
    if (!target.HasPointColors() || !source.HasPointColors()) {
        utility::LogError(
                "Source and/or Target pointcloud missing colors attribute.");
    }
    if (!target.HasPointNormals()) {
        utility::LogError("Target pointcloud missing normals attribute.");
    }
    if (!target.HasPointAttr("color_gradients")) {
        utility::LogError(
                "Target pointcloud missing color_gradients attribute.");
    }

    // Get pose {6} of type Float64 from correspondences indexed source and
    // target point cloud.
    core::Tensor pose = pipelines::kernel::ComputePoseColoredICP(
            source.GetPoints(), source.GetPointColors(), target.GetPoints(),
            target.GetPointNormals(), target.GetPointColors(),
            target.GetPointAttr("color_gradients"), correspondences,
            this->kernel_, this->lambda_geometric_);

    // Get transformation {4,4} of type Float64 from pose {6}.
    core::Tensor transform = pipelines::kernel::PoseToTransformation(pose);

    return transform;
}

}  // namespace registration
}  // namespace pipelines
}  // namespace t
}  // namespace open3d
