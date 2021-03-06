#include "positionctl.h"

<<<<<<< HEAD
double YawFromQuaternion(const geometry_msgs::Quaternion &quat)
=======
double YawFromQuaternion(const tf::Quaternion& quat)
>>>>>>> 069bd59e1212a124a9ec0d3bcb34155af7c2a354
{
  tf::Matrix3x3 m_orient(quat);

  double roll, pitch, yaw;
  m_orient.getRPY(roll, pitch, yaw);

  return yaw;
}

double YawFromQuaternion(const geometry_msgs::Quaternion &quat) {
  tf::Quaternion quat_tf;
  tf::quaternionMsgToTF(quat, quat_tf);

  return YawFromQuaternion(quat_tf);
}

double AngularMinus(double a, double b)
{
  double res1 = a - b;
  double res2 = (a < b) ? (a + 2 * M_PI - b) : (a - 2 * M_PI - b);

  return (std::abs(res1) < std::abs(res2)) ? res1 : res2;
}

PositionCtl::PositionCtl()
{
  ros::NodeHandle node_priv;

  // Setup PID Paramters
  Kp_X = Max_X = A_X = 0;
  Kp_Y = Max_Y = A_Y = 0;
  Kp_W = Max_W = A_W = 0;

  // Setup Position Paramters
  X = Y = W = 0;
  Set_X = Set_Y = Set_W = 0;

  // Setup Reconfigurable Paramters
  static ros::NodeHandle DynamicParamNodeHandle("~/position");
  static dynamic_reconfigure::Server<rcbigctl::ControllerConfig> DynamicParamServer(DynamicParamNodeHandle);
  DynamicParamServer.setCallback(boost::bind(&PositionCtl::CallbackDynamicParam, this, _1, _2));

  // Setup Comm
  //pos_sub = node_priv.subscribe<nav_msgs::Odometry>("odom", 100, &PositionCtl::CallbackPosition, this);
  setpoint_sub = node_priv.subscribe<geometry_msgs::PoseStamped>("move_base_simple/goal", 100,
                                                                 &PositionCtl::CallbackSetpoint, this);
  //path_sub = node_priv.subscribe<nav_msgs::Path>("local_planner/path", 100, &PositionCtl::CallbackPath, this);
  twist_pub = node_priv.advertise<geometry_msgs::Twist>("velocity", 100);
}

PositionCtl::~PositionCtl()
{
}

void PositionCtl::update()
{
  UpdateCloseloop();
}

void PositionCtl::CallbackDynamicParam(rcbigctl::ControllerConfig &config, uint32_t level)
{
  ROS_INFO("Position Controller Reconfigure: [Kp_X = %lf, Max_X = %lf, A_X = %lf, Kp_Y = %lf, Max_Y = %lf, A_Y = %lf, "
           "Kp_W = %lf, Max_W = %lf, A_W = %lf]",
           config.Kp_X, config.Max_X, config.A_X, config.Kp_Y, config.Max_Y, config.A_Y, config.Kp_W, config.Max_W,
           config.A_W);

  Kp_X = config.Kp_X;
  Max_X = config.Max_X;
  A_X = config.A_X;

  Kp_Y = config.Kp_Y;
  Max_Y = config.Max_Y;
  A_Y = config.A_Y;

  Kp_W = config.Kp_W;
  Max_W = config.Max_W;
  A_W = config.A_W;
}

void PositionCtl::UpdateCloseloop()
{
  //lookup XYW From TF
  tf::StampedTransform tf_map_base;
  try {
    tf_listener.lookupTransform("map", "base", ros::Time(0), tf_map_base);
  }
  catch (tf::TransformException ex) {
    ROS_ERROR("%s",ex.what());
    return;
  }

  X = tf_map_base.getOrigin().getX();
  Y = tf_map_base.getOrigin().getY();
  W = YawFromQuaternion(tf_map_base.getRotation());

  //Closeloop Twist
  geometry_msgs::Twist twist;
  twist.linear.z = 0;
  twist.angular.x = 0;
  twist.angular.y = 0;

  geometry_msgs::Vector3Stamped vWorld;
  vWorld.header.stamp = ros::Time();
  vWorld.header.frame_id = "map";

  vWorld.vector.x = Kp_X * (Set_X - X);
  vWorld.vector.y = Kp_Y * (Set_Y - Y);
  vWorld.vector.z = 0;

  // Transform World Velocity To Robot Velocity
  geometry_msgs::Vector3Stamped vRobot;
  try
  {
    tf_listener.transformVector("base", vWorld, vRobot);
  }
  catch (tf::TransformException ex)
  {
    ROS_ERROR("%s", ex.what());
  }

  twist.linear.x = FoutX.filter(vRobot.vector.x, Max_X, A_X);
  twist.linear.y = FoutY.filter(vRobot.vector.y, Max_Y, A_Y);
  twist.angular.z = FoutW.filter(Kp_W * AngularMinus(Set_W, W), Max_W, A_W);

  twist_pub.publish(twist);
}

/* 
void PositionCtl::CallbackPosition(const nav_msgs::Odometry::ConstPtr& odom)
{
  X = odom->pose.pose.position.x;
  Y = odom->pose.pose.position.y;
  W = fmod(YawFromQuaternion(odom->pose.pose.orientation), 2 * M_PI);

  UpdateCloseloop();
}
*/
/*
void PositionCtl::setPoseSetpoint(const geometry_msgs::Pose& pose)
{
  Set_X = pose.position.x;
  Set_Y = pose.position.y;
  Set_W = fmod(YawFromQuaternion(pose.orientation), 2 * M_PI);
}
*/
void PositionCtl::CallbackSetpoint(const geometry_msgs::PoseStamped::ConstPtr& pose)
{
  Set_X = pose->pose.position.x;
  Set_Y = pose->pose.position.y;
  Set_W = fmod(YawFromQuaternion(pose->pose.orientation), 2 * M_PI);
}

/* 
void PositionCtl::CallbackPath(const nav_msgs::Path::ConstPtr& path)
{
  int index = std::min(10 - 1, (int)path->poses.size() - 1);
  setPoseSetpoint(path->poses[index].pose);
}
*/