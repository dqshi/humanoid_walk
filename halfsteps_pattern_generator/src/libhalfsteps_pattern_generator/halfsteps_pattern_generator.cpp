#include <stdexcept>
#include <boost/date_time.hpp>
#include <boost/math/constants/constants.hpp>

#include <Eigen/LU>

#include "halfsteps_pattern_generator.hh"
#include "newPGstepStudy.h"

#include <angles/angles.h>
#include <walk_interfaces/util.hh>
#include <walk_msgs/conversion.hh>
#include <walk_msgs/Footprint2d.h>

static const double g = 9.81;
static const double timeBeforeZmpShift = 0.95;
static const double timeAfterZmpShift = 1.05;
static const double halfStepLength = 2.;
static const double STEP = 0.005;


HalfStepsPatternGenerator::HalfStepsPatternGenerator()
  : halfStepsPgParent_t()
{}

HalfStepsPatternGenerator::HalfStepsPatternGenerator
(const HalfStepsPatternGenerator& pg)
  : halfStepsPgParent_t()
{}

HalfStepsPatternGenerator::~HalfStepsPatternGenerator()
{}

HalfStepsPatternGenerator&
HalfStepsPatternGenerator::operator= (const HalfStepsPatternGenerator& pg)
{
  if (&pg == this)
    return *this;
  return *this;
}

void
HalfStepsPatternGenerator::computeTrajectories()
{
  CnewPGstepStudy pg;
  StepFeatures stepFeatures;

  double comZ = initialCenterOfMassPosition()[2];

  Eigen::Matrix<double, 6, 1> initialStep;
  initialStep(0) =
    std::fabs(initialLeftFootPosition() (0, 3)
	      - initialRightFootPosition() (0, 3)) / 2.;
  initialStep(1) =
    std::fabs(initialLeftFootPosition() (1, 3)
	      - initialRightFootPosition() (1, 3)) / 2.;
  initialStep(2) = 0.;

  initialStep(3) = -initialStep (0);
  initialStep(4) = -initialStep (1);
  initialStep(5) = std::atan2(initialRightFootPosition() (1,0),
			      initialRightFootPosition() (0,0));

  std::vector<double> stepData;
  for (unsigned i = 0; i < 6; ++i)
    stepData.push_back(initialStep (i));

  // Footprints
  // Convert footprints into relative positions.
  walk::HomogeneousMatrix3d previousPosition;
  if (startWithLeftFoot ())
    previousPosition = initialRightFootPosition ();
  else
    previousPosition = initialLeftFootPosition ();

  walk::HomogeneousMatrix3d newPosition;
  walk::HomogeneousMatrix3d relativePosition;

  for (unsigned i = 0; i < this->footprints().size (); ++i)
    {
      walk::trans2dToTrans3d
	(newPosition,
	 this->footprints()[i].position[0],
	 this->footprints()[i].position[1],
	 this->footprints()[i].position[2]);

      relativePosition = newPosition * previousPosition.inverse ();

      walk_msgs::Footprint2d footprint;
      walk_msgs::convertHomogeneousMatrix3dToFootprint2d
	(footprint, relativePosition);
      stepData.push_back(this->footprints()[i].slideUp);
      stepData.push_back(this->footprints()[i].horizontalDistance);
      stepData.push_back(this->footprints()[i].stepHeight);
      stepData.push_back(this->footprints()[i].slideDown);
      stepData.push_back(footprint.x);
      stepData.push_back(footprint.y);
      stepData.push_back(angles::to_degrees (footprint.theta));

      previousPosition = newPosition;
    }

  pg.produceSeqSlidedHalfStepFeatures
    (stepFeatures, STEP, comZ, g,
     timeBeforeZmpShift, timeAfterZmpShift, halfStepLength, stepData,
     startWithLeftFoot() ? 'L' : 'R');

  getLeftFootTrajectory().data().resize(stepFeatures.size);
  getRightFootTrajectory().data().resize(stepFeatures.size);
  getCenterOfMassTrajectory().data().resize(stepFeatures.size);
  getZmpTrajectory().data().resize(stepFeatures.size);
  getPostureTrajectory().data().resize(stepFeatures.size);

  using boost::posix_time::milliseconds;

  for (unsigned i = 0; i < stepFeatures.size; ++i)
    {
      // Left foot.
      getLeftFootTrajectory().data()[i].duration = milliseconds(STEP * 1e3);
      getLeftFootTrajectory().data()[i].position.setIdentity();
      getLeftFootTrajectory().data()[i].position (0,3) =
	stepFeatures.leftfootXtraj[i];
      getLeftFootTrajectory().data()[i].position (1,3) =
	stepFeatures.leftfootYtraj[i];
      getLeftFootTrajectory().data()[i].position (2,3) =
	stepFeatures.leftfootHeight[i];

      double theta = stepFeatures.leftfootOrient[i] * M_PI / 180.;
      getLeftFootTrajectory().data()[i].position (0,0) = std::cos(theta);
      getLeftFootTrajectory().data()[i].position (0,1) = -std::sin(theta);
      getLeftFootTrajectory().data()[i].position (1,0) = std::sin(theta);
      getLeftFootTrajectory().data()[i].position (1,1) = std::cos(theta);

      // Right foot.
      getRightFootTrajectory().data()[i].duration = milliseconds(STEP * 1e3);
      getRightFootTrajectory().data()[i].position.setIdentity();
      getRightFootTrajectory().data()[i].position (0,3) =
	stepFeatures.rightfootXtraj[i];
      getRightFootTrajectory().data()[i].position (1,3) =
	stepFeatures.rightfootYtraj[i];
      getRightFootTrajectory().data()[i].position (2,3) =
	stepFeatures.rightfootHeight[i];

      theta = stepFeatures.rightfootOrient[i] * M_PI / 180.;
      getRightFootTrajectory().data()[i].position (0,0) = std::cos(theta);
      getRightFootTrajectory().data()[i].position (0,1) = -std::sin(theta);
      getRightFootTrajectory().data()[i].position (1,0) = std::sin(theta);
      getRightFootTrajectory().data()[i].position (1,1) = std::cos(theta);

      // Center of mass
      getCenterOfMassTrajectory().data()[i].duration = milliseconds(STEP * 1e3);
      getCenterOfMassTrajectory().data()[i].position[0] =
	stepFeatures.comTrajX[i];
      getCenterOfMassTrajectory().data()[i].position[1] =
	stepFeatures.comTrajY[i];
      getCenterOfMassTrajectory().data()[i].position[2] = comZ;

      // ZMP
      getZmpTrajectory().data()[i].duration = milliseconds(STEP * 1e3);
      getZmpTrajectory().data()[i].position.setIdentity();
      getZmpTrajectory().data()[i].position[0] = stepFeatures.zmpTrajX[i];
      getZmpTrajectory().data()[i].position[1] = stepFeatures.zmpTrajY[i];

      //Posture
      //FIXME:
    }
}
