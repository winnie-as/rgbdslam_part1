/* This file is part of RGBDSLAM.
 * 
 * RGBDSLAM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * RGBDSLAM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with RGBDSLAM.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "ros/ros.h"
#include <QtGui>
#include <QtOpenGL>
#include <QThread>
#include <GL/gl.h>
#include <GL/glut.h>
#include "boost/foreach.hpp"
#include <cmath>
#include <QApplication>
#include <QAction>
#include <QMenu>
#ifdef GL2PS
#include <gl2ps.h>
#endif
#include "glviewer.h"
#include "misc2.h"
#include "scoped_timer.h"

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE  0x809D
#endif

const double PI= 3.14159265358979323846;

template <typename PointType>
inline bool validXYZ(const PointType& p){
      if(ParameterServer::instance()->get<double>("maximum_depth") < p.z) return false;
      return std::isfinite(p.z) && std::isfinite(p.y) && std::isfinite(p.x);
};

template <typename PointType>
inline void setGLColor(const PointType& p){
    unsigned char b,g,r;
#ifndef RGB_IS_4TH_DIM
    b = *(  (unsigned char*)(&p.rgb));
    g = *(1+(unsigned char*)(&p.rgb));
    r = *(2+(unsigned char*)(&p.rgb));
#else
    b = *(  (unsigned char*)(&p.data[3]));
    g = *(1+(unsigned char*)(&p.data[3]));
    r = *(2+(unsigned char*)(&p.data[3]));
#endif
    glColor3ub(r,g,b); //glColor3f(1.0,1.0,1.0);
};

GLViewer::GLViewer(QWidget *parent)
    : QGLWidget(QGLFormat(QGL::SampleBuffers|QGL::StereoBuffers), parent),
      xRot(180*16.0),
      yRot(0),
      zRot(0),
      xTra(0),
      yTra(0),
      zTra(-50),//go back 50 (pixels?)
      polygon_mode(GL_FILL),
      cloud_list_indices(),
      cloud_matrices(new QList<QMatrix4x4>()),
      show_poses_(ParameterServer::instance()->get<bool>("show_axis")),
      show_ids_(false),
      show_grid_(false), 
      show_tfs_(false), 
      show_edges_(ParameterServer::instance()->get<bool>("show_axis")),
      show_clouds_(true),
      show_features_(false),
      follow_mode_(true),
      stereo_(false),
      black_background_(true),
      width_(0),
      height_(0),
      stereo_shift_(0.1),
      fov_(100.0/180.0*PI),
      rotation_stepping_(1.0),
      myparent(parent),
      button_pressed_(false),
      fast_rendering_step_(1)
{
    bg_col_[0] = bg_col_[1] = bg_col_[2] = bg_col_[3] = 0.0;//black background
    ROS_DEBUG_COND(!this->format().stereo(), "Stereo not supported");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); //can make good use of more space
    viewpoint_tf_.setToIdentity();
}

GLViewer::~GLViewer() { }

QSize GLViewer::minimumSizeHint() const {
    return QSize(400, 400);
}

QSize GLViewer::sizeHint() const {
    return QSize(640, 480);
}

static void qNormalizeAngle(int &angle) {
    while (angle < 0)
        angle += 360 * 16;
    while (angle > 360 * 16)
        angle -= 360 * 16;
}

void GLViewer::setXRotation(int angle) { 
    qNormalizeAngle(angle);
    if (angle != xRot) {
        xRot = angle;
    }
}


void GLViewer::setYRotation(int angle) {
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
    }
}

void GLViewer::setRotationGrid(double rot_step_in_degree) {
  rotation_stepping_ = rot_step_in_degree;
}

void GLViewer::setStereoShift(double shift) {
  stereo_shift_ = shift;
  clearAndUpdate();
}

void GLViewer::setZRotation(int angle) {
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
    }
}

void GLViewer::initializeGL() {
    glClearColor(bg_col_[0],bg_col_[1],bg_col_[2],bg_col_[3]); 
    glEnable (GL_BLEND); 
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LINE_SMOOTH);
    //glEnable(GL_POINT_SMOOTH);
    //glShadeModel(GL_SMOOTH);
    //glEnable(GL_LIGHTING);
    //glEnable(GL_LIGHT0);
    //glEnable(GL_MULTISAMPLE);
    //gluPerspective(fov_, 1.00, 0.01, 1e9); //1.38 = tan(57/2°)/tan(43/2°)
    ////gluLookAt(0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 1.0, 0.0);
    //static GLfloat lightPosition[4] = { 0.5, 5.0, 7.0, 1.0 };
    //glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
}
//assumes gl mode for triangles
inline
void GLViewer::drawTriangle(const point_type& p1, const point_type& p2, const point_type& p3){
    setGLColor(p1);
    glVertex3f(p1.x, p1.y, p1.z);

    setGLColor(p2);
    glVertex3f(p2.x, p2.y, p2.z);

    setGLColor(p3);
    glVertex3f(p3.x, p3.y, p3.z);
}

void GLViewer::drawGrid(){
    //glEnable (GL_LINE_STIPPLE);
    //glLineStipple (1, 0x0F0F);
    //glEnable(GL_BLEND); 
    glBegin(GL_LINES);
    glLineWidth(1);
    ParameterServer* ps = ParameterServer::instance();
    float scale = ps->get<double>("gl_cell_size");
    //glColor4f(1-bg_col_[0],1-bg_col_[1],1-bg_col_[2],0.4); //invers of background, transp
    glColor3f(0.5,0.5,0.5); //gray
    int size = ps->get<int>("gl_grid_size_xy")/2;
    for(int i = -size; i <= size ; i++){
      //glColor4f(0.2,0.5,0.2,1.0); //green, the color of the y axis
      glVertex3f(i*scale,  size*scale, 0);
      glVertex3f(i*scale, -size*scale, 0);
      //glColor4f(0.5,0.2,0.2,1.0); //red, the color of the x axis
      glVertex3f( size*scale, i*scale, 0);
      glVertex3f(-size*scale, i*scale, 0);
    }
    size = ps->get<int>("gl_grid_size_xz")/2;
    for(int i = -size; i <= size ; i++){
      //glColor4f(0.2,0.2,0.5,1.0); //blue, the color of the z axis
      glVertex3f(i*scale, 0,  size*scale);
      glVertex3f(i*scale, 0, -size*scale);
      //glColor4f(0.5,0.2,0.2,1.0); //red, the color of the x axis
      glVertex3f( size*scale, 0, i*scale);
      glVertex3f(-size*scale, 0, i*scale);
    }
    size = ps->get<int>("gl_grid_size_yz")/2;
    for(int i = -size; i <= size ; i++){
      //glColor4f(0.2,0.2,0.5,1.0); //blue, the color of the z axis
      glVertex3f(0, i*scale,  size*scale);
      glVertex3f(0, i*scale, -size*scale);
      //glColor4f(0.2,0.5,0.2,1.0); //green, the color of the y axis
      glVertex3f(0,  size*scale, i*scale);
      glVertex3f(0, -size*scale, i*scale);
    }
    glEnd();
    //glDisable (GL_LINE_STIPPLE);
}

void GLViewer::drawAxis(float scale){
    glEnable(GL_BLEND); 
    glBegin(GL_LINES);
    glLineWidth(4);
    glColor4f (0.9, 0, 0, 1.0);
    glVertex3f(0, 0, 0);
    glColor4f (0.9, 0, 0, 0.0);
    glVertex3f(scale, 0, 0);
    glColor4f (0, 0.9, 0, 1.0);
    glVertex3f(0, 0, 0);
    glColor4f (0, 0.9, 0, 0.0);
    glVertex3f(0, scale, 0);
    glColor4f (0, 0, 0.9, 1.0);
    glVertex3f(0, 0, 0);
    glColor4f (0, 0, 0.9, 0.0);
    glVertex3f(0, 0, scale);
    glEnd();
}

void GLViewer::paintGL() {
    if(!this->isVisible()) return;
    //ROS_INFO("This is paint-thread %d", (unsigned int)QThread::currentThreadId());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPointSize(ParameterServer::instance()->get<double>("gl_point_size"));
    if(stereo_){
        float ratio = (float)(width_) / (float) height_;
        glViewport(0, 0, width_/2, height_);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov_, ratio, 0.1, 1e4); 
        glMatrixMode(GL_MODELVIEW);
        drawClouds(stereo_shift_);

        glViewport(width_/2, 0, width_/2, height_);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov_, ratio, 0.1, 1e4); 
        glMatrixMode(GL_MODELVIEW);
    }
    drawClouds(0.0);
}

void GLViewer::drawClouds(float xshift) {
    ScopedTimer s(__FUNCTION__);
    if(follow_mode_){
        int id = cloud_matrices->size()-1;
        if(id >= 0)setViewPoint((*cloud_matrices)[id]);
    }
    glDisable (GL_BLEND);  //Don't blend the clouds, they have no alpha values
    glLoadIdentity();
    //Camera transformation
    glTranslatef(xTra+xshift, yTra, zTra);
    int x_steps = (xRot / 16.0)/rotation_stepping_;
    int y_steps = (yRot / 16.0)/rotation_stepping_;
    int z_steps = (zRot / 16.0)/rotation_stepping_;
    glRotatef(x_steps*rotation_stepping_, 1.0, 0.0, 0.0);
    glRotatef(y_steps*rotation_stepping_, 0.0, 1.0, 0.0);
    glRotatef(z_steps*rotation_stepping_, 0.0, 0.0, 1.0);
    glMultMatrixd(static_cast<GLdouble*>( viewpoint_tf_.data() ));//works as long as qreal and GLdouble are typedefs to double (might depend on hardware)
    if(show_grid_) {
      drawGrid(); //Draw a 10x10 grid with 1m x 1m cells
    }
    if(show_poses_) drawAxis(0.2);//Show origin as big axis

    ROS_DEBUG("Drawing %i PointClouds", cloud_list_indices.size());
    int step = button_pressed_ ? fast_rendering_step_ : 1; //if last_draw_duration_ was bigger than 100hz, skip clouds in drawing when button pressed
    for(int i = 0; i<cloud_list_indices.size() && i<cloud_matrices->size(); i+=step){
        glPushMatrix();
        glMultMatrixd(static_cast<GLdouble*>( (*cloud_matrices)[i].data() ));//works as long as qreal and GLdouble are typedefs to double (might depend on hardware)
        if(show_clouds_) glCallList(cloud_list_indices[i]);
        if(show_features_ && feature_list_indices.size()>i){
          glCallList(feature_list_indices[i]);
        }
        glPopMatrix();
    }

    glDisable(GL_DEPTH_TEST);
    if(show_edges_) drawEdges();

    for(int i = 0; i<cloud_list_indices.size() && i<cloud_matrices->size(); i++){
        glPushMatrix();
        glMultMatrixd(static_cast<GLdouble*>( (*cloud_matrices)[i].data() ));//works as long as qreal and GLdouble are typedefs to double (might depend on hardware)
        if(show_poses_) drawAxis((i + 1 == cloud_list_indices.size()) ? 0.5:0.075); //Draw last pose Big
        if(show_ids_) {
          glColor4f(1-bg_col_[0],1-bg_col_[1],1-bg_col_[2],1.0); //inverse of bg color
          this->renderText(0.,0.,0.,QString::number(i), QFont("Monospace", 8));
        }
        glPopMatrix();
    }
    glEnable(GL_DEPTH_TEST);
    if(button_pressed_){
      if(s.elapsed() > 0.05) { //Try to maintain high speed rendering if button is pressed
        fast_rendering_step_++;
        ROS_INFO("Increased renderer skipto every %d. cloud during motion", fast_rendering_step_);
      }
    }
}

void GLViewer::resizeGL(int width, int height)
{
    width_ = width;
    height_ = height;
    //int side = qMin(width, height);
    glViewport(0, 0, width, height);
    //glViewport((width - side) / 2, (height - side) / 2, side, side);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
//#ifdef QT_OPENGL_ES_1
//    glOrthof(-0.5, +0.5, -0.5, +0.5, 1.0, 15.0);
//#else
//    glOrtho(-0.5, +0.5, -0.5, +0.5, 1.0, 15.0);
//#endif
    //gluPerspective(fov_, 1.38, 0.01, 1e9); //1.38 = tan(57/2°)/tan(43/2°) as kinect has viewing angles 57 and 43
    float ratio = (float)width / (float) height;
    gluPerspective(fov_, ratio, 0.1, 1e4); 
    glMatrixMode(GL_MODELVIEW);
}

void GLViewer::mouseDoubleClickEvent(QMouseEvent *event) {
    xRot=180*16.0;
    yRot=0;
    zRot=0;
    xTra=0;
    yTra=0;
    if(cloud_matrices->size()>0){
      int id = 0;
      switch (QApplication::keyboardModifiers()){
        case Qt::NoModifier:  
            id = cloud_matrices->size()-1;
        case Qt::ControlModifier:  
            setViewPoint((*cloud_matrices)[id]);
            break;
        case Qt::ShiftModifier:  
            viewpoint_tf_.setToIdentity();
      }
      if (event->buttons() & Qt::MidButton) { 
          zTra=-50;
      }
    }
    /*
    if(!setClickedPosition(event->x(), event->y())){
      ROS_INFO("Clicked into space");
    }
    */
    clearAndUpdate();
}
void GLViewer::toggleStereo(bool flag){
  stereo_ = flag;
  resizeGL(width_, height_);
  clearAndUpdate();
}
void GLViewer::toggleBackgroundColor(bool flag){
  black_background_ = flag;
  if(flag){
    bg_col_[0] = bg_col_[1] = bg_col_[2] = bg_col_[3] = 0.0;//black background
  }
  else{
    bg_col_[0] = bg_col_[1] = bg_col_[2] = 1.0;//white background
  }
  glClearColor(bg_col_[0],bg_col_[1],bg_col_[2],bg_col_[3]); 
  clearAndUpdate();
}
void GLViewer::toggleShowFeatures(bool flag){
  show_features_ = flag;
  clearAndUpdate();
}
void GLViewer::toggleShowClouds(bool flag){
  show_clouds_ = flag;
  clearAndUpdate();
}
void GLViewer::toggleShowTFs(bool flag){
  show_tfs_ = flag;
  clearAndUpdate();
}
void GLViewer::toggleShowGrid(bool flag){
  show_grid_ = flag;
  clearAndUpdate();
}
void GLViewer::toggleShowIDs(bool flag){
  show_ids_ = flag;
  clearAndUpdate();
}
void GLViewer::toggleShowEdges(bool flag){
  show_edges_ = flag;
  clearAndUpdate();
}
void GLViewer::toggleShowPoses(bool flag){
  show_poses_ = flag;
  clearAndUpdate();
}
void GLViewer::toggleFollowMode(bool flag){
  follow_mode_ = flag;
}

