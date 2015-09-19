#include <cassert>
#include <cmath>
#include <Eigen/Core>
#include <Recognition/Model.h>
#include <Camera/CameraModel.h>
#include <boost/filesystem.hpp>
#include <highgui.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/rgbd.hpp>
#include <opencv2/core/eigen.hpp>
#include <C5G/Pose.h>
#include <Recognition/DetectorWMasks.h>
#include <Recognition/Utils.h>

namespace Recognition{
  Model::Model(const std::string& id, const boost::filesystem::path& trainDir)
    :
      _myId(id),
      _camModel(1,1,1,1,1,1,1,1,1,1,1,1,1),
      _detector(new cv::linemod::DetectorWMasks(*cv::linemod::getDefaultLINEMOD()))
  {
    readFrom(id, trainDir);
  }

  Model::Model(const std::string& id, const std::string& meshFile, const Camera::CameraModel& cam)
    :
      _myId(id),
      mesh_file_path(meshFile),
      _detector(new cv::linemod::DetectorWMasks(*cv::linemod::getDefaultLINEMOD())),
      _camModel(cam),
      renderer_width(cam.getWidth()),
      renderer_height(cam.getHeight()),
      renderer_focal_length_x(cam.getFx()),
      renderer_focal_length_y(cam.getFy()),
      renderer_near(0.1),
      renderer_far(10),
      _renderer(new Renderer3d(meshFile))
  {
    _renderer->set_parameters(renderer_width, renderer_height, renderer_focal_length_x, renderer_focal_length_y, renderer_near, renderer_far);
  }


  void Model::readFrom(const std::string& id, const boost::filesystem::path& trainDir)
  {
    namespace fs=boost::filesystem;

    using fs::path;
    _myId=id;
    /* Load the yml of that class obtained in the Training phase */
    fs::path lmSaveFile = trainDir / path(_myId) / fs::path(_myId+"_Linemod.yml");

    cv::FileStorage inFile(lmSaveFile.string(), cv::FileStorage::READ);
    inFile["mesh_file_path"] >> mesh_file_path;
    _detector->read(inFile.root());
    std::cout<<"\tNumber of templates:"<<_detector->numTemplates()<<"\n";


    /** Read the trained class ID for this object */
    cv::FileNode fn = inFile["object"];
    fn["class_id"] >> _myId;
    _detector->readClass(fn);

    /** Initialize the renderer with the same parameters used for learning */
    _renderer=std::shared_ptr<Renderer3d>(new Renderer3d(mesh_file_path));

    cv::FileNode fr = inFile["rendering"];
    fr["renderer_width"] >> renderer_width;
    fr["renderer_height"] >> renderer_height;
    fr["renderer_focal_length_x"] >> renderer_focal_length_x;
    fr["renderer_focal_length_y"] >> renderer_focal_length_y;
    fr["renderer_near"] >> renderer_near;
    fr["renderer_far"] >> renderer_far;
    _camModel=Camera::CameraModel(renderer_width, renderer_height, renderer_focal_length_x, renderer_focal_length_y, 0, renderer_height/2.0, renderer_width/2.0);
    _renderer->set_parameters(renderer_width, renderer_height, renderer_focal_length_x, renderer_focal_length_y, renderer_near, renderer_far);

    _myData={};
    const cv::FileNode& n=inFile["trainData"];
    /** Reads the camera models: can't use readSequence! */
    assert(n.type() == cv::FileNode::SEQ && "Data are not a sequence!");
    for(const auto& it : n){
      int tID;
      it["id"] >> tID;
      TrainingData t{{},{},{},{},{},{0,0,0,0,0,0,0},{}};
      read(it["data"], t, {{},{},{},{},{},{0,0,0,0,0,0,0},{}});
      _myData.insert(std::make_pair(tID,t));
    }
  }

  const std::vector<cv::linemod::Template> Model::getTemplates(int templateID) const {
    return _detector->getTemplates(_myId, templateID);
  }

  void Model::addAllTemplates(cv::linemod::DetectorWMasks& det) const {
    for(int i=0; i<_detector->numTemplates(); ++i){
      auto& t=getTemplates(i);
      det.addSyntheticTemplate(t, _myId);
    }
  }

