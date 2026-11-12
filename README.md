# square_mission

ROS Noetic 的**正方形定点巡航**任务包 —— 完整、可直接放进任何 catkin 工作空间编译的独立 ROS 包。

```
等飞控连接 → 持续发 setpoint 并等 OFFBOARD+解锁 → 起飞到 altitude
→ 悬停 2 s → 依次飞各航点（正方形）→ 精确降落 → 退出
```

全程 20 Hz 发布 `/mavros/setpoint_raw/local`。

## 目录结构

```
square_mission/
├── package.xml                 依赖声明
├── CMakeLists.txt              add_executable(square_mission_node src/square.cpp)
├── config/
│   ├── square.yaml             仿真默认参数（4 点 / 0.5 m / if_debug 1）
│   └── square_hardware.yaml    实机参考参数（5 点 / 1.0 m / if_debug 0）
├── launch/
│   └── square.launch           启动文件（可用 param_file 覆盖参数文件）
└── src/
    └── square.cpp              任务节点
```

## 依赖

| 依赖 | 用途 |
|---|---|
| ROS Noetic | 运行环境 |
| `mavros_msgs` | 飞控消息类型 |
| **`template`** | 提供 `mission_pos_cruise()` / `precision_land()` / `hover()` 等函数（定义在 `template/include/template.h`） |

⚠️ **本包不包含 `template` 库**。`template` 是课程/战队工作空间里的基础库包，必须已经存在于同一个工作空间里，否则编译会报 `template.h: No such file or directory`。

## 安装

```bash
# 直接作为包放进工作空间（推荐）
cd ~/template2026_ws/src
git clone https://github.com/<你的账号>/square_mission.git
cd ~/template2026_ws && catkin_make -j2
```

## 运行

```bash
source ~/template2026_ws/devel/setup.bash

# 仿真（读包内 config/square.yaml）
roslaunch square_mission square.launch

# 实机（读包内实机参数）
roslaunch square_mission square.launch param_file:=/home/airhust/template2026_ws/src/square_mission/config/square_hardware.yaml
```

启动后会打印参数并停在 `1 to go on , else to quit`，输 `1` 继续。

## 参数

| 参数 | 含义 | 仿真 | 实机 |
|---|---|---|---|
| `altitude` | 相对起飞点的高度 [m] | 0.8 | 0.8 |
| `err_max` | 到点判定误差 [m] | 0.15 | 0.15 |
| `if_debug` | 1=程序自动切 OFFBOARD；0=飞手拨杆 | 1 | **0** |
| `stop_time` | 每点到点后额外悬停秒数 | 0.0 | 0.0 |
| `waypoints/i/x,y` | 第 i 个航点相对起飞点偏移 | 4 点 / 0.5 m | 5 点 / 1.0 m |

**改 YAML 不用重新编译**，重新 `roslaunch` 一次即生效。

## 设计要点

- **航点数量不写死**：`hasParam` 循环读，YAML 配 4 个飞 4 个、配 5 个飞 5 个
- **每段循环都 publish + spinOnce**：保证 setpoint 流不断，避免 OFFBOARD 掉模式
- **`if_debug == 0` 时只打印不强切**：实机上 OFFBOARD 由飞手负责
- **降落用 `precision_land()`**：锁落点、压 z 到 -0.15 保持 5 s，**但不自动上锁**

## 安全提示

- 实机起飞前必须确认定位正常，否则不允许切 OFFBOARD
- 飞机解锁或空中时，先让飞手接管，再动程序
- 落地后立刻手动上锁 / 断电

## 与「单文件仓库」的区别

| | 单文件仓库 | 本包 |
|---|---|---|
| 内容 | 只有 `square.cpp` | 完整 ROS 包（package.xml + CMakeLists + launch + config + src） |
| 使用方式 | 覆盖课程工作空间的 `src/mission/src/square.cpp` | 作为独立包 clone 进任何工作空间 |
| 是否改课程代码 | 会覆盖 | 不覆盖，与课程包并存 |
| 依赖 `template` | 是 | 是（仍需要） |

## License

MIT

## 参数命名空间（坑）

`launch/square.launch` 里的 `<rosparam command="load" .../>` 必须放在 **`<node>` 外面**（全局命名空间）。
如果放在 `<node>` 里面，参数会落到 `/square_mission/altitude`，而源码用 `nh.param("altitude", ...)`
读的是 `/altitude` —— 读不到时会退回到代码默认值，其中 `if_debug` 默认是 **1**（实机会自动切 OFFBOARD），很危险。
课程 `mission` 包的 `square.launch` 也是放在外面的，保持一致。

## 关于可执行文件名

本包的可执行文件叫 **`square_mission_node`**，不是 `square`。

原因：catkin 工作空间里所有包共用一套 CMake 目标名空间。课程仓库的 `mission` 包
已经有一个名为 `square` 的可执行目标，如果本包也叫 `square`，`catkin_make` 会直接报错：

```
add_executable cannot create target "square" because another target with the same name already exists
```

所以这里用 `square_mission_node`，与课程包并存不冲突。运行时节点名是 `square_mission`。