/** Create context menu */
void GLViewer::mouseReleaseEvent(QMouseEvent *event) {
  button_pressed_ = false;
  clearAndUpdate();

  if(event->button() == Qt::RightButton)
  {
      QMenu menu;
      QMenu* viewMenu = &menu; //ease copy and paste from qt_gui.cpp
      GLViewer* glviewer = this;//ease copy and paste from qt_gui.cpp

      QAction *toggleCloudDisplay = new QAction(tr("Show &Clouds"), this);
      toggleCloudDisplay->setCheckable(true);
      toggleCloudDisplay->setChecked(show_clouds_);
      toggleCloudDisplay->setStatusTip(tr("Toggle whether point clouds should be rendered"));
      connect(toggleCloudDisplay, SIGNAL(toggled(bool)), glviewer, SLOT(toggleShowClouds(bool)));
      viewMenu->addAction(toggleCloudDisplay);

      QAction *toggleShowPosesAct = new QAction(tr("Show &Poses of Graph"), this);
      toggleShowPosesAct->setShortcut(QString("P"));
      toggleShowPosesAct->setCheckable(true);
      toggleShowPosesAct->setChecked(show_poses_);
      toggleShowPosesAct->setStatusTip(tr("Display poses as axes"));
      connect(toggleShowPosesAct, SIGNAL(toggled(bool)), glviewer, SLOT(toggleShowPoses(bool)));
      viewMenu->addAction(toggleShowPosesAct);

      QAction *toggleShowIDsAct = new QAction(tr("Show Pose IDs"), this);
      toggleShowIDsAct->setShortcut(QString("I"));
      toggleShowIDsAct->setCheckable(true);
      toggleShowIDsAct->setChecked(show_ids_);
      toggleShowIDsAct->setStatusTip(tr("Display pose ids at axes"));
      connect(toggleShowIDsAct, SIGNAL(toggled(bool)), glviewer, SLOT(toggleShowIDs(bool)));
      viewMenu->addAction(toggleShowIDsAct);

      QAction *toggleShowEdgesAct = new QAction(tr("Show &Edges of Graph"), this);
      toggleShowEdgesAct->setShortcut(QString("E"));
      toggleShowEdgesAct->setCheckable(true);
      toggleShowEdgesAct->setChecked(show_edges_);
      toggleShowEdgesAct->setStatusTip(tr("Display edges of pose graph as lines"));
      connect(toggleShowEdgesAct, SIGNAL(toggled(bool)), glviewer, SLOT(toggleShowEdges(bool)));
      viewMenu->addAction(toggleShowEdgesAct);

      QAction *toggleShowTFs = new QAction(tr("Show Pose TFs"), this);
      toggleShowTFs->setCheckable(true);
      toggleShowTFs->setChecked(show_tfs_);
      toggleShowTFs->setStatusTip(tr("Display pose transformations at axes"));
      connect(toggleShowTFs, SIGNAL(toggled(bool)), glviewer, SLOT(toggleShowTFs(bool)));
      viewMenu->addAction(toggleShowTFs);

      QAction *toggleShowFeatures = new QAction(tr("Show &Feature Locations"), this);
      toggleShowFeatures->setCheckable(true);
      toggleShowFeatures->setChecked(show_features_);
      toggleShowFeatures->setStatusTip(tr("Toggle whether feature locations should be rendered"));
      connect(toggleShowFeatures, SIGNAL(toggled(bool)), glviewer, SLOT(toggleShowFeatures(bool)));
      viewMenu->addAction(toggleShowFeatures);

      QAction *toggleTriangulationAct = new QAction(tr("&Toggle Triangulation"), this);
      toggleTriangulationAct->setShortcut(QString("T"));
      toggleTriangulationAct->setStatusTip(tr("Switch between surface, wireframe and point cloud"));
      connect(toggleTriangulationAct, SIGNAL(triggered(bool)), glviewer, SLOT(toggleTriangulation()));
      viewMenu->addAction(toggleTriangulationAct);

      QAction *toggleFollowAct = new QAction(tr("Follow &Camera"), this);
      toggleFollowAct->setShortcut(QString("Shift+F"));
      toggleFollowAct->setCheckable(true);
      toggleFollowAct->setChecked(follow_mode_);
      toggleFollowAct->setStatusTip(tr("Always use viewpoint of last frame (except zoom)"));
      connect(toggleFollowAct, SIGNAL(toggled(bool)), glviewer, SLOT(toggleFollowMode(bool)));
      viewMenu->addAction(toggleFollowAct);

      QAction *toggleShowGrid = new QAction(tr("Show Grid"), this);
      toggleShowGrid->setCheckable(true);
      toggleShowGrid->setChecked(show_grid_);
      toggleShowGrid->setStatusTip(tr("Display XY plane grid"));
      connect(toggleShowGrid, SIGNAL(toggled(bool)), glviewer, SLOT(toggleShowGrid(bool)));
      viewMenu->addAction(toggleShowGrid);

      QAction *toggleBGColor = new QAction(tr("Toggle Background"), this);
      toggleBGColor->setShortcut(QString("B"));
      toggleBGColor->setCheckable(true);
      toggleBGColor->setChecked(black_background_);
      toggleBGColor->setStatusTip(tr("Toggle whether background should be white or black"));
      connect(toggleBGColor, SIGNAL(toggled(bool)), glviewer, SLOT(toggleBackgroundColor(bool)));
      viewMenu->addAction(toggleBGColor);

      viewMenu->exec(mapToGlobal(event->pos()));
  }
  QGLWidget::mouseReleaseEvent(event);  //Dont forget to pass on the event to parent
}

