/*
 * Copyright 2014 Aldebaran
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <Recognition/linemod_icp.h>
#include <iostream>

/** get 3D points out of the image */
float matToVec(const cv::Mat_<cv::Vec3f> &src_ref, const cv::Mat_<cv::Vec3f> &src_mod, std::vector<cv::Vec3f>& pts_ref, std::vector<cv::Vec3f>& pts_mod)
{
  pts_ref.clear();
  pts_mod.clear();
  int px_missing = 0;

  cv::MatConstIterator_<cv::Vec3f> it_ref = src_ref.begin();
  cv::MatConstIterator_<cv::Vec3f> it_mod = src_mod.begin();
  for (; it_ref != src_ref.end(); ++it_ref, ++it_mod)
  {
    if (!cv::checkRange(*it_ref))
      continue;

    pts_ref.push_back(*it_ref);
    if (cv::checkRange(*it_mod))
    {
      pts_mod.push_back(*it_mod);
    }
    else
    {
      pts_mod.push_back(cv::Vec3f(0.0f, 0.0f, 0.0f));
      ++px_missing;
    }
  }

  float ratio = 0.0f;
  if ((src_ref.cols > 0) && (src_ref.rows > 0))
    ratio = float(px_missing) / float(src_ref.cols * src_ref.rows);
  return ratio;
}

/** Computes the centroid of 3D points */
void getMean(const std::vector<cv::Vec3f> &pts, cv::Vec3f& centroid)
{
  centroid = cv::Vec3f(0.0f, 0.0f, 0.0f);
  size_t n_points = 0;
  for (std::vector<cv::Vec3f>::const_iterator it = pts.begin(); it != pts.end(); ++it) {
    if (!cv::checkRange(*it))
      continue;
    centroid += (*it);
    ++n_points;
  }

  if (n_points > 0)
  {
    centroid(0) /= float(n_points);
    centroid(1) /= float(n_points);
    centroid(2) /= float(n_points);
  }
}

/** Transforms the point cloud using the rotation and translation */
void transformPoints(const std::vector<cv::Vec3f> &src, std::vector<cv::Vec3f>& dst, const cv::Matx33f &R, const cv::Vec3f &T)
{
  std::vector<cv::Vec3f>::const_iterator it_src = src.begin();
  std::vector<cv::Vec3f>::iterator it_dst = dst.begin();
  for (; it_src != src.end(); ++it_src, ++it_dst) {
    if (!cv::checkRange(*it_src))
      continue;
    (*it_dst) = R * (*it_src) + T;
  }
}

/** Computes the L2 distance between two vectors of 3D points of the same size */
float getL2distClouds(const std::vector<cv::Vec3f> &model, const std::vector<cv::Vec3f> &ref, float &dist_mean, const float mode)
{
  int nbr_inliers = 0;
  int counter = 0;
  float ratio_inliers = 0.0f;
  //std::cout<<"@L2 dist_mean: "<<dist_mean<<"\n";
  float dist_expected = dist_mean * 3.0f;//0.015f;//
  //std::cout<<"@L2 dist_expected: "<<dist_expected<<"\n";
  dist_mean = 0.0f;

   //std::cout<<"@L2 dist_mean: "<<dist_mean<<"\n";
  //use the whole region
  std::vector<cv::Vec3f>::const_iterator it_match = model.begin();
  std::vector<cv::Vec3f>::const_iterator it_ref = ref.begin();
  for(; it_match != model.end(); ++it_match, ++it_ref)
  {
    if (!cv::checkRange(*it_ref))
      continue;

    if (cv::checkRange(*it_match))
    {
      float dist = cv::norm(*it_match - *it_ref);
      //std::cout<<"@L2 dist||match-ref||2: "<<dist<<"\n";
      if ((dist < dist_expected) || (mode == 0))
        dist_mean += dist;
      if (dist < dist_expected)
        ++nbr_inliers;
    }
    ++counter;
    
  }

   std::cout<<"@L2 dist_mean: "<<dist_mean<<"\n";
   std::cout<<"@L2 float(nbr_inliers): "<<float(nbr_inliers)<<"\n";
  if (counter > 0 )//&& nbr_inliers > 0)
  {
    dist_mean /= float(nbr_inliers);
    //std::cout<<"@L2 dist_mean: "<<dist_mean<<"\n";
    //std::cout<<"@L2 float(nbr_inliers): "<<float(nbr_inliers)<<"\n";
    ratio_inliers = float(nbr_inliers) / float(counter);
  }
  else
    dist_mean = std::numeric_limits<float>::max();

  return ratio_inliers;
}

