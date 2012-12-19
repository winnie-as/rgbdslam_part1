#include "gm2dl_io.h"

#include "data_queue.h"

#include "g2o/core/graph_optimizer_sparse.h"
#include "g2o/core/factory.h"

#include "g2o/types/sclam2d/edge_se2_sensor_calib.h"
#include "g2o/types/data/robot_laser.h"

#include "g2o/stuff/string_tools.h"

#include <fstream>
#include <iostream>
using namespace std;

namespace g2o {

  const int Gm2dlIO::ID_LASERPOSE  = numeric_limits<int>::max();
  const int Gm2dlIO::ID_ODOMCALIB  = numeric_limits<int>::max() - 1;

  bool Gm2dlIO::readGm2dl(const std::string& filename, SparseOptimizer& optimizer, bool overrideCovariances)
  {
    optimizer.clear();
    ifstream is(filename.c_str());
    if (! is.good())
      return false;

    bool laserOffsetInitDone = false;
    VertexSE2* laserOffset = new VertexSE2;
    //laserOffset->fixed() = true;
    laserOffset->setId(ID_LASERPOSE);
    if (! optimizer.addVertex(laserOffset)) {
      cerr << "Unable to add laser offset" << endl;
      return false;
    }

    // parse the GM2DL file an extract the vertices, edges, and the laser data
    stringstream currentLine;
    VertexSE2* previousVertex = 0;
    while(1) {
      int bytesRead = readLine(is, currentLine);
      if (bytesRead == -1)
        break;
      string tag;
      currentLine >> tag;
      if (tag == "VERTEX" || tag == "VERTEX2"){
        int id;
        Eigen::Vector3d p;
        currentLine >> id >> p.x() >> p.y() >> p.z();
        // adding the robot pose
        VertexSE2* v = new VertexSE2;
        v->setId(id);
        //cerr << "Read vertex id " << id << endl;
        if (! optimizer.addVertex(v)) {
          cerr << "vertex " << id << " is already in the graph, reassigning "<<  endl;
          delete v;
          v = dynamic_cast<VertexSE2*>(optimizer.vertex(id));
          assert(v);
        } 
        v->setEstimate(p);
        previousVertex = v;

      } else if (tag == "EDGE" || tag == "EDGE2"){
        if (! laserOffsetInitDone) {
          cerr << "Error: need laser offset" << endl;
          return false;
        }
        int id1, id2;
        EdgeSE2SensorCalib* e = new EdgeSE2SensorCalib;
        Eigen::Vector3d p;
        Eigen::Matrix3d& m = e->information();
        currentLine >> id1 >> id2 >> p.x() >> p.y() >> p.z();
        if (overrideCovariances){
          m = Eigen::Matrix3d::Identity();
        } else {
          currentLine >> m(0, 0) >> m(0, 1) >> m(1, 1) >> m(2, 2) >> m(0, 2) >> m(1, 2);
          m(1, 0) = m(0, 1);
          m(2, 0) = m(0, 2);
          m(2, 1) = m(1, 2);
        }
        previousVertex = 0;
        VertexSE2* v1 = dynamic_cast<VertexSE2*>(optimizer.vertex(id1));
        VertexSE2* v2 = dynamic_cast<VertexSE2*>(optimizer.vertex(id2));
        if (! v1) {
          cerr << "vertex " << id1 << " is not existing, cannot add edge (" << id1 << "," << id2 << ")" << endl; 
          delete e;
          continue;
        }
        if (! v2) {
          cerr << "vertex " << id2 << " is not existing, cannot add edge (" << id1 << "," << id2 << ")" << endl; 
          delete e;
          continue;
        }

        //if (0)
        //if (abs(id1 - id2) != 1)
          //m *= 1e-6;

        // TODO transform measurement covariance by considering the laserOffset to measurement between the lasers
        SE2 transf; transf.fromVector(p);
        e->setMeasurement(laserOffset->estimate().inverse() * transf * laserOffset->estimate());
        //e->inverseMeasurement() = e->measurement().inverse();

        e->vertices()[0] = v1;
        e->vertices()[1] = v2;
        e->vertices()[2] = laserOffset;
        if (! optimizer.addEdge(e)){
          cerr << "error in adding edge " << id1 << "," << id2 << endl;
          delete e;
        }
        //cerr << PVAR(e->inverseMeasurement().toVector().transpose()) << endl;
        //cerr << PVAR(e->information()) << endl;

      } else if (tag == "ROBOTLASER1") {
        if (previousVertex) {
          RobotLaser* rl2 = new RobotLaser;
          rl2->read(currentLine);
          if (! laserOffsetInitDone) {
            laserOffsetInitDone = true;
            //cerr << "g2o Laseroffset is " << rl2->laserParams().laserPose.toVector().transpose() << endl;
            laserOffset->setEstimate(rl2->laserParams().laserPose);
          }
          previousVertex->setUserData(rl2);
          previousVertex = 0;
        }
      }
    }

    return true;
  }