void GLViewer::mousePressEvent(QMouseEvent *event) {
  button_pressed_ = true;
  lastPos = event->pos();
}

void GLViewer::wheelEvent(QWheelEvent *event) {
    zTra += ((float)event->delta())/25.0; 
    clearAndUpdate();
}
void GLViewer::mouseMoveEvent(QMouseEvent *event) {//TODO: consolidate setRotation methods
    int dx = event->x() - lastPos.x();
    int dy = event->y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
      switch (QApplication::keyboardModifiers()){
        case Qt::NoModifier:  
            setXRotation(xRot - 8 * dy);
            setYRotation(yRot + 8 * dx);
            break;
        case Qt::ControlModifier:  
            setXRotation(xRot - 8 * dy);
            setZRotation(zRot + 8 * dx);
            break;
        case Qt::ShiftModifier:  
            xTra += dx/200.0;
            yTra -= dy/200.0;
      }
      clearAndUpdate();
    } else if (event->buttons() & Qt::MidButton) {
            xTra += dx/200.0;
            yTra -= dy/200.0;
      clearAndUpdate();
    }

    lastPos = event->pos();
}

void GLViewer::updateTransforms(QList<QMatrix4x4>* transforms){
    ROS_WARN_COND(transforms->size() < cloud_matrices->size(), "Got less transforms than before!");
    // This doesn't deep copy, but should work, as qlist maintains a reference count 
    //FIXME: This should be replaced by a mutex'ed delete. Requires also to mutex all the read accesses
    QList<QMatrix4x4>* cloud_matrices_tmp = cloud_matrices;
    cloud_matrices = transforms; 
    ROS_DEBUG("New Cloud matrices size: %d", cloud_matrices->size());
    //clearAndUpdate();
    //FIXME: This should be replaced by a mutex'ed delete. Requires also to mutex all the read accesses
    delete cloud_matrices_tmp;
}