  int Model::numTemplates() const {
    return _detector->numTemplates();
  }
  cv::Matx33d Model::getR(int templateID) const {
    return _myData.at(templateID).R;
  }
  /*cv::Vec3f Model::getT(int templateID) const {
    return _myData.at(templateID).T;
  }*/
  float Model::getA(int templateID) const {
    return _myData.at(templateID).a;
  }
  float Model::getB(int templateID) const {
    return _myData.at(templateID).b;
  }
  float Model::getG(int templateID) const {
    return _myData.at(templateID).g;
  }
  float Model::getDist(int templateID) const {
    return _myData.at(templateID).dist;
  }
  cv::Matx33f Model::getK(int templateID) const {
    return _myData.at(templateID).cam.getIntrinsic();
  }
  Camera::CameraModel Model::getCam(int templateID) const {
    return _myData.at(templateID).cam;
  }
  cv::Mat Model::getHueHist(int templateID) const {
    return _myData.at(templateID).hueHist;
  }
  Model::TrainingData Model::getData(int templateID) const {
    return _myData.at(templateID);
  }

  void Model::render(cv::Vec3d T, cv::Vec3d up, cv::Mat &image_out, cv::Mat &depth_out, cv::Mat &mask_out, cv::Rect &rect_out) const {
    renderDepthOnly(T, up, depth_out, mask_out, rect_out);
    renderImageOnly(T, up, image_out, rect_out);
  }

  void Model::renderImageOnly(cv::Vec3d T, cv::Vec3d up, cv::Mat &image_out, cv::Rect &rect_out) const {
    _renderer->lookAt(T(0), T(1), T(2), up(0), up(1), up(2));
    _renderer->renderImageOnly(image_out, rect_out);
  }

  void Model::renderDepthOnly(cv::Vec3d T, cv::Vec3d up, cv::Mat &depth_out, cv::Mat &mask_out, cv::Rect &rect_out) const {
    _renderer->lookAt(T(0), T(1), T(2), up(0), up(1), up(2));
    _renderer->renderDepthOnly(depth_out, mask_out, rect_out);
  }

  //TODO pcl::PointCloud<pcl::PointXYZRGB> Model::getPointCloud(cv::Vec3d T, cv::Vec3d up, const Camera::CameraModel& cam ){
  //TODO   cv::Matrix
  //TODO }

  const std::shared_ptr<Renderer3d> Model::getRenderer() const {
    return _renderer;
  }

  cv::Matx33d Model::camTUp2ObjRot(const cv::Vec3d& tDir, const cv::Vec3d& upDir){
    cv::Vec3d t=tDir, up=upDir;
    normalize_vector(t(0),t(1),t(2));

    // compute the left vector
    cv::Vec3d y;
    y = up.cross(t);
    normalize_vector(y(0),y(1),y(2));

    // re-compute the orthonormal up vector
    up = t.cross(y);
    normalize_vector(up(0), up(1), up(2));

    cv::Mat R_full = (cv::Mat_<double>(3, 3) <<
        -y(0), -y(1), -y(2),
        -up(0), -up(1), -up(2),
        t(0), t(1), t(2)
        );

    cv::Matx33d R = R_full;
    R = R.t();

    return R.inv();
  }

