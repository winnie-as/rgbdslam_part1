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

#ifndef LASER_PARAMETERS_H
#define LASER_PARAMETERS_H

#include "g2o/math_groups/se2.h"

namespace g2o {

  struct LaserParameters
  {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
    LaserParameters(int type, int beams, double firstBeamAngle, double angularStep, double maxRange, double accuracy, int remissionMode);
    LaserParameters(int beams, double firstBeamAngle, double angularStep, double maxRange);
    SE2 laserPose;
    int type;
    double firstBeamAngle;
    double fov;
    double angularStep;
    double accuracy;
    int remissionMode;
    double maxRange;
  };

}

#endif
