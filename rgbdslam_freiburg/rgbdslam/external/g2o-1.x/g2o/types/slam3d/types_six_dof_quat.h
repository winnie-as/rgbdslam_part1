// g2o - General Graph Optimization
// Copyright (C) 2011 R. Kuemmerle, G. Grisetti, W. Burgard
// 
// g2o is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// g2o is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef THREE_D_TYPES
#define THREE_D_TYPES

#include "g2o/config.h"
#include "g2o/core/base_vertex.h"
#include "g2o/core/base_binary_edge.h"
#include "g2o/core/hyper_graph_action.h"
#include "g2o/math_groups/se3quat.h"


#define THREE_D_TYPES_ANALYTIC_JACOBIAN

#include "vertex_se3_quat.h"
#include "edge_se3_quat.h"
#include "vertex_trackxyz.h"

#include "vertex_se3_euler.h"
#include "edge_se3_euler.h"

#include "se3_offset_parameters.h"
#include "edge_se3_trackxyz.h"
#include "edge_se3_offset.h"

#include "camera_parameters.h"
#include "edge_project_disparity.h"
#include "edge_project_depth.h"
#include "edge_se3_prior.h"

#include "vertex_plane.h"
#include "edge_se3_plane_calib.h"

#endif
