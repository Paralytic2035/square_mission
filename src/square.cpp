// ============================================================
// square.cpp —— 正方形定点巡航任务（mission 包）
// 仿照 template2026_ws 的 mission 包写法，调用 template 包提供的库函数
//
// 流程：等飞控 → 发 setpoint 流并等 OFFBOARD+解锁 → 起飞到 altitude
//       → 悬停 2 s → 依次飞 4 个航点（正方形）→ 精确降落 → 退出
//
// 参数全部从 config/square.yaml 读：altitude / err_max / if_debug /
// stop_time / waypoints/i/x / waypoints/i/y
// ============================================================

#include <template.h>          // 库里提供：current_state / local_pos / setpoint_raw /
                               //           mission_pos_cruise() / precision_land() / hover()

#include <clocale>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

// ---------------- 可调参数（默认值，实际以 YAML 为准） ----------------
float altitude  = 0.8f;    // 相对起飞点的飞行高度 [m]
float err_max   = 0.15f;   // 到点判定误差 [m]
float if_debug  = 1.0f;    // 1=程序自动切 OFFBOARD；0=等飞手拨杆
float stop_time = 0.0f;    // 每到一个航点额外悬停的秒数（0=不停）

// ---------------- 航点容器 ----------------
struct Waypoint
{
  float x;
  float y;
};
std::vector<Waypoint> waypoints;

// ---------------- 打印本次生效的参数（现场核对用） ----------------
void print_param()
{
  std::cout << "=== 控制参数 ===" << std::endl;
  std::cout << "err_max  : " << err_max << std::endl;
  std::cout << "altitude : " << altitude << std::endl;
  std::cout << "if_debug : " << if_debug << std::endl;
  std::cout << "stop_time: " << stop_time << std::endl;
  std::cout << "waypoint 数量: " << waypoints.size() << std::endl;
  for (size_t i = 0; i < waypoints.size(); ++i)
  {
    std::cout << "waypoints/" << i << ": (" << waypoints[i].x
              << ", " << waypoints[i].y << ")" << std::endl;
  }
  if (if_debug == 1)
    std::cout << "自动offboard" << std::endl;
  else
    std::cout << "遥控器offboard" << std::endl;
}

