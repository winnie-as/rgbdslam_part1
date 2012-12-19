#include "vertex_trackxyz.h"
#include <stdio.h>

#ifdef WINDOWS
#include <windows.h>
#endif

#ifdef G2O_HAVE_OPENGL
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

#include <typeinfo>

namespace g2o {
  // TRACK VERTEX

  bool VertexTrackXYZ::read(std::istream& is) {
    Vector3d lv;
    for (int i=0; i<3; i++)
      is >> lv[i];
    setEstimate(lv);
    return true;
  }

  bool VertexTrackXYZ::write(std::ostream& os) const {
    Vector3d lv=estimate();
    for (int i=0; i<3; i++){
      os << lv[i] << " ";
    }
    return os.good();
  }


#ifdef G2O_HAVE_OPENGL
  VertexTrackXYZDrawAction::VertexTrackXYZDrawAction(): DrawAction(typeid(VertexTrackXYZ).name()){
  }

  bool VertexTrackXYZDrawAction::refreshPropertyPtrs(HyperGraphElementAction::Parameters* params_){
    if (! DrawAction::refreshPropertyPtrs(params_))
      return false;
    if (_previousParams){
      _pointSize = _previousParams->makeProperty<FloatProperty>(_typeName + "::POINT_SIZE", 1.);
    } else {
      _pointSize = 0;
    }
    return true;
  }


  HyperGraphElementAction* VertexTrackXYZDrawAction::operator()(HyperGraph::HyperGraphElement* element, 
							       HyperGraphElementAction::Parameters* params ){

    if (typeid(*element).name()!=_typeName)
      return 0;
    refreshPropertyPtrs(params);
    if (! _previousParams)
      return this;
    
    if (_show && !_show->value())
      return this;
    VertexTrackXYZ* that = static_cast<VertexTrackXYZ*>(element);
    

    glPushAttrib(GL_ENABLE_BIT | GL_POINT_BIT);
    glDisable(GL_LIGHTING);
    glColor3f(0.8,0.5,0.3);
    if (_pointSize) {
      glPointSize(_pointSize->value());
    }
    glBegin(GL_POINTS);
    glVertex3f(that->estimate()(0),that->estimate()(1),that->estimate()(2));
    glEnd();
    glPopAttrib();
    return this;
  }
#endif

}