void GLViewer::addPointCloud(pointcloud_type * pc, QMatrix4x4 transform){
    ROS_DEBUG("pc pointer in addPointCloud: %p (this is %p in thread %d)", pc, this, (unsigned int)QThread::currentThreadId());
    if(ParameterServer::instance()->get<double>("squared_meshing_threshold") < 0){
      pointCloud2GLPoints(pc);
    } else {
      pointCloud2GLStrip(pc);
    }
    cloud_matrices->push_back(transform); //keep for later
    clearAndUpdate();
    //QApplication::processEvents(QEventLoop::ExcludeSocketNotifiers);
    /*
    std::string file_prefix = ParameterServer::instance()->get<std::string>("screencast_path_prefix");
    if(!file_prefix.empty()){
      QString filename;
      filename.sprintf("%s%.5d.png", file_prefix.c_str(), cloud_matrices->size());
      QPixmap::grabWidget(this->myparent).save(filename);
    }
    */
}

void GLViewer::addFeatures(const std::vector<Eigen::Vector4f, Eigen::aligned_allocator<Eigen::Vector4f> >* feature_locations_3d)
{
    ScopedTimer s(__FUNCTION__);
    ROS_DEBUG("Making GL list from feature points");
    GLuint feature_list_index = glGenLists(1);
    if(!feature_list_index) {
        ROS_ERROR("No display list could be created");
        return;
    }
    glNewList(feature_list_index, GL_COMPILE);
    feature_list_indices.push_back(feature_list_index);
    glBegin(GL_LINES);
    float r = (float)rand()/(float)RAND_MAX;
    float g = (float)rand()/(float)RAND_MAX;
    float b = (float)rand()/(float)RAND_MAX;
    BOOST_FOREACH(const Eigen::Vector4f& ft, *feature_locations_3d)
    {
      if(std::isfinite(ft[2])){
        glColor4f(r,g,b, 1.0); //inverse of bg color, non transp
        //glColor4f(1-bg_col_[0],bg_col_[1],bg_col_[2],1.0); //inverse of bg color, non transp
        glVertex3f(ft[0], ft[1], ft[2]);
        glColor4f(r,g,b, 0.0); //inverse of bg color, non transp
        //glColor4f(1-bg_col_[0],bg_col_[1],bg_col_[2],0.0); //inverse of bg color, non transp
        glVertex3f(ft[0], ft[1], ft[2]-sqrt(depth_covariance(ft[2])));
      }
    }
    glEnd();
    glEndList();
}