int main(int argc, char **argv)
{
  // 防止中文输出乱码
  setlocale(LC_ALL, "");

  // 节点名必须和 CMakeLists 里的可执行文件名一致，否则 rosnode list 里对不上
  ros::init(argc, argv, "square");
  ros::NodeHandle nh;

  // ---------------- 订阅 & 发布 ----------------
  ros::Subscriber state_sub = nh.subscribe<mavros_msgs::State>(
      "mavros/state", 10, state_cb);
  ros::Subscriber local_pos_sub = nh.subscribe<nav_msgs::Odometry>(
      "/mavros/local_position/odom", 10, local_pos_cb);

  // 位置控制指令统一走 setpoint_raw/local（类型见 template.h 里 setpoint_raw）
  ros::Publisher setpoint_pub = nh.advertise<mavros_msgs::PositionTarget>(
      "/mavros/setpoint_raw/local", 100);

  // ---------------- 服务客户端：切模式、解锁 ----------------
  ros::ServiceClient arming_client =
      nh.serviceClient<mavros_msgs::CommandBool>("mavros/cmd/arming");
  ros::ServiceClient set_mode_client =
      nh.serviceClient<mavros_msgs::SetMode>("mavros/set_mode");

  // 20 Hz：飞控有 500 ms 心跳，setpoint 必须 > 2 Hz 持续推，OFFBOARD 才切得进去
  ros::Rate rate(20);

  // ---------------- 读参数 ----------------
  nh.param<float>("altitude", altitude, altitude);
  nh.param<float>("err_max", err_max, err_max);
  nh.param<float>("if_debug", if_debug, if_debug);
  nh.param<float>("stop_time", stop_time, stop_time);

  // 航点数量不写死：读到连续编号断了就停（4 个点、5 个点都能跑）
  for (int i = 0; i < 16; ++i)
  {
    const std::string prefix = "waypoints/" + std::to_string(i);
    if (!nh.hasParam(prefix + "/x"))
      break;
    Waypoint w{0.0f, 0.0f};
    nh.param<float>(prefix + "/x", w.x, 0.0f);
    nh.param<float>(prefix + "/y", w.y, 0.0f);
    waypoints.push_back(w);
  }

  // YAML 没配航点时给一套默认正方形，保证程序能跑
  if (waypoints.empty())
  {
    waypoints.push_back({0.0f, 0.0f});
    waypoints.push_back({0.5f, 0.0f});
    waypoints.push_back({0.5f, 0.5f});
    waypoints.push_back({0.0f, 0.5f});
  }

  print_param();

  // ---------------- 起飞前的人工确认（安全暂停点） ----------------
  int choice = 0;
  std::cout << "1 to go on , else to quit" << std::endl;
  std::cin >> choice;
  if (choice != 1)
  {
    std::cout << "已取消，未做任何动作。" << std::endl;
    return 0;
  }

  // ---------------- 等 MAVROS 连上飞控 ----------------
  ROS_INFO("等待 MAVROS 连接...");
  while (ros::ok() && !current_state.connected)
  {
    ros::spinOnce();
    rate.sleep();
  }
  ROS_INFO("MAVROS 已连接");

  mavros_msgs::SetMode offb_set_mode;
  offb_set_mode.request.custom_mode = "OFFBOARD";

  mavros_msgs::CommandBool arm_cmd;
  arm_cmd.request.value = true;

  ros::Time last_request = ros::Time::now();

  // ---------------- 阶段 1：持续发 setpoint，等 OFFBOARD + 解锁 ----------------
  // 注意：这里必须持续 publish，PX4 只有在收到稳定 setpoint 流之后才允许进 OFFBOARD
  while (ros::ok())
  {
    if (current_state.mode != "OFFBOARD")
    {
      if (if_debug == 1 && (ros::Time::now() - last_request > ros::Duration(3.0)))
      {
        if (set_mode_client.call(offb_set_mode) && offb_set_mode.response.mode_sent)
          ROS_INFO("已请求 OFFBOARD");
        last_request = ros::Time::now();
      }
    }
    else if (!current_state.armed)
    {
      if (ros::Time::now() - last_request > ros::Duration(3.0))
      {
        if (arming_client.call(arm_cmd) && arm_cmd.response.success)
          ROS_INFO("已请求解锁");
        last_request = ros::Time::now();
      }
    }
    else
    {
      break;   // 已经在 OFFBOARD 且已解锁
    }

    mission_pos_cruise(0, 0, altitude, 0, err_max);   // 先给"起飞点"当 setpoint
    setpoint_pub.publish(setpoint_raw);
    ros::spinOnce();
    rate.sleep();
  }
  ROS_INFO("已进入 OFFBOARD 且解锁，开始起飞");

  // ---------------- 阶段 2：爬升到 altitude ----------------
  while (ros::ok())
  {
    if (mission_pos_cruise(0, 0, altitude, 0, err_max))
      break;
    setpoint_pub.publish(setpoint_raw);
    ros::spinOnce();
    rate.sleep();
  }
  ROS_INFO("已到达起飞高度 %.2f m", altitude);

  // ---------------- 阶段 3：悬停 2 s 稳住姿态 ----------------
  while (ros::ok())
  {
    if (hover(2.0f))
      break;
    setpoint_pub.publish(setpoint_raw);
    ros::spinOnce();
    rate.sleep();
  }

  // ---------------- 阶段 4：依次飞各航点 ----------------
  for (size_t i = 0; i < waypoints.size() && ros::ok(); ++i)
  {
    ROS_INFO("飞向航点 %zu: (%.2f, %.2f)", i, waypoints[i].x, waypoints[i].y);

    while (ros::ok())
    {
      if (mission_pos_cruise(waypoints[i].x, waypoints[i].y, altitude, 0, err_max))
        break;
      setpoint_pub.publish(setpoint_raw);
      ros::spinOnce();
      rate.sleep();
    }

    ROS_INFO("到达航点 %zu", i);

    // 需要"每点停一下"就把 YAML 里 stop_time 设成非 0
    if (stop_time > 0.0f)
    {
      while (ros::ok())
      {
        if (hover(stop_time))
          break;
        setpoint_pub.publish(setpoint_raw);
        ros::spinOnce();
        rate.sleep();
      }
    }
  }

  // ---------------- 阶段 5：降落 ----------------
  ROS_INFO("航线完成，开始降落");
  while (ros::ok())
  {
    if (precision_land())
      break;
    setpoint_pub.publish(setpoint_raw);
    ros::spinOnce();
    rate.sleep();
  }

  ROS_INFO("任务结束。落地后请手动上锁（该降落函数不自动上锁）。");
  return 0;
}