  void Model::addTraining(const Eigen::Matrix3d& rot, double distance, const Camera::CameraModel& cam){

    Eigen::Affine3d transformation = Eigen::Affine3d::Identity();
    transformation.translation() << 0, 0, distance;
    transformation.linear()=rot;

    cv::Rect rect;
    cv::Mat image, depth, mask;
    /** Performs a render of the object at the desired camera's position and adds it to the trained templates */
    render(transformation, image, depth, mask, rect);
    if(image.empty())
    {
      /** Nothing to be done, this template is completely unuseful as the object can't be seen from this position */
      return;
    }
      
    // Create a structuring element
    constexpr int erosion_size = 3;  
    cv::Mat erosion_element = cv::getStructuringElement(cv::MORPH_CROSS,
            cv::Size(2 * erosion_size + 1, 2 * erosion_size + 1),
            cv::Point(erosion_size, erosion_size) );
    cv::Mat eroded_mask, contour;
    // Apply erosion or dilation on the image
    cv::erode(mask,eroded_mask,erosion_element);
    contour=mask&(~eroded_mask);

    std::vector<cv::Mat> sources(2), trainMasks(2);
    sources[0] = image;
    sources[1] = depth;
    trainMasks[0]=contour;
    trainMasks[1]=eroded_mask;
    assert(image.type()==CV_8UC3);
    assert(depth.type()==CV_16UC1);
    int template_in = _detector->addTemplate(sources, _myId, trainMasks);
    if (template_in == -1)
    {
      /** Nothing to be done, this template is completely unuseful as the object can't be seen from this position */
      return;
    }

    /** hue histogram */
    /* Convert to HSV */
    cv::Mat R;
    eigen2cv(rot, R);
    cv::Mat hsv_mat;
    cv::cvtColor(image, hsv_mat, CV_BGR2HSV);
    std::vector<cv::Mat> hsv_planes;
    cv::split( hsv_mat, hsv_planes );
    /* Compute histogram over 30 bins */
    int Hbins = 30;
    int histSize = Hbins; /* 1D histogram */
    /* Range over which to compute the histogram */
#if 0
    const float histRange[] = { 0, 180 };
    const float* histRangePtr = histRange;
    cv::Mat Hue_Hist;
    /* Compute and normalize hist only on the mask */
    cv::calcHist( &hsv_planes[0], 1, 0, mask, Hue_Hist, 1, &histSize, &histRangePtr, true, false );
    cv::normalize(Hue_Hist, Hue_Hist, 0.0, 1.0, cv::NORM_MINMAX, -1, cv::Mat() );
#endif

    /** Save the computed data */
    if(_myData.find(template_in)!=_myData.end()){
      std::cerr << "##############Duplicate template ID!!################\n";
    }
    _myData.insert(std::make_pair(template_in,TrainingData{R, distance, 0, 0, 0, cam, cv::Mat()}));
  }

  void Model::addTraining(const double distance, const double alpha, const double beta, const double gamma, const Camera::CameraModel& cam){

    /** Apply Euler angle rotations */
    Eigen::Affine3d rot = Eigen::Affine3d::Identity();
    rot.rotate (Eigen::AngleAxisd (alpha, Eigen::Vector3d::UnitX()));
    rot.rotate (Eigen::AngleAxisd (beta, Eigen::Vector3d::UnitY()));
    rot.rotate (Eigen::AngleAxisd (gamma, Eigen::Vector3d::UnitZ()));
    addTraining(rot.rotation().matrix(), distance, cam);
  }

  void Model::saveToDirectory(const boost::filesystem::path& saveDir) const
  {
    using boost::filesystem::path;
    assert(is_directory(saveDir) && "no valid directory provided");
    path filename=saveDir / path(_myId) / path(_myId+"_Linemod.yml");

    cv::FileStorage fs(filename.string(), cv::FileStorage::WRITE);

    fs << "mesh_file_path" << mesh_file_path ;

    _detector->write(fs);

    assert(_detector->classIds().size()==1 && "Multiple classes into the same model");
    fs << "object" ;
    fs << "{";
    _detector->writeClass(_myId, fs);
    fs << "}";

    //save : R, T, dist, and Ks for that class
    int nData=_myData.size();
    fs << "trainData" << "[";
    for (auto& item : _myData)
    {
      fs << "{";
      fs << "id" << item.first;
      fs << "data" << item.second;
      fs << "}";
    }
    fs << "]";

    fs << "rendering"  << "{";
    fs << "renderer_width" <<  renderer_width;
    fs << "renderer_height" <<  renderer_height;
    fs << "renderer_focal_length_x" <<  renderer_focal_length_x;
    fs << "renderer_focal_length_y" <<  renderer_focal_length_y;
    fs << "renderer_near" <<  renderer_near;
    fs << "renderer_far" <<  renderer_far;
    fs << "renderer_n_points" <<  renderer_n_points;
    fs << "}";

  }

  void Model::render(const Eigen::Affine3f& pose, cv::Mat& rgb_out, cv::Mat& depth_out, cv::Mat& mask_out, cv::Rect& rect_out) const {
    render(Eigen::Affine3d(pose), rgb_out, depth_out, mask_out, rect_out);
  }