/** Refine the object pose by icp (Iterative Closest Point) alignment of two vectors of 3D points.*/
float icpCloudToCloud(const std::vector<cv::Vec3f> &pts_ref, std::vector<cv::Vec3f> &pts_model, cv::Matx33f& R, cv::Vec3f& T, float &px_inliers_ratio, int mode)
{
  //optimal rotation matrix
  cv::Matx33f R_optimal;
  //optimal transformation
  cv::Vec3f T_optimal;

  //the number of desired iterations defined depending on the mode
  int icp_it_th = 70;//35; //maximal number of iterations
  if (mode == 1)
    icp_it_th = 35;//4 //minimal number of iterations
  else if (mode == 2)
    icp_it_th = 35;//4

  //desired distance between two point clouds
  const float dist_th = 0.012f;
  //The mean distance between the reference and the model point clouds
  float dist_mean = 0.0f;
  px_inliers_ratio = getL2distClouds(pts_model, pts_ref, dist_mean, mode);
  //The difference between two previously obtained mean distances between the reference and the model point clouds
  float dist_diff = std::numeric_limits<float>::max();
//  std::cout<<"@ICP 1o L2--------"<<"\n";
//   std::cout<<"dist_mean: "<<dist_mean<<"\n";
//   std::cout<<"dist_diff: "<<dist_diff<<"\n";  
  //the number of performed iterations
  int iter = 0;
  while (( ((dist_mean > dist_th) && (dist_diff > 0.0001f)) || (mode == 1) ) && (iter < icp_it_th))
  {
    ++iter;

    //subsample points from the match and ref clouds
    if (pts_model.empty() || pts_ref.empty())
      continue;

    //compute centroids of each point subset
    cv::Vec3f m_centroid, r_centroid;
    getMean(pts_model, m_centroid);
    getMean(pts_ref, r_centroid);

    //compute the covariance matrix
    cv::Matx33f covariance (0,0,0, 0,0,0, 0,0,0);
    std::vector<cv::Vec3f>::iterator it_s = pts_model.begin();
    std::vector<cv::Vec3f>::const_iterator it_ref = pts_ref.begin();
    for (; it_s < pts_model.end(); ++it_s, ++it_ref)
      covariance += (*it_s) * (*it_ref).t();

    cv::Mat w, u, vt;
    cv::SVD::compute(covariance, w, u, vt);
    //compute the optimal rotation
    R_optimal = cv::Mat(vt.t() * u.t());

    //compute the optimal translation
    T_optimal = r_centroid - R_optimal * m_centroid;
    if (!cv::checkRange(R_optimal) || !cv::checkRange(T_optimal))
      continue;

    //transform the point cloud
    transformPoints(pts_model, pts_model, R_optimal, T_optimal);

    //compute the distance between the transformed and ref point clouds
    dist_diff = dist_mean;
//      std::cout<<"@ICP 2 Before L2--------"<<"\n";
//      std::cout<<"dist_mean: "<<dist_mean<<"\n";
//      std::cout<<"dist_diff: "<<dist_diff<<"\n";  
    px_inliers_ratio = getL2distClouds(pts_model, pts_ref, dist_mean, mode);
//      std::cout<<"@ICP 3 After L2--------"<<"\n";
//      std::cout<<"dist_mean: "<<dist_mean<<"\n";
//      std::cout<<"dist_diff: "<<dist_diff<<"\n";  
    dist_diff -= dist_mean;
//      std::cout<<"@ICP 4--------"<<"\n";
//      std::cout<<"dist_mean: "<<dist_mean<<"\n";
//      std::cout<<"dist_diff: "<<dist_diff<<"\n";  

    //update the translation matrix: turn to opposite direction at first and then do translation
    T = R_optimal * T;
    //do translation
    cv::add(T, T_optimal, T);
    //update the rotation matrix
    R = R_optimal * R;
    std::cout << " @ICP it " << iter << "/" << icp_it_th << " : " << std::fixed << dist_mean << " " << dist_diff << " " << px_inliers_ratio << " " << pts_model.size() << std::endl;
  }

    //std::cout << " icp " << mode << " " << dist_min << " " << iter << "/" << icp_it_th  << " " << px_inliers_ratio << " " << d_diff << " " << std::endl;
  return dist_mean;
}