void GLViewer::pointCloud2GLStrip(pointcloud_type * pc){
    ScopedTimer s(__FUNCTION__);
    ROS_DEBUG("Making GL list from point-cloud pointer %p in thread %d", pc, (unsigned int)QThread::currentThreadId());
    GLuint cloud_list_index = glGenLists(1);
    if(!cloud_list_index) {
        ROS_ERROR("No display list could be created");
        return;
    }
    float mesh_thresh = ParameterServer::instance()->get<double>("squared_meshing_threshold");
    glNewList(cloud_list_index, GL_COMPILE);
    //ROS_INFO_COND(!pc->is_dense, "Expected dense cloud for opengl drawing");
    point_type origin;
    origin.x = 0;
    origin.y = 0;
    origin.z = 0;

    float depth;
    bool strip_on = false, flip = false; //if flip is true, first the lower then the upper is inserted
    const int w=pc->width, h=pc->height;
    unsigned char b,g,r;
    const int step = ParameterServer::instance()->get<int>("visualization_skip_step");
    for( int y = 0; y < h-step; y+=step){ //go through every point and make two triangles 
        for( int x = 0; x < w-step; x+=step){//for it and its neighbours right and/or down
            using namespace pcl;
            if(!strip_on){ //Generate vertices for new triangle
                const point_type* ll = &pc->points[(x)+(y+step)*w]; //one down (lower left corner)
                if(!validXYZ(*ll)) continue; // both new triangles in this step would use this point
                const point_type* ur = &pc->points[(x+step)+y*w]; //one right (upper right corner)
                if(!validXYZ(*ur)) continue; // both new triangles in this step would use this point
          
                const point_type* ul = &pc->points[x+y*w]; //current point (upper right)
                if(validXYZ(*ul)){ //ul, ur, ll all valid
                  depth = squaredEuclideanDistance(*ul,origin);
                  if (squaredEuclideanDistance(*ul,*ll)/depth <= mesh_thresh  and 
                      squaredEuclideanDistance(*ul,*ll)/depth <= mesh_thresh  and
                      squaredEuclideanDistance(*ur,*ll)/depth <= mesh_thresh){
                    glBegin(GL_TRIANGLE_STRIP);
                    strip_on = true;
                    flip = false; //correct order, upper first
                    //Prepare the first two vertices of a triangle strip
                    //drawTriangle(*ul, *ll, *ur);
                    setGLColor(*ul);

                    //glColor3ub(255,0,0);
                    glVertex3f(ul->x, ul->y, ul->z);
                  }
                } 
                if(!strip_on) { //can't use the point on the upper left, should I still init a triangle?
                  const point_type* lr = &pc->points[(x+step)+(y+step)*w]; //one right-down (lower right)
                  if(!validXYZ(*lr)) {
                    //if this is not valid, there is no way to make a new triangle in the next step
                    //and one could have been drawn starting in this step, only if ul had been valid
                    x++;
                    continue;
                  } else { //at least one can be started at the lower left
                    depth = squaredEuclideanDistance(*ur,origin);
                    if (squaredEuclideanDistance(*ur,*ll)/depth <= mesh_thresh  and 
                        squaredEuclideanDistance(*lr,*ll)/depth <= mesh_thresh  and
                        squaredEuclideanDistance(*ur,*lr)/depth <= mesh_thresh){
                      glBegin(GL_TRIANGLE_STRIP);
                      strip_on = true;
                      flip = true; //but the lower has to be inserted first, for correct order
                    }
                  }
                }
                if(strip_on) { //Be this the second or the first vertex, insert it
                  setGLColor(*ll);

                  //glColor3ub(0,255,0);
                  glVertex3f(ll->x, ll->y, ll->z);
                }
                continue; //not relevant but demonstrate that nothing else is done in this iteration
            } // end strip was off
            else 
            {//neighbours to the left and left down are already set
              const point_type* ul;
              if(flip){ ul = &pc->points[(x)+(y+step)*w]; } //one down (lower left corner) 
              else { ul = &pc->points[x+y*w]; } //current point (upper right)
              if(validXYZ(*ul)){ //Neighbours to the left are prepared
                depth = squaredEuclideanDistance(*ul,origin);
                if (squaredEuclideanDistance(*ul,*(ul-step))/depth > mesh_thresh){
                  glEnd();
                  strip_on = false;
                  continue;
                }
                //Complete the triangle with both leftern neighbors
                //drawTriangle(*ul, *ll, *ur);
                setGLColor(*ul);

                //glColor3ub(255,0,0);
                glVertex3f(ul->x, ul->y, ul->z);
              } else {
                glEnd();
                strip_on = false;
                continue; //TODO: Could restart with next point instead
              }
              //The following point connects one to the left with the other on this horizontal level
              const point_type* ll;
              if(flip){ ll = &pc->points[x+y*w]; } //current point (upper right)
              else { ll = &pc->points[(x)+(y+step)*w]; } //one down (lower left corner) 
              if(validXYZ(*ll)){ 
                depth = squaredEuclideanDistance(*ll,origin);
                if (squaredEuclideanDistance(*ul,*ll)/depth > mesh_thresh or
                    squaredEuclideanDistance(*ul,*(ul-step))/depth > mesh_thresh or
                    squaredEuclideanDistance(*ll,*(ll-step))/depth > mesh_thresh){
                  glEnd();
                  strip_on = false;
                  continue;
                }
                setGLColor(*ul);

                glVertex3f(ll->x, ll->y, ll->z);
              } else {
                glEnd();
                strip_on = false;
                continue;
              }
            }//completed triangles if strip is running
        }
        if(strip_on) glEnd();
        strip_on = false;
    }
    ROS_DEBUG("Compiled pointcloud into list %i",  cloud_list_index);
    glEndList();
    cloud_list_indices.push_back(cloud_list_index);
    //pointcloud_type pc_empty;
    //pc_empty.points.swap(pc->points);
    //pc->width = 0;
    //pc->height = 0;
    Q_EMIT cloudRendered(pc);
}