  void Model::render(const Eigen::Affine3d& pose, cv::Mat& rgb_out, cv::Mat& depth_out, cv::Mat& mask_out, cv::Rect& rect_out) const {
    constexpr double PI  =3.141592653589793238463;
    auto newPose=Eigen::AngleAxisd(PI, Eigen::Vector3d::UnitX())*pose;

    _renderer->setObjectPose(newPose);
    _renderer->renderDepthOnly(depth_out, mask_out, rect_out);
    _renderer->renderImageOnly(rgb_out, rect_out);
  }

  void Model::render(const C5G::Pose& pose, cv::Mat& rgb_out, cv::Mat& depth_out, cv::Mat& mask_out, cv::Rect& rect_out) const {
    /** Gets the T and UP vector from the Pose object */
    render(C5G::Pose::poseToTransform(pose), rgb_out, depth_out, mask_out, rect_out);
  }

  pcl::PointCloud<pcl::PointXYZRGB>::Ptr Model::getPointCloud(const C5G::Pose& pose) const {
    cv::Mat image_out, depth_out, mask_out;
    cv::Rect rect_out;

    /** Gets the T and UP vector from the Pose object */
    Eigen::Affine3d objTransformation=C5G::Pose::poseToTransform(pose);
    cv::Mat t;
    eigen2cv(Eigen::Vector3d(objTransformation.translation()), t);
    Eigen::Matrix3d R_temp(objTransformation.rotation());
    R_temp=R_temp.inverse();
    cv::Vec3d u (-R_temp(0,1), -R_temp(1,1), -R_temp(2,1));
    std::cout << "Up vector: " << u(0) << ","<< u(1) << ","<< u(2) << "\n";

    render(t, u, image_out, depth_out, mask_out, rect_out);

    cv::Mat_<cv::Vec3f> pointsXYZ;
    cv::rgbd::depthTo3d(depth_out, _camModel.getIntrinsic(), pointsXYZ);
    /** Fills model and reference pointClouds with points taken from (X,Y,Z) coordinates */
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr modelCloudPtr (new pcl::PointCloud<pcl::PointXYZRGB>);

    /** Model PointCloud*/
    int mySize = pointsXYZ.rows*pointsXYZ.cols;
    modelCloudPtr->resize(mySize);
    modelCloudPtr->height = 1;
    modelCloudPtr->is_dense = true;

    for(int i=0; i<pointsXYZ.rows; ++i)
    {
      for(int j=0; j<pointsXYZ.cols; ++j){
        modelCloudPtr->points[i*pointsXYZ.cols+j].x=pointsXYZ[i][j][0];
        modelCloudPtr->points[i*pointsXYZ.cols+j].y=pointsXYZ[i][j][1];
        modelCloudPtr->points[i*pointsXYZ.cols+j].z=pointsXYZ[i][j][2];
        modelCloudPtr->points[i*pointsXYZ.cols+j].r=0;
        modelCloudPtr->points[i*pointsXYZ.cols+j].g=255;
        modelCloudPtr->points[i*pointsXYZ.cols+j].b=0;
      }
    }
    return modelCloudPtr;
  }
}
namespace cv{
  void write( FileStorage& fs, const std::string& name, const Recognition::Model::TrainingData& data){
    /* Read YAML Vector */
    fs << "{";
    fs << "R" << cv::Mat(data.R);
    fs << "dist" << data.dist;
    fs << "a" << data.a;
    fs << "b" << data.b;
    fs << "g" << data.g;
    fs << "cam" << data.cam;
    fs << "hueHist" << data.hueHist;
    fs << "}";
  }
  void read(const FileNode& node, Recognition::Model::TrainingData& x, const Recognition::Model::TrainingData& default_value){
    if(node.empty()){
      x=default_value;
      return;
    }

    /* Read YAML Vector */
    cv::Mat R;
    cv::Vec3d T;
    float dist,a,b,g;
    cv::Mat hueHist;

    node["R"] >> R;
    node["T"] >> T;
    node["dist"] >> dist;
    node["a"] >> a;
    node["b"] >> b;
    node["g"] >> g;
    node["hueHist"] >> hueHist;
    x={R,dist,a,b,g,Camera::CameraModel::readFrom(node["cam"]), hueHist};
  }
}