  bool Gm2dlIO::writeGm2dl(const std::string& filename, const SparseOptimizer& optimizer)
  {
    ofstream fout(filename.c_str());
    if (! fout.good()) {
      return false;
    }
    Factory* factory = Factory::instance();

    for (SparseOptimizer::VertexIDMap::const_iterator it = optimizer.vertices().begin(); it != optimizer.vertices().end(); ++it) {
      OptimizableGraph::Vertex* v = static_cast<OptimizableGraph::Vertex*>(it->second);
      fout << "VERTEX2 " << v->id() << " ";
      v->write(fout);
      fout << endl;
      OptimizableGraph::Data* data = v->userData();
      if (data) { // writing the data via the factory
        string tag = factory->tag(data);
        if (tag.size() > 0) {
          fout << tag << " ";
          data->write(fout);
          fout << endl;
        }
      }
    }

    OptimizableGraph::EdgeContainer edgesToSave; // sorting edges to have them in the order of insertion again
    for (HyperGraph::EdgeSet::const_iterator it = optimizer.edges().begin(); it != optimizer.edges().end(); ++it) {
      const OptimizableGraph::Edge* e = static_cast<const OptimizableGraph::Edge*>(*it);
      edgesToSave.push_back(const_cast<OptimizableGraph::Edge*>(e));
    }
    sort(edgesToSave.begin(), edgesToSave.end(), OptimizableGraph::EdgeIDCompare());

    for (OptimizableGraph::EdgeContainer::const_iterator it = edgesToSave.begin(); it != edgesToSave.end(); ++it) {
      OptimizableGraph::Edge* e = *it;
      EdgeSE2SensorCalib* calibEdge = dynamic_cast<EdgeSE2SensorCalib*>(e);
      if (calibEdge) {
        // write back in the gm2dl format
        fout << "EDGE2 " << calibEdge->vertices()[0]->id() << " " << calibEdge->vertices()[1]->id();
        Eigen::Vector3d meas = calibEdge->measurement().toVector();
        fout << " " << meas.x() << " " << meas.y() << " " << meas.z();
        const Eigen::Matrix3d& m = calibEdge->information();
        fout << " " << m(0, 0) << " " <<  m(0, 1) << " " << m(1, 1) << " "
          << m(2, 2) << " " << m(0, 2) << " " << m(1, 2);
        fout << endl;
      } else {
        //cerr << "Strange Edge Type: " << factory->tag(e) << endl;
      }
    }

    return fout.good();
  }

  bool Gm2dlIO::updateLaserData(SparseOptimizer& optimizer)
  {
    VertexSE2* laserOffset = dynamic_cast<VertexSE2*>(optimizer.vertex(ID_LASERPOSE));
    if (! laserOffset) {
      cerr << "Laser offset not found" << endl;
      return false;
    }

    for (SparseOptimizer::VertexIDMap::const_iterator it = optimizer.vertices().begin(); it != optimizer.vertices().end(); ++it) {
      VertexSE2* v = dynamic_cast<VertexSE2*>(it->second);
      if (! v)
        continue;
      if (v->id() == ID_LASERPOSE)
        continue;
      RobotLaser* robotLaser = dynamic_cast<RobotLaser*>(v->userData());
      if (robotLaser) { // writing the data via the factory
        robotLaser->setOdomPose(v->estimate());
        robotLaser->laserParams().laserPose = laserOffset->estimate();
      }
    }
    return true;
  }

  int Gm2dlIO::readRobotLaser(const std::string& filename, DataQueue& queue)
  {
    ifstream is(filename.c_str());
    if (! is.good())
      return false;

    int cnt = 0;

    // parse the GM2DL file an extract the vertices, edges, and the laser data
    stringstream currentLine;
    while(1) {
      int bytesRead = readLine(is, currentLine);
      if (bytesRead == -1)
        break;
      string tag;
      currentLine >> tag;
      if (tag == "ROBOTLASER1") {
        RobotLaser* rl2 = new RobotLaser;
        rl2->read(currentLine);
        queue.add(rl2);
        cnt++;
      }
    }
    return cnt;
  }

} // end namespace