void GLViewer::deleteLastNode(){
  if(cloud_list_indices.size() <= 1){
    this->reset();
    return;
  }
	GLuint nodeId = cloud_list_indices.back();
	cloud_list_indices.pop_back();
	glDeleteLists(nodeId,1);
	GLuint ftId = feature_list_indices.back();
	feature_list_indices.pop_back();
	glDeleteLists(ftId,1);
}

void GLViewer::pointCloud2GLPoints(pointcloud_type * pc){
    ScopedTimer s(__FUNCTION__);
    ROS_DEBUG("Making GL list from point-cloud pointer %p in thread %d", pc, (unsigned int)QThread::currentThreadId());
    GLuint cloud_list_index = glGenLists(1);
    if(!cloud_list_index) {
        ROS_ERROR("No display list could be created");
        return;
    }
    float mesh_thresh = ParameterServer::instance()->get<double>("squared_meshing_threshold");
    cloud_list_indices.push_back(cloud_list_index);
    glNewList(cloud_list_index, GL_COMPILE);
    glBegin(GL_POINTS);
    //ROS_INFO_COND(!pc->is_dense, "Expected dense cloud for opengl drawing");
    point_type origin;
    origin.x = 0;
    origin.y = 0;
    origin.z = 0;

    float depth;
    unsigned int w=pc->width, h=pc->height;
    for(unsigned int x = 0; x < w-1; x++){
        for(unsigned int y = 0; y < h-1; y++){
            using namespace pcl;
            const point_type* p = &pc->points[x+y*w]; //current point
            if(!(validXYZ(*p))) continue;
            setGLColor(*p);
            glVertex3f(p->x, p->y, p->z);
        }
    }
    glEnd();
    ROS_DEBUG("Compiled pointcloud into list %i",  cloud_list_index);
    glEndList();
    Q_EMIT cloudRendered(pc);
}

