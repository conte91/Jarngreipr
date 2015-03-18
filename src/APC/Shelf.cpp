#include <C5G/Pose.h>
#include <APC/Shelf.h>

namespace APC {

  using C5G::Pose;

  const Pose Shelf::POSE={0, 0, 1, 0, 0, 0};
  const double Shelf::BIN_HEIGHT=0.2;
  const double Shelf::BIN_WIDTH=0.2;
  const double Shelf::BIN_DEPTH=0.2;
  const double Shelf::SECURITY_DISTANCE=0.05;
  const Pose Shelf::BIN0={0, 0, 1, 0, 0, 0};

  Pose Shelf::getBinPose(int i, int j){
    Pose rel={0, -i*BIN_WIDTH, j*BIN_HEIGHT, 0, 0, 0};
    return Shelf::BIN0+rel;
  }

  Pose Shelf::getBinSafePose(int i, int j){
    Pose xx={-Shelf::BIN_DEPTH/2-Shelf::SECURITY_DISTANCE, 0, 0, 0, 0, 0};
    return getBinPose(i, j)+xx;
  }

}