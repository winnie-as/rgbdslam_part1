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

#include "types_six_dof_quat.h"
#include "g2o/core/factory.h"
#include "g2o/stuff/macros.h"

#include <iostream>

namespace g2o {
  using namespace std;

  void G2O_ATTRIBUTE_CONSTRUCTOR init_three_d_types(void)
  {
    Factory* factory = Factory::instance();
    //cerr << "Calling " << __FILE__ << " " << __PRETTY_FUNCTION__ << endl;
    factory->registerType("VERTEX_SE3:QUAT", new HyperGraphElementCreator<VertexSE3>);
    factory->registerType("EDGE_SE3:QUAT", new HyperGraphElementCreator<EdgeSE3>);
    factory->registerType("VERTEX_TRACKXYZ", new HyperGraphElementCreator<VertexTrackXYZ>);

    factory->registerType("VERTEX3", new HyperGraphElementCreator<VertexSE3Euler>);
    factory->registerType("EDGE3", new HyperGraphElementCreator<EdgeSE3Euler>);

    factory->registerType("PARAMS_SE3OFFSET", new HyperGraphElementCreator<SE3OffsetParameters>);
    factory->registerType("EDGE_SE3_TRACKXYZ", new HyperGraphElementCreator<EdgeSE3TrackXYZ>);
    factory->registerType("EDGE_SE3_PRIOR", new HyperGraphElementCreator<EdgeSE3Prior>);
    factory->registerType("EDGE_SE3_OFFSET", new HyperGraphElementCreator<EdgeSE3Offset>);

    factory->registerType("PARAMS_CAMERACALIB", new HyperGraphElementCreator<CameraParameters>);
    factory->registerType("EDGE_PROJECT_DISPARITY", new HyperGraphElementCreator<EdgeProjectDisparity>);    
    factory->registerType("EDGE_PROJECT_DEPTH", new HyperGraphElementCreator<EdgeProjectDepth>);

    factory->registerType("VERTEX_PLANE", new HyperGraphElementCreator<VertexPlane>);
    factory->registerType("EDGE_SE3_PLANE_CALIB", new HyperGraphElementCreator<EdgeSE3PlaneSensorCalib>);

    HyperGraphActionLibrary* actionLib = HyperGraphActionLibrary::instance();
    actionLib->registerAction(new VertexSE3WriteGnuplotAction);
    actionLib->registerAction(new EdgeSE3WriteGnuplotAction);

#ifdef G2O_HAVE_OPENGL
    actionLib->registerAction(new VertexTrackXYZDrawAction);
    actionLib->registerAction(new VertexSE3DrawAction);
    actionLib->registerAction(new EdgeSE3DrawAction);

    HyperGraphElementAction* vertexse3eulerdraw=new VertexSE3DrawAction;
    vertexse3eulerdraw->setTypeName(typeid(VertexSE3Euler).name());
    actionLib->registerAction(vertexse3eulerdraw);

    HyperGraphElementAction* edgese3eulerdraw=new EdgeSE3DrawAction;
    edgese3eulerdraw->setTypeName(typeid(EdgeSE3Euler).name());
    actionLib->registerAction(edgese3eulerdraw);


    actionLib->registerAction(new CameraCacheDrawAction);
    actionLib->registerAction(new VertexPlaneDrawAction);
#endif
  }

} // end namespace