void GLViewer::pointCloud2GLList(pointcloud_type const * pc){
    ScopedTimer s(__FUNCTION__);
    ROS_DEBUG("Making GL list from point-cloud pointer %p in thread %d", pc, (unsigned int)QThread::currentThreadId());
    GLuint cloud_list_index = glGenLists(1);
    if(!cloud_list_index) {
        ROS_ERROR("No display list could be created");
        return;
    }
    float mesh_thresh = ParameterServer::instance()->get<double>("squared_meshing_threshold");
    cloud_list_indices.push_back(cloud_list_index);
    glNewList(cloud_list_index, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    //ROS_INFO_COND(!pc->is_dense, "Expected dense cloud for opengl drawing");
    point_type origin;
    origin.x = 0;
    origin.y = 0;
    origin.z = 0;

    float depth;
    unsigned int w=pc->width, h=pc->height;
    for(unsigned int x = 0; x < w-1; x++){
        for(unsigned int y = 0; y < h-1; y++){
            using namespace pcl;

            const point_type* pi = &pc->points[x+y*w]; //current point

            if(!(validXYZ(*pi))) continue;
            depth = squaredEuclideanDistance(*pi,origin);

            const point_type* pl = &pc->points[(x+1)+(y+1)*w]; //one right-down
            if(!(validXYZ(*pl)) or squaredEuclideanDistance(*pi,*pl)/depth > mesh_thresh)  
              continue;

            const point_type* pj = &pc->points[(x+1)+y*w]; //one right
            if(validXYZ(*pj)
               and squaredEuclideanDistance(*pi,*pj)/depth <= mesh_thresh  
               and squaredEuclideanDistance(*pj,*pl)/depth <= mesh_thresh){
              drawTriangle(*pi, *pj, *pl);
            }
            const point_type* pk = &pc->points[(x)+(y+1)*w]; //one down
            
            if(validXYZ(*pk)
               and squaredEuclideanDistance(*pi,*pk)/depth <= mesh_thresh  
               and squaredEuclideanDistance(*pk,*pl)/depth <= mesh_thresh){
              drawTriangle(*pi, *pk, *pl);
            }
        }
    }
    glEnd();
    ROS_DEBUG("Compiled pointcloud into list %i",  cloud_list_index);
    glEndList();
}

void GLViewer::reset(){
    if(!cloud_list_indices.empty()){
      unsigned int max= cloud_list_indices.size() > feature_list_indices.size()? cloud_list_indices.back() : feature_list_indices.back();
      glDeleteLists(1,max);
    }
    cloud_list_indices.clear();
    feature_list_indices.clear();
    cloud_matrices->clear();
    edge_list_.clear();
    clearAndUpdate();
}
QImage GLViewer::renderList(QMatrix4x4 transform, int list_id){
    return QImage();
}

void GLViewer::setEdges(const QList<QPair<int, int> >* edge_list){
  //if(edge_list_) delete edge_list_;
  edge_list_ = *edge_list;
}

void GLViewer::drawEdges(){
  if(edge_list_.empty()) return;
  //glEnable (GL_LINE_STIPPLE);
  //glLineStipple (1, 0x1111);
  glBegin(GL_LINES);
  glLineWidth(12);
  for(int i = 0; i < edge_list_.size(); i++){
    int id1 = edge_list_[i].first;
    int id2 = edge_list_[i].second;
    float x,y,z;
    if(cloud_matrices->size() > id1 && cloud_matrices->size() > id2){//only happens in weird circumstances
      if(abs(id1 - id2) == 1){//consecutive
        glColor4f(bg_col_[0],1-bg_col_[1],1-bg_col_[2],1.0); //cyan on black, red on white
      } else if(abs(id1 - id2) > 20){//consider a loop closure
        glColor4f(1-bg_col_[0],1-bg_col_[1],bg_col_[2],0.4); //orange on black, blue on black, transp
      } else { //near predecessor
        glColor4f(1-bg_col_[0],1-bg_col_[1],1-bg_col_[2],0.4); //inverse of bg color, but transp
      }
      x = (*cloud_matrices)[id1](0,3);
      y = (*cloud_matrices)[id1](1,3);
      z = (*cloud_matrices)[id1](2,3);
      glVertex3f(x,y,z);
      x = (*cloud_matrices)[id2](0,3);
      y = (*cloud_matrices)[id2](1,3);
      z = (*cloud_matrices)[id2](2,3);
      glVertex3f(x,y,z);
    }
    //This happens if the edges are updated faster than the poses/clouds
    else ROS_WARN("Not enough cloud matrices (%d) for vertex ids (%d and %d)", cloud_matrices->size(), id1, id2);
  }
  glEnd();
  //glDisable (GL_LINE_STIPPLE);

  if(show_tfs_){
    glDisable(GL_DEPTH_TEST);
    glColor4f(1-bg_col_[0],1-bg_col_[1],1-bg_col_[2],1.0); //inverse of bg color, non transp
    for(int i = 0; i < edge_list_.size(); i++){
      int id1 = edge_list_[i].first;
      int id2 = edge_list_[i].second;
      if(cloud_matrices->size() > id1 && cloud_matrices->size() > id2){//only happens in weird circumstances
        Eigen::Vector3f x1((*cloud_matrices)[id1](0,3), (*cloud_matrices)[id1](1,3), (*cloud_matrices)[id1](2,3));
        Eigen::Vector3f x2((*cloud_matrices)[id2](0,3), (*cloud_matrices)[id2](1,3), (*cloud_matrices)[id2](2,3));
        Eigen::Vector3f dx = x2 - x1;
        Eigen::Vector3f xm = x1 + 0.5*dx;

        QString edge_info;
        edge_info.sprintf("%d-%d:\n(%.2f, %.2f, %.2f)", id1, id2, dx(0), dx(1), dx(2));
        this->renderText(xm(0), xm(1), xm(2), edge_info, QFont("Monospace", 6));
      }
      //This happens if the edges are updated faster than the poses/clouds
      else ROS_WARN("Not enough cloud matrices (%d) for vertex ids (%d and %d)", cloud_matrices->size(), id1, id2);
    }
    glEnable(GL_DEPTH_TEST);
  }
}


void GLViewer::setViewPoint(QMatrix4x4 new_vp){
    ///Moving the camera is inverse to moving the points to draw
    viewpoint_tf_ = new_vp.inverted();
}

bool GLViewer::setClickedPosition(int x, int y) {
    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];
    GLfloat winX, winY, winZ;
    GLdouble posX, posY, posZ;

    glGetDoublev( GL_MODELVIEW_MATRIX, modelview );
    glGetDoublev( GL_PROJECTION_MATRIX, projection );
    glGetIntegerv( GL_VIEWPORT, viewport );

    winX = (float)x;
    winY = (float)viewport[3] - (float)y;
    glReadPixels( x, int(winY), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ );
    if(winZ != 1){ //default value, where nothing was rendered
      gluUnProject( winX, winY, winZ, modelview, projection, viewport, &posX, &posY, &posZ);
      ROS_DEBUG_STREAM((float)winZ << ", " << posX << "," << posY << "," << posZ);
      viewpoint_tf_(0,3) = -posX;
      viewpoint_tf_(1,3) = -posY;
      viewpoint_tf_(2,3) = -posZ;
      return true;
    } else {
      return false;
    }
}

void GLViewer::toggleTriangulation() {
    ROS_INFO("Toggling Triangulation");
    if(polygon_mode == GL_FILL){ // Turn on Pointcloud mode
        polygon_mode = GL_POINT;
    //Wireframe mode is Slooow
    //} else if(polygon_mode == GL_POINT){ // Turn on Wireframe mode
        //polygon_mode = GL_LINE;
    } else { // Turn on Surface mode
        polygon_mode = GL_FILL;
    }
    glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);
    clearAndUpdate();
}

void GLViewer::drawToPS(QString filename){
#ifdef GL2PS

  FILE *fp = fopen(qPrintable(filename), "wb");
  if(!fp) ROS_ERROR("Could not open file %s", qPrintable(filename));
  GLint buffsize = 0, state = GL2PS_OVERFLOW;
  GLint viewport[4];
  char *oldlocale = setlocale(LC_NUMERIC, "C");


  glGetIntegerv(GL_VIEWPORT, viewport);

  while( state == GL2PS_OVERFLOW ){
    buffsize += 1024*1024;
    gl2psBeginPage ( "GL View", "RGBD-SLAM", viewport,
                     GL2PS_PDF, GL2PS_BSP_SORT, GL2PS_SILENT |
                     GL2PS_SIMPLE_LINE_OFFSET | GL2PS_NO_BLENDING |
                     GL2PS_OCCLUSION_CULL | GL2PS_BEST_ROOT,
                     GL_RGBA, 0, NULL, 0, 0, 0, buffsize,
                     fp, "LatexFile" );
    drawClouds(0.0);
    state = gl2psEndPage();
  }
  setlocale(LC_NUMERIC, oldlocale);
  fclose(fp);
#else
  QMessageBox::warning(this, tr("This functionality is not supported."), tr("This feature needs to be compiled in. To do so, install libgl2ps-dev and set the USE_GL2PS flag at the top of CMakeLists.txt. The feature has been removed to ease the installation process, sorry for the inconvenience."));
#endif
}


inline void GLViewer::clearAndUpdate(){
  updateGL();
}
